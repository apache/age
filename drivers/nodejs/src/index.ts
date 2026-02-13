/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

import { Client, QueryResult, QueryResultRow } from 'pg'
import pgTypes from 'pg-types'
import { CharStreams, CommonTokenStream } from 'antlr4ts'
import { AgtypeLexer } from './antlr4/AgtypeLexer'
import { AgtypeParser } from './antlr4/AgtypeParser'
import CustomAgTypeListener from './antlr4/CustomAgTypeListener'
import { ParseTreeWalker } from 'antlr4ts/tree'

/**
 * Valid graph name pattern based on Apache AGE's naming conventions
 * (Neo4j/openCypher compatible). Allows ASCII letters, digits,
 * underscores, plus dots and hyphens in middle positions.
 *
 * DESIGN NOTE: AGE's server-side validation (name_validation.h) uses
 * full Unicode ID_Start/ID_Continue character classes, accepting names
 * like 'mydätabase' or 'mydঅtabase'. This driver intentionally restricts
 * to ASCII-only as a security hardening measure — it reduces the attack
 * surface for homoglyph and encoding-based injection vectors. Server-side
 * validation remains the authoritative check for Unicode names.
 *
 * Start: ASCII letter or underscore
 * Middle: ASCII letter, digit, underscore, dot, or hyphen
 * End: ASCII letter, digit, or underscore
 */
const VALID_GRAPH_NAME = /^[A-Za-z_][A-Za-z0-9_.\-]*[A-Za-z0-9_]$/

/**
 * Valid label name pattern based on Apache AGE's naming rules.
 * Labels follow stricter identifier rules than graph names — dots and
 * hyphens are NOT permitted in label names.
 *
 * Note: ASCII-only restriction (see VALID_GRAPH_NAME design note above).
 */
const VALID_LABEL_NAME = /^[A-Za-z_][A-Za-z0-9_]*$/

/**
 * Valid SQL identifier pattern for column names and types in the
 * query result AS clause. Standard PostgreSQL unquoted identifier rules.
 *
 * Note: ASCII-only restriction (see VALID_GRAPH_NAME design note above).
 */
const VALID_SQL_IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/

/**
 * Validates that a graph name conforms to Apache AGE's naming conventions
 * and is safe for use in SQL queries.
 *
 * Graph names must:
 * - Be at least 3 characters and at most 63 characters
 * - Start with an ASCII letter or underscore
 * - Contain only ASCII letters, digits, underscores, dots, and hyphens
 * - End with an ASCII letter, digit, or underscore
 *
 * Note: This is intentionally stricter than AGE's server-side validation
 * (which accepts Unicode letters). See VALID_GRAPH_NAME design note.
 *
 * @param graphName - The graph name to validate
 * @throws Error if the graph name is invalid
 */
function validateGraphName (graphName: string): void {
  if (!graphName || typeof graphName !== 'string') {
    throw new Error('Graph name must be a non-empty string')
  }
  if (graphName.length < 3) {
    throw new Error(
      `Invalid graph name: '${graphName}'. Graph names must be at least 3 characters.`
    )
  }
  if (graphName.length > 63) {
    throw new Error('Graph name must not exceed 63 characters (PostgreSQL name limit)')
  }
  if (!VALID_GRAPH_NAME.test(graphName)) {
    throw new Error(
      `Invalid graph name: '${graphName}'. Graph names must start with a letter ` +
      'or underscore, may contain letters, digits, underscores, dots, and hyphens, ' +
      'and must end with a letter, digit, or underscore.'
    )
  }
}

/**
 * Validates that a label name conforms to Apache AGE's naming rules.
 * Label names are stricter than graph names — only letters, digits,
 * and underscores are permitted (no dots or hyphens).
 *
 * @param labelName - The label name to validate
 * @throws Error if the label name is invalid
 */
function validateLabelName (labelName: string): void {
  if (!labelName || typeof labelName !== 'string') {
    throw new Error('Label name must be a non-empty string')
  }
  if (labelName.length > 63) {
    throw new Error('Label name must not exceed 63 characters (PostgreSQL name limit)')
  }
  if (!VALID_LABEL_NAME.test(labelName)) {
    throw new Error(
      `Invalid label name: '${labelName}'. Label names must start with a letter ` +
      'or underscore and contain only letters, digits, and underscores.'
    )
  }
}

/**
 * Escapes a PostgreSQL dollar-quoted string literal by ensuring the
 * cypher query does not contain the dollar-quote delimiter. If the
 * default $$ delimiter conflicts, a unique tagged delimiter is used.
 *
 * @param cypher - The Cypher query string
 * @returns An object with the delimiter and safely quoted string
 */
function cypherDollarQuote (cypher: string): { delimiter: string; quoted: string } {
  if (!cypher.includes('$$')) {
    return { delimiter: '$$', quoted: `$$${cypher}$$` }
  }
  // Generate a unique tag that doesn't appear in the cypher query
  let tag = 'cypher'
  let counter = 0
  while (cypher.includes(`$${tag}$`)) {
    tag = `cypher${counter++}`
  }
  return { delimiter: `$${tag}$`, quoted: `$${tag}$${cypher}$${tag}$` }
}

function AGTypeParse (input: string) {
  const chars = CharStreams.fromString(input)
  const lexer = new AgtypeLexer(chars)
  const tokens = new CommonTokenStream(lexer)
  const parser = new AgtypeParser(tokens)
  const tree = parser.agType()
  const printer = new CustomAgTypeListener()
  ParseTreeWalker.DEFAULT.walk(printer, tree)
  return printer.getResult()
}

/**
 * Options for setAGETypes configuration.
 */
interface SetAGETypesOptions {
  /**
   * If true, will attempt to CREATE EXTENSION IF NOT EXISTS age.
   * Defaults to false. Set to true only if the connected user has
   * sufficient privileges.
   */
  createExtension?: boolean
}

/**
 * Configures the pg client to properly parse AGE agtype results.
 *
 * This function:
 * 1. Loads the AGE extension into the session
 * 2. Sets the search_path to include ag_catalog
 * 3. Registers the agtype type parser
 *
 * @param client - A connected pg Client instance
 * @param types - The pg types module for registering type parsers
 * @param options - Optional configuration settings
 * @throws Error if AGE extension is not installed or agtype is not found
 */
async function setAGETypes (client: Client, types: typeof pgTypes, options?: SetAGETypesOptions) {
  const createExtension = options?.createExtension ?? false

  if (createExtension) {
    await client.query('CREATE EXTENSION IF NOT EXISTS age;')
  }

  try {
    await client.query("LOAD 'age';")
    await client.query('SET search_path = ag_catalog, "$user", public;')
  } catch (err: unknown) {
    const msg = err instanceof Error ? err.message : String(err)
    throw new Error(
      `Failed to load AGE extension: ${msg}. ` +
      'Ensure the AGE extension is installed in the database. ' +
      'You may need to run CREATE EXTENSION age; first, or pass ' +
      '{ createExtension: true } to setAGETypes().'
    )
  }

  const oidResults = await client.query(
    "SELECT typelem FROM pg_type WHERE typname = '_agtype';"
  )

  if (oidResults.rows.length < 1) {
    throw new Error(
      'AGE agtype type not found. Ensure the AGE extension is installed ' +
      'and loaded in the current database. Run CREATE EXTENSION age; first, ' +
      'or pass { createExtension: true } to setAGETypes().'
    )
  }

  types.setTypeParser(oidResults.rows[0].typelem, AGTypeParse)
}

/**
 * Column definition for Cypher query results.
 */
interface CypherColumn {
  /** Column alias name */
  name: string
  /** Column type (defaults to 'agtype') */
  type?: string
}

/**
 * Executes a Cypher query safely against an AGE graph.
 *
 * This function validates the graph name to prevent SQL injection,
 * properly quotes the Cypher query using dollar-quoting, and builds
 * the required SQL wrapper.
 *
 * @param client - A connected pg Client instance (with AGE types set)
 * @param graphName - The target graph name (must be a valid AGE graph name)
 * @param cypher - The Cypher query string
 * @param columns - Column definitions for the result set
 * @returns The query result
 * @throws Error if graphName is invalid or query fails
 *
 * @example
 * ```typescript
 * const result = await queryCypher(
 *   client,
 *   'my_graph',
 *   'MATCH (n:Person) WHERE n.name = $name RETURN n',
 *   [{ name: 'n' }]
 * );
 * ```
 */
async function queryCypher<T extends QueryResultRow = any> (
  client: Client,
  graphName: string,
  cypher: string,
  columns: CypherColumn[] = [{ name: 'result' }]
): Promise<QueryResult<T>> {
  // Validate graph name against injection
  validateGraphName(graphName)

  // Validate column definitions
  if (!columns || columns.length === 0) {
    throw new Error('At least one column definition is required')
  }

  for (const col of columns) {
    if (!col.name || typeof col.name !== 'string') {
      throw new Error('Column name must be a non-empty string')
    }
    // Column names must be valid SQL identifiers
    if (!VALID_SQL_IDENTIFIER.test(col.name)) {
      throw new Error(
        `Invalid column name: '${col.name}'. Column names must be valid SQL identifiers.`
      )
    }
    if (col.type && !VALID_SQL_IDENTIFIER.test(col.type)) {
      throw new Error(
        `Invalid column type: '${col.type}'. Column types must be valid SQL type identifiers.`
      )
    }
  }

  // Build column list safely
  const columnList = columns
    .map(col => `${col.name} ${col.type ?? 'agtype'}`)
    .join(', ')

  // Safely dollar-quote the cypher query
  const { quoted } = cypherDollarQuote(cypher)

  const sql = `SELECT * FROM cypher('${graphName}', ${quoted}) AS (${columnList});`

  return client.query<T>(sql)
}

/**
 * Creates a new graph safely.
 *
 * @param client - A connected pg Client instance
 * @param graphName - Name for the new graph (must be a valid AGE graph name)
 * @throws Error if graphName is invalid
 */
async function createGraph (client: Client, graphName: string): Promise<void> {
  validateGraphName(graphName)
  await client.query(`SELECT * FROM ag_catalog.create_graph('${graphName}');`)
}

/**
 * Drops an existing graph safely.
 *
 * @param client - A connected pg Client instance
 * @param graphName - Name of the graph to drop (must be a valid AGE graph name)
 * @param cascade - If true, drop dependent objects (default: false)
 * @throws Error if graphName is invalid
 */
async function dropGraph (client: Client, graphName: string, cascade: boolean = false): Promise<void> {
  validateGraphName(graphName)
  if (typeof cascade !== 'boolean') {
    throw new Error('cascade parameter must be a boolean')
  }
  await client.query(`SELECT * FROM ag_catalog.drop_graph('${graphName}', ${cascade});`)
}

export {
  setAGETypes,
  AGTypeParse,
  queryCypher,
  createGraph,
  dropGraph,
  validateGraphName,
  validateLabelName,
  SetAGETypesOptions,
  CypherColumn
}
