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
import psycopg
from psycopg.types import TypeInfo
from psycopg import sql
from psycopg.client_cursor import ClientCursor
from .exceptions import *
from .builder import parseAgeValue


_EXCEPTION_NoConnection = NoConnection()
_EXCEPTION_GraphNotSet = GraphNotSet()

WHITESPACE = re.compile(r'\s')

# Valid AGE graph name pattern aligned with Apache AGE's internal validation
# and Neo4j/openCypher naming conventions.
# Start: letter or underscore
# Middle: letter, digit, underscore, dot, or hyphen
# End: letter, digit, or underscore
#
# Design note: The middle segment uses `*` (not `+`) intentionally.
# This makes the regex match names as short as 2 characters at the
# regex level. However, validate_graph_name() checks MIN_GRAPH_NAME_LENGTH
# *before* applying this regex, so 2-character names are rejected with a
# clear "must be at least 3 characters" error rather than a confusing
# regex-mismatch error. This ordering gives users actionable feedback.
VALID_GRAPH_NAME = re.compile(r'^[A-Za-z_][A-Za-z0-9_.\-]*[A-Za-z0-9_]$')
MIN_GRAPH_NAME_LENGTH = 3

# Valid SQL identifier for labels, column names, and types.
# Stricter than graph names — no dots or hyphens.
VALID_IDENTIFIER = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')
MAX_IDENTIFIER_LENGTH = 63


def validate_graph_name(graph_name: str) -> None:
    """Validate that a graph name conforms to Apache AGE's naming rules.

    Graph names must:
    - Be at least 3 characters and at most 63 characters
    - Start with a letter or underscore
    - Contain only letters, digits, underscores, dots, and hyphens
    - End with a letter, digit, or underscore

    This aligns with AGE's internal validation and Neo4j/openCypher
    naming conventions.

    Args:
        graph_name: The graph name to validate.

    Raises:
        InvalidGraphName: If the graph name is invalid.
    """
    if not graph_name or not isinstance(graph_name, str):
        raise InvalidGraphName(
            str(graph_name),
            "Graph name must be a non-empty string."
        )
    if len(graph_name) < MIN_GRAPH_NAME_LENGTH:
        raise InvalidGraphName(
            graph_name,
            f"Graph names must be at least {MIN_GRAPH_NAME_LENGTH} characters."
        )
    if len(graph_name) > MAX_IDENTIFIER_LENGTH:
        raise InvalidGraphName(
            graph_name,
            f"Must not exceed {MAX_IDENTIFIER_LENGTH} characters "
            "(PostgreSQL name limit)."
        )
    if not VALID_GRAPH_NAME.match(graph_name):
        raise InvalidGraphName(
            graph_name,
            "Graph names must start with a letter or underscore, "
            "may contain letters, digits, underscores, dots, and hyphens, "
            "and must end with a letter, digit, or underscore."
        )


def validate_identifier(name: str, context: str = "identifier") -> None:
    """Validate that a name is a safe SQL identifier for labels, columns, or types.

    This follows stricter rules than graph names — only letters, digits,
    and underscores are permitted (no dots or hyphens).

    Args:
        name: The identifier to validate.
        context: What the identifier represents (for error messages).

    Raises:
        InvalidIdentifier: If the identifier is invalid.
    """
    if not name or not isinstance(name, str):
        raise InvalidIdentifier(
            str(name),
            f"{context} must be a non-empty string."
        )
    if len(name) > MAX_IDENTIFIER_LENGTH:
        raise InvalidIdentifier(
            name,
            f"{context} must not exceed {MAX_IDENTIFIER_LENGTH} characters."
        )
    if not VALID_IDENTIFIER.match(name):
        raise InvalidIdentifier(
            name,
            f"{context} must start with a letter or underscore "
            "and contain only letters, digits, and underscores."
        )


class AgeDumper(psycopg.adapt.Dumper):
    def dump(self, obj: Any) -> bytes | bytearray | memoryview:
        pass


class AgeLoader(psycopg.adapt.Loader):
    def load(self, data: bytes | bytearray | memoryview) -> Any | None:
        if isinstance(data, memoryview):
            data_bytes = data.tobytes()
        else:
            data_bytes = data

        return parseAgeValue(data_bytes.decode('utf-8'))


def setUpAge(conn:psycopg.connection, graphName:str, load_from_plugins:bool=False):
    with conn.cursor() as cursor:
        if load_from_plugins:
            cursor.execute("LOAD '$libdir/plugins/age';")
        else:
            cursor.execute("LOAD 'age';")

        cursor.execute("SET search_path = ag_catalog, '$user', public;")

        ag_info = TypeInfo.fetch(conn, 'agtype')

        if not ag_info:
            raise AgeNotSet(
                "AGE agtype type not found. Ensure the AGE extension is "
                "installed and loaded in the current database. "
                "Run CREATE EXTENSION age; first."
            )

        conn.adapters.register_loader(ag_info.oid, AgeLoader)
        conn.adapters.register_loader(ag_info.array_oid, AgeLoader)

        # Check graph exists
        if graphName != None:
            checkGraphCreated(conn, graphName)

# Create the graph, if it does not exist
def checkGraphCreated(conn:psycopg.connection, graphName:str):
    validate_graph_name(graphName)
    with conn.cursor() as cursor:
        cursor.execute(sql.SQL("SELECT count(*) FROM ag_graph WHERE name={graphName}").format(graphName=sql.Literal(graphName)))
        if cursor.fetchone()[0] == 0:
            cursor.execute(sql.SQL("SELECT create_graph({graphName});").format(graphName=sql.Literal(graphName)))
            conn.commit()


def deleteGraph(conn:psycopg.connection, graphName:str):
    validate_graph_name(graphName)
    with conn.cursor() as cursor:
        cursor.execute(sql.SQL("SELECT drop_graph({graphName}, true);").format(graphName=sql.Literal(graphName)))
        conn.commit()


def _validate_column(col: str) -> str:
    """Validate and normalize a column specification for use in SQL.

    Accepts either a plain column name (e.g. 'v') or a name with type
    (e.g. 'v agtype'). Validates each component to prevent SQL injection.

    Args:
        col: Column specification string.

    Returns:
        Normalized column specification, or empty string if blank.

    Raises:
        InvalidIdentifier: If any component is invalid.
    """
    col = col.strip()
    if not col:
        return ''

    if WHITESPACE.search(col):
        parts = col.split()
        if len(parts) != 2:
            raise InvalidIdentifier(
                col,
                "Column specification must be 'name' or 'name type'."
            )
        name, type_name = parts
        validate_identifier(name, "Column name")
        validate_identifier(type_name, "Column type")
        return f"{name} {type_name}"
    else:
        validate_identifier(col, "Column name")
        return f"{col} agtype"


def buildCypher(graphName:str, cypherStmt:str, columns:list) ->str:
    if graphName == None:
        raise _EXCEPTION_GraphNotSet

    columnExp=[]
    if columns != None and len(columns) > 0:
        for col in columns:
            validated = _validate_column(col)
            if validated:
                columnExp.append(validated)
    else:
        columnExp.append('v agtype')

    # Design note: String concatenation is used here instead of
    # psycopg.sql.Identifier() because column specifications are
    # "name type" pairs (e.g. "v agtype") that don't map directly to
    # sql.Identifier(). Each component has already been validated by
    # _validate_column() → validate_identifier(), which restricts
    # names to ^[A-Za-z_][A-Za-z0-9_]*$ and max 63 chars. The
    # graphName and cypherStmt are NOT embedded here — this template
    # only contains the validated column list and static SQL keywords.
    stmtArr = []
    stmtArr.append("SELECT * from cypher(NULL,NULL) as (")
    stmtArr.append(','.join(columnExp))
    stmtArr.append(");")
    return "".join(stmtArr)

def execSql(conn:psycopg.connection, stmt:str, commit:bool=False, params:tuple=None) -> psycopg.cursor :
    if conn == None or conn.closed:
        raise _EXCEPTION_NoConnection
    
    cursor = conn.cursor()
    try:
        cursor.execute(stmt, params)
        if commit:
            conn.commit()

        return cursor
    except SyntaxError as cause:
        conn.rollback()
        raise cause
    except Exception as cause:
        conn.rollback()
        raise SqlExecutionError("Execution ERR[" + str(cause) +"](" + stmt +")", cause)


def querySql(conn:psycopg.connection, stmt:str, params:tuple=None) -> psycopg.cursor :
    return execSql(conn, stmt, False, params)

# Execute cypher statement and return cursor.
# If cypher statement changes data (create, set, remove),
# You must commit session(ag.commit())
# (Otherwise the execution cannot make any effect.)
def execCypher(conn:psycopg.connection, graphName:str, cypherStmt:str, cols:list=None, params:tuple=None) -> psycopg.cursor :
    if conn == None or conn.closed:
        raise _EXCEPTION_NoConnection

    cursor = conn.cursor()
    #clean up the string for mogrification
    cypherStmt = cypherStmt.replace("\n", "")
    cypherStmt = cypherStmt.replace("\t", "")
    cypher = str(cursor.mogrify(cypherStmt, params))
    cypher = cypher.strip()

    preparedStmt = "SELECT * FROM age_prepare_cypher({graphName},{cypherStmt})"

    cursor = conn.cursor()
    try:
        cursor.execute(sql.SQL(preparedStmt).format(graphName=sql.Literal(graphName),cypherStmt=sql.Literal(cypher)))
    except SyntaxError as cause:
        conn.rollback()
        raise cause
    except Exception as cause:
        conn.rollback()
        raise SqlExecutionError("Execution ERR[" + str(cause) +"](" + preparedStmt +")", cause)

    stmt = buildCypher(graphName, cypher, cols)

    cursor = conn.cursor()
    try:
        cursor.execute(stmt)
        return cursor
    except SyntaxError as cause:
        conn.rollback()
        raise cause
    except Exception as cause:
        conn.rollback()
        raise SqlExecutionError("Execution ERR[" + str(cause) +"](" + stmt +")", cause)


def cypher(cursor:psycopg.cursor, graphName:str, cypherStmt:str, cols:list=None, params:tuple=None) -> psycopg.cursor :
    #clean up the string for mogrification
    cypherStmt = cypherStmt.replace("\n", "")
    cypherStmt = cypherStmt.replace("\t", "")
    cypher = str(cursor.mogrify(cypherStmt, params))
    cypher = cypher.strip()

    preparedStmt = "SELECT * FROM age_prepare_cypher({graphName},{cypherStmt})"
    cursor.execute(sql.SQL(preparedStmt).format(graphName=sql.Literal(graphName),cypherStmt=sql.Literal(cypher)))

    stmt = buildCypher(graphName, cypher, cols)
    cursor.execute(stmt)


# def execCypherWithReturn(conn:psycopg.connection, graphName:str, cypherStmt:str, columns:list=None , params:tuple=None) -> psycopg.cursor :
#     stmt = buildCypher(graphName, cypherStmt, columns)
#     return execSql(conn, stmt, False, params)

# def queryCypher(conn:psycopg.connection, graphName:str, cypherStmt:str, columns:list=None , params:tuple=None) -> psycopg.cursor :
#     return execCypherWithReturn(conn, graphName, cypherStmt, columns, params)


class Age:
    def __init__(self):
        self.connection = None    # psycopg connection]
        self.graphName = None

    # Connect to PostgreSQL Server and establish session and type extension environment.
    def connect(self, graph:str=None, dsn:str=None, connection_factory=None, cursor_factory=ClientCursor,
                load_from_plugins:bool=False, **kwargs):
        conn = psycopg.connect(dsn, cursor_factory=cursor_factory, **kwargs)
        setUpAge(conn, graph, load_from_plugins)
        self.connection = conn
        self.graphName = graph
        return self

    def close(self):
        self.connection.close()

    def setGraph(self, graph:str):
        checkGraphCreated(self.connection, graph)
        self.graphName = graph
        return self

    def commit(self):
        self.connection.commit()

    def rollback(self):
        self.connection.rollback()

    def execCypher(self, cypherStmt:str, cols:list=None, params:tuple=None) -> psycopg.cursor :
        return execCypher(self.connection, self.graphName, cypherStmt, cols=cols, params=params)

    def cypher(self, cursor:psycopg.cursor, cypherStmt:str, cols:list=None, params:tuple=None) -> psycopg.cursor :
        return cypher(cursor, self.graphName, cypherStmt, cols=cols, params=params)

    # def execSql(self, stmt:str, commit:bool=False, params:tuple=None) -> psycopg.cursor :
    #     return execSql(self.connection, stmt, commit, params)


    # def execCypher(self, cypherStmt:str, commit:bool=False, params:tuple=None) -> psycopg.cursor :
    #     return execCypher(self.connection, self.graphName, cypherStmt, commit, params)

    # def execCypherWithReturn(self, cypherStmt:str, columns:list=None , params:tuple=None) -> psycopg.cursor :
    #     return execCypherWithReturn(self.connection, self.graphName, cypherStmt, columns, params)

    # def queryCypher(self, cypherStmt:str, columns:list=None , params:tuple=None) -> psycopg.cursor :
    #     return queryCypher(self.connection, self.graphName, cypherStmt, columns, params)

