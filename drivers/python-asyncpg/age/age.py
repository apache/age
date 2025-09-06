# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import re
import asyncio
import asyncpg
import json
from .exceptions import *
from .builder import parseAgeValue


_EXCEPTION_NoConnection = NoConnection()
_EXCEPTION_GraphNotSet = GraphNotSet()

WHITESPACE = re.compile(r'\s')

def _validate_graph_name(graphName: str):
    if not re.match(r'^[a-zA-Z0-9_]+$', graphName):
        raise ValueError(f"Invalid graph name: {graphName}")

def agtype_decoder(data):
    return parseAgeValue(data)

def agtype_encoder(value):
    return str(value)


async def setUpAge(conn: asyncpg.Connection, graphName: str, load_from_plugins: bool = False):
    if graphName:
        _validate_graph_name(graphName)

    if load_from_plugins:
        await conn.execute("LOAD '$libdir/plugins/age';")
    else:
        await conn.execute("LOAD 'age';")

    await conn.execute("SET search_path = ag_catalog, '$user', public;")

    # We need to fetch the OID for the 'agtype' type
    agtype_oid = await conn.fetchval("SELECT oid FROM pg_type WHERE typname = 'agtype'")

    if not agtype_oid:
        raise AgeNotSet("agtype not found. Make sure the AGE extension is properly installed.")

    await conn.set_type_codec(
        'agtype',
        encoder=agtype_encoder,
        decoder=agtype_decoder,
        schema='pg_catalog',
        format='text'
    )

    # Check graph exists
    if graphName is not None:
        await checkGraphCreated(conn, graphName)


async def checkGraphCreated(conn: asyncpg.Connection, graphName: str):
    _validate_graph_name(graphName)
    # Use parameters to avoid SQL injection, even with validation.
    # asyncpg doesn't support identifiers as parameters, so we have to be careful.
    # The validation helps, but this is an area for future improvement.
    count = await conn.fetchval(f"SELECT count(*) FROM ag_graph WHERE name = $1", graphName)
    if count == 0:
        await conn.execute(f"SELECT create_graph($1);", graphName)


async def deleteGraph(conn: asyncpg.Connection, graphName: str):
    _validate_graph_name(graphName)
    await conn.execute(f"SELECT drop_graph($1, true);", graphName)


async def execCypher(conn: asyncpg.Connection, graphName: str, cypherStmt: str, cols: list = None, params: dict = None) -> list[asyncpg.Record]:
    if conn is None or conn.is_closed():
        raise _EXCEPTION_NoConnection

    if graphName is None:
        raise _EXCEPTION_GraphNotSet

    _validate_graph_name(graphName)

    columnExp=[]
    if cols is not None and len(cols) > 0:
        for col in cols:
            col_strip = col.strip()
            if col_strip == '':
                continue
            # Further validation for column names
            if not re.match(r'^[a-zA-Z0-9_]+$', col_strip.split()[0]):
                 raise ValueError(f"Invalid column name: {col}")

            if WHITESPACE.search(col_strip) is not None:
                columnExp.append(col_strip)
            else:
                columnExp.append(col_strip + " agtype")
    else:
        columnExp.append('v agtype')

    params_agtype = None
    if params:
        # The cypher function expects an agtype map for parameters.
        # We can represent this as a JSON string.
        params_agtype = json.dumps(params)

    # The graph name is validated, but it's still not ideal to use f-string.
    # This is a limitation of asyncpg not supporting identifiers as parameters.
    query = f"SELECT * FROM cypher('{graphName}', $1, $2) AS ({','.join(columnExp)})"

    try:
        return await conn.fetch(query, cypherStmt, params_agtype)
    except Exception as cause:
        raise SqlExecutionError(f"Execution ERR[{cause}]({query})", cause)


class Age:
    def __init__(self):
        self.connection: asyncpg.Connection | None = None
        self.graphName: str | None = None

    async def connect(self, graph: str = None, dsn: str = None, load_from_plugins: bool = False, **kwargs):
        self.connection = await asyncpg.connect(dsn=dsn, **kwargs)
        await setUpAge(self.connection, graph, load_from_plugins)
        self.graphName = graph
        return self

    async def close(self):
        if self.connection and not self.connection.is_closed():
            await self.connection.close()

    async def setGraph(self, graph: str):
        await checkGraphCreated(self.connection, graph)
        self.graphName = graph
        return self

    async def execCypher(self, cypherStmt: str, cols: list = None, params: dict = None):
        return await execCypher(self.connection, self.graphName, cypherStmt, cols=cols, params=params)
