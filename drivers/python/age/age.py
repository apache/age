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
import psycopg2 
from psycopg2 import errors
from psycopg2 import extensions as ext
from psycopg2 import sql
from .exceptions import *
from .builder import ResultHandler , parseAgeValue, newResultHandler


_EXCEPTION_NoConnection = NoConnection()
_EXCEPTION_GraphNotSet = GraphNotSet()

WHITESPACE = re.compile('\s')

def setUpAge(conn:ext.connection, graphName:str):
    with conn.cursor() as cursor:
        cursor.execute("LOAD 'age';")
        cursor.execute("SET search_path = ag_catalog, '$user', public;")

        cursor.execute("SELECT typelem FROM pg_type WHERE typname='_agtype'")
        oid = cursor.fetchone()[0]
        if oid == None :
            raise AgeNotSet()

        AGETYPE = ext.new_type((oid,), 'AGETYPE', parseAgeValue)
        ext.register_type(AGETYPE)
        # ext.register_adapter(Path, marshalAgtValue)

        # Check graph exists
        if graphName != None:
            checkGraphCreated(conn, graphName)

# Create the graph, if it does not exist
def checkGraphCreated(conn:ext.connection, graphName:str):
    with conn.cursor() as cursor:
        cursor.execute(sql.SQL("SELECT count(*) FROM ag_graph WHERE name={graphName}").format(graphName=sql.Literal(graphName)))
        if cursor.fetchone()[0] == 0:
            cursor.execute(sql.SQL("SELECT create_graph({graphName});").format(graphName=sql.Literal(graphName)))
            conn.commit()


def deleteGraph(conn:ext.connection, graphName:str):
    with conn.cursor() as cursor:
        cursor.execute(sql.SQL("SELECT drop_graph({graphName}, true);").format(graphName=sql.Literal(graphName)))
        conn.commit()
    

def buildCypher(graphName:str, cypherStmt:str, columns:list) ->str:
    if graphName == None:
        raise _EXCEPTION_GraphNotSet
    
    columnExp=[]
    if columns != None and len(columns) > 0:
        for col in columns:
            if col.strip() == '':
                continue
            elif WHITESPACE.search(col) != None:
                columnExp.append(col)
            else:
                columnExp.append(col + " agtype")
    else:
        columnExp.append('v agtype')

    stmtArr = []
    stmtArr.append("SELECT * from cypher(NULL,NULL) as (")
    stmtArr.append(','.join(columnExp))
    stmtArr.append(");")
    return "".join(stmtArr)

def execSql(conn:ext.connection, stmt:str, commit:bool=False, params:tuple=None) -> ext.cursor :
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


def querySql(conn:ext.connection, stmt:str, params:tuple=None) -> ext.cursor :
    return execSql(conn, stmt, False, params)

# Execute cypher statement and return cursor.
# If cypher statement changes data (create, set, remove), 
# You must commit session(ag.commit()) 
# (Otherwise the execution cannot make any effect.)
def execCypher(conn:ext.connection, graphName:str, cypherStmt:str, cols:list=None, params:tuple=None) -> ext.cursor :
    if conn == None or conn.closed:
        raise _EXCEPTION_NoConnection

    cursor = conn.cursor()
    #clean up the string for mogrificiation
    cypherStmt = cypherStmt.replace("\n", "")
    cypherStmt = cypherStmt.replace("\t", "")
    cypher = str(cursor.mogrify(cypherStmt, params))
    cypher = cypher[2:len(cypher)-1]

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


def cypher(cursor:ext.cursor, graphName:str, cypherStmt:str, cols:list=None, params:tuple=None) -> ext.cursor :
    #clean up the string for mogrificiation
    cypherStmt = cypherStmt.replace("\n", "")
    cypherStmt = cypherStmt.replace("\t", "")
    cypher = str(cursor.mogrify(cypherStmt, params))
    cypher = cypher[2:len(cypher)-1]

    preparedStmt = "SELECT * FROM age_prepare_cypher({graphName},{cypherStmt})"
    cursor.execute(sql.SQL(preparedStmt).format(graphName=sql.Literal(graphName),cypherStmt=sql.Literal(cypher)))

    stmt = buildCypher(graphName, cypher, cols)
    cursor.execute(stmt)


# def execCypherWithReturn(conn:ext.connection, graphName:str, cypherStmt:str, columns:list=None , params:tuple=None) -> ext.cursor :
#     stmt = buildCypher(graphName, cypherStmt, columns)
#     return execSql(conn, stmt, False, params)

# def queryCypher(conn:ext.connection, graphName:str, cypherStmt:str, columns:list=None , params:tuple=None) -> ext.cursor :
#     return execCypherWithReturn(conn, graphName, cypherStmt, columns, params)


class Age:
    def __init__(self):
        self.connection = None    # psycopg2 connection]
        self.graphName = None


    def get_pg_version(self):
        """Get postgres version"""
        crsr = self.connection.cursor()
        crsr.execute('SELECT version()')
        db_version = crsr.fetchone()
        crsr.close()
        return db_version


    def close_connection(self):
        """close connection with postgresql database"""
        if self.connection is not None:
            self.connection.close()
            self.connection = None


    def create_graph(self, graphName):
        """create age db graph"""
        try :
            setUpAge(self.connection, graphName)
            self.graphName = graphName
        except Exception as e:
            print(e)


    def delete_graph(self, graphName = "None"):
        """delete age db graph"""
        if graphName == "None" :
            graphName = self.graphName
        try :
            deleteGraph(self.connection, graphName)
            self.graphName = None
        except Exception as e:
            print(e)


    def dictToStr(self, property):
        p = "{"
        for x,y in property.items():
            p+= x + " : "
            if type(y) == type({}):
                p+= self.dictToStr(y)
            else :
                p+= "'"
                p+= str(y)
                p+= "'"
            p+= ","
        p = p.removesuffix(',')
        p+= "}"
        return p


    def extract_vertices(self, vertices):
        """returns from an agtype to python dict vertices"""
        tmp = {}
        tmp['label'] = vertices.label
        tmp['id'] = vertices.id
        for x in vertices.properties:
            tmp[x] = vertices[x]
        return tmp


    def extract_edge(self, edge):
        """returns from an agtype to python dict edge"""
        tmp = {}
        tmp['label'] = edge.label
        tmp['id'] = edge.id
        tmp['start_id'] = edge.start_id
        tmp['end_id'] = edge.end_id
        for x in edge.properties:
            tmp[x] = edge[x]
        return tmp


    def set_vertices(self,graphName , label, property):
        """Add a vertices to the graph"""
        with self.connection.cursor() as cursor:

            query = """
            SELECT * from cypher(
                '%s', 
                $$ 
                    CREATE (v:%s %s) 
                    RETURN v
                $$
            ) as (v agtype); 
            """ % (graphName, label,self.dictToStr(property))
            try :
                cursor.execute(query)
                for row in cursor:
                    return self.extract_vertices(row[0])

                # When data inserted or updated, You must commit.
                self.connection.commit()
            except Exception as ex:
                print(type(ex), ex)
                # if exception occurs, you must rollback all transaction. 
                self.connection.rollback()


    def set_edge(self, graphName, label1, prop1, label2, prop2, edge_label, edge_prop):
        with self.connection.cursor() as cursor:
            query ="""
            SELECT * from cypher(
                '%s', 
                $$ 
                    MATCH ( a:%s %s), (b:%s %s) 
                    CREATE (a)-[r:%s %s]->(b)
                $$) as (v agtype); 
            """ % (
                graphName,label1,
                self.dictToStr(prop1), 
                label2, 
                self.dictToStr(prop2), 
                edge_label, 
                self.dictToStr(edge_prop)
            )
            try :
                cursor.execute(query)
                for row in cursor:
                    print("CREATED::", row[0])
                self.connection.commit()
            except Exception as ex:
                print(type(ex), ex)
                self.connection.rollback()


    def get_all_edge(self, graphName = "None" ):
        if graphName == "None" :
            graphName = self.graphName
        try:
            with self.connection.cursor() as cursor:
                cursor.execute(
                    """
                    SELECT * from cypher(
                        %s, 
                        $$ 
                            MATCH p=()-[]->() 
                            RETURN p 
                            LIMIT 10 
                        $$
                    ) as (v agtype); """, (graphName,))
                res = []
                for row in cursor:
                    path = row[0]
                    tmp = {}
                    tmp['v1'] = self.extract_vertices(path[0])
                    tmp['e'] = self.extract_edge(path[1])
                    tmp['v2'] = self.extract_vertices(path[2])
                    # print(tmp)
                    res.append(tmp)
                return res


        except Exception as ex:
            print(type(ex), ex)
            # if exception occurs, you must rollback even though just retrieving.
            self.connection.rollback()


    def get_all_vertices(self, graphName = "None"):
        if graphName == "None" :
            graphName = self.graphName
        res = []
        with self.connection.cursor() as cursor:
            try :
                cursor.execute(
                    """
                    SELECT * from cypher
                    (%s, 
                    $$ 
                        MATCH (n) 
                        RETURN n 
                    $$) 
                        as (v agtype); 
                    """, (graphName,))
                for row in cursor:
                    vertices = self.extract_vertices(row[0])
                    res.append(vertices)
            except Exception as e:
                print(e)
        return res


    # Connect to PostgreSQL Server and establish session and type extension environment.
    def connect(self, graph:str=None, dsn:str=None, connection_factory=None, cursor_factory=None, **kwargs):
        conn = psycopg2.connect(dsn, connection_factory, cursor_factory, **kwargs)
        setUpAge(conn, graph)
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
    
    def execCypher(self, cypherStmt:str, cols:list=None, params:tuple=None) -> ext.cursor :
        return execCypher(self.connection, self.graphName, cypherStmt, cols=cols, params=params)

    def cypher(self, cursor:ext.cursor, cypherStmt:str, cols:list=None, params:tuple=None) -> ext.cursor :
        return cypher(cursor, self.graphName, cypherStmt, cols=cols, params=params)

    # def execSql(self, stmt:str, commit:bool=False, params:tuple=None) -> ext.cursor :
    #     return execSql(self.connection, stmt, commit, params)
        
    
    # def execCypher(self, cypherStmt:str, commit:bool=False, params:tuple=None) -> ext.cursor :
    #     return execCypher(self.connection, self.graphName, cypherStmt, commit, params)

    # def execCypherWithReturn(self, cypherStmt:str, columns:list=None , params:tuple=None) -> ext.cursor :
    #     return execCypherWithReturn(self.connection, self.graphName, cypherStmt, columns, params)

    # def queryCypher(self, cypherStmt:str, columns:list=None , params:tuple=None) -> ext.cursor :
    #     return queryCypher(self.connection, self.graphName, cypherStmt, columns, params)


