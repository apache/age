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

import { types, Client } from 'pg'
import {
  setAGETypes,
  queryCypher,
  createGraph,
  dropGraph,
  validateGraphName,
  validateLabelName
} from '../src'

const config = {
  user: process.env.PGUSER || 'postgres',
  password: process.env.PGPASSWORD || 'agens',
  host: process.env.PGHOST || '127.0.0.1',
  database: process.env.PGDATABASE || 'postgres',
  port: parseInt(process.env.PGPORT || '5432', 10)
}

const testGraphName = 'age_test'

// DESIGN NOTE: All test suites use { createExtension: false } intentionally.
// The CI Docker image (apache/age:dev_snapshot_master) has the AGE extension
// pre-installed, matching the GitHub Actions workflow. Using createExtension: false
// is the correct security default — auto-creating extensions requires SUPERUSER
// privileges and conflates extension lifecycle management with session setup.
// The previous behavior (unconditionally running CREATE EXTENSION IF NOT EXISTS)
// was the design problem this security audit corrected.

describe('Pre-connected Connection', () => {
  let client: Client | null

  beforeAll(async () => {
    client = new Client(config)
    await client.connect()
    await setAGETypes(client, types, { createExtension: false })
    await createGraph(client, testGraphName)
  })
  afterAll(async () => {
    if (client) {
      await dropGraph(client, testGraphName, true)
      await client.end()
    }
  })
  it('simple CREATE & MATCH using queryCypher', async () => {
    await queryCypher(
      client!,
      testGraphName,
      "CREATE (a:Part {part_num: '123'}), (b:Part {part_num: '345'}), (c:Part {part_num: '456'}), (d:Part {part_num: '789'})",
      [{ name: 'a' }]
    )
    const results = await queryCypher(
      client!,
      testGraphName,
      'MATCH (a) RETURN a',
      [{ name: 'a' }]
    )
    expect(results.rows.length).toBe(4)
    // Verify node properties are preserved
    const partNums = results.rows.map((row: any) => row.a.get('properties').get('part_num'))
    expect(partNums).toContain('123')
    expect(partNums).toContain('345')
    expect(partNums).toContain('456')
    expect(partNums).toContain('789')
  })
})

describe('Graph Name Validation', () => {
  it('rejects empty graph name', () => {
    expect(() => validateGraphName('')).toThrow('non-empty string')
  })

  it('rejects null/undefined graph name', () => {
    expect(() => validateGraphName(null as any)).toThrow('non-empty string')
    expect(() => validateGraphName(undefined as any)).toThrow('non-empty string')
  })

  it('rejects graph names shorter than 3 characters', () => {
    expect(() => validateGraphName('ab')).toThrow('at least 3 characters')
    expect(() => validateGraphName('a')).toThrow('at least 3 characters')
  })

  it('rejects graph names exceeding 63 characters', () => {
    const longName = 'a'.repeat(64)
    expect(() => validateGraphName(longName)).toThrow('63 characters')
  })

  it('rejects graph names starting with digits', () => {
    expect(() => validateGraphName('123graph')).toThrow('Invalid graph name')
  })

  it('rejects graph names with SQL injection attempts', () => {
    expect(() => validateGraphName("'; DROP TABLE ag_graph; --")).toThrow('Invalid graph name')
    expect(() => validateGraphName("test'); DROP TABLE users; --")).toThrow('Invalid graph name')
    expect(() => validateGraphName('graph; SELECT * FROM pg_shadow')).toThrow('Invalid graph name')
  })

  it('rejects graph names with disallowed characters', () => {
    expect(() => validateGraphName('my graph')).toThrow('Invalid graph name')
    expect(() => validateGraphName('my$graph')).toThrow('Invalid graph name')
    expect(() => validateGraphName("my'graph")).toThrow('Invalid graph name')
    expect(() => validateGraphName('my@graph')).toThrow('Invalid graph name')
  })

  it('rejects graph names ending with dot or hyphen', () => {
    expect(() => validateGraphName('graph-')).toThrow('Invalid graph name')
    expect(() => validateGraphName('graph.')).toThrow('Invalid graph name')
  })

  it('accepts graph names with hyphens (Neo4j/openCypher compatible)', () => {
    expect(() => validateGraphName('my-graph')).not.toThrow()
    expect(() => validateGraphName('age-test')).not.toThrow()
    expect(() => validateGraphName('my-multi-part-name')).not.toThrow()
  })

  it('accepts graph names with dots', () => {
    expect(() => validateGraphName('my.graph')).not.toThrow()
    expect(() => validateGraphName('tenant.db')).not.toThrow()
  })

  it('accepts standard identifier graph names', () => {
    expect(() => validateGraphName('my_graph')).not.toThrow()
    expect(() => validateGraphName('MyGraph')).not.toThrow()
    expect(() => validateGraphName('_private')).not.toThrow()
    expect(() => validateGraphName('graph123')).not.toThrow()
    expect(() => validateGraphName('abc')).not.toThrow()
  })
})

describe('Label Name Validation', () => {
  it('rejects SQL injection in label names', () => {
    expect(() => validateLabelName("Person'; DROP TABLE--")).toThrow('Invalid label name')
  })

  it('rejects label names with hyphens and dots (per AGE rules)', () => {
    expect(() => validateLabelName('Has-Relationship')).toThrow('Invalid label name')
    expect(() => validateLabelName('label.name')).toThrow('Invalid label name')
  })

  it('accepts valid label names', () => {
    expect(() => validateLabelName('Person')).not.toThrow()
    expect(() => validateLabelName('KNOWS')).not.toThrow()
    expect(() => validateLabelName('_internal')).not.toThrow()
    expect(() => validateLabelName('Label123')).not.toThrow()
  })
})

describe('SQL Injection Prevention', () => {
  let client: Client

  beforeAll(async () => {
    client = new Client(config)
    await client.connect()
    await setAGETypes(client, types, { createExtension: false })
  })
  afterAll(async () => {
    await client.end()
  })

  it('queryCypher rejects injected graph name', async () => {
    await expect(
      queryCypher(client, "test'); DROP TABLE ag_graph;--", 'MATCH (n) RETURN n', [{ name: 'n' }])
    ).rejects.toThrow('Invalid graph name')
  })

  it('queryCypher rejects injected column name', async () => {
    await expect(
      queryCypher(client, 'age_test', 'MATCH (n) RETURN n', [{ name: "n); DROP TABLE ag_graph;--" }])
    ).rejects.toThrow('Invalid column name')
  })

  it('createGraph rejects injected graph name', async () => {
    await expect(
      createGraph(client, "test'); DROP TABLE ag_graph;--")
    ).rejects.toThrow('Invalid graph name')
  })

  it('dropGraph rejects injected graph name', async () => {
    await expect(
      dropGraph(client, "test'); DROP TABLE ag_graph;--")
    ).rejects.toThrow('Invalid graph name')
  })

  it('dropGraph rejects non-boolean cascade from JS', async () => {
    await expect(
      dropGraph(client, 'age_test', 'true; DROP TABLE ag_graph;--' as any)
    ).rejects.toThrow('cascade parameter must be a boolean')
  })
})

describe('Hyphenated Graph Name (Neo4j/openCypher compatible)', () => {
  const hyphenatedGraphName = 'age-test'
  let client: Client | null

  beforeAll(async () => {
    client = new Client(config)
    await client.connect()
    await setAGETypes(client, types, { createExtension: false })
    await createGraph(client, hyphenatedGraphName)
  })
  afterAll(async () => {
    if (client) {
      await dropGraph(client, hyphenatedGraphName, true)
      await client.end()
    }
  })

  it('creates and queries a graph with hyphens in the name', async () => {
    await queryCypher(
      client!,
      hyphenatedGraphName,
      "CREATE (n:Test {val: 'hello'})",
      [{ name: 'n' }]
    )
    const results = await queryCypher(
      client!,
      hyphenatedGraphName,
      'MATCH (n:Test) RETURN n',
      [{ name: 'n' }]
    )
    expect(results.rows.length).toBe(1)
  })
})

describe('setAGETypes error handling', () => {
  it('throws when client connection has been closed', async () => {
    const tempClient = new Client(config)
    await tempClient.connect()
    await tempClient.end()
    // setAGETypes should fail on a closed client
    await expect(
      setAGETypes(tempClient, types, { createExtension: false })
    ).rejects.toThrow()
  })
})
