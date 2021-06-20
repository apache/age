import psycopg2 
from psycopg2 import errors
from .exceptions import *
from .builder import buildGraph, getRows, ResultHandler 


_EXCEPTION_NoConnection = NoConnection()
_EXCEPTION_GraphNotSet = GraphNotSet()
_EXCEPTION_NoCursor = NoCursor()


class Age:
    def __init__(self):
        self.conn = None    # psycopg2 connection
        self.cursor = None  # psycopg2 cursor
        self.graphName = None

    def connect(self, dsn=None, graph=None, connection_factory=None, cursor_factory=None, **kwargs):
        conn = psycopg2.connect(dsn, connection_factory, cursor_factory, **kwargs)
        cursor = conn.cursor()
        self.conn = conn
        self.cursor = cursor
        self.cursor.execute("LOAD 'age';")
        self.cursor.execute("SET search_path = ag_catalog, '$user', public;")
        if graph != None:
            try :
                self.setGraph(graph)
            except GraphNotFound:
                self.createGraph(graph)
                self.setGraph(graph)

        return self

    def close(self):
        self.cursor.close()
        self.conn.close()

    def setGraph(self, graph):
        if self.graphExists(graph) :
            # apache age doesn't have configuration parameter "graph_path" 
            # self.cursor.execute("SET graph_path = %s", (graph,))
            self.graphName = graph
            return self
        else:
            raise GraphNotFound(graph)
        return self

    def graphExists(self, graph):
        self.cursor.execute("SELECT count(*) FROM ag_graph WHERE name=%s", (graph,))
        return self.cursor.fetchone()[0] > 0 

    def createGraph(self, graphName):
        if self.graphExists(graphName) :
            raise GraphAlreadyExists(graphName)
        else:
            self.cursor.execute("SELECT create_graph(%s);", (graphName,))

    def deleteGraph(self, graphName):
        if self.graphExists(graphName) :
            self.cursor.execute("SELECT drop_graph(%s, true);", (graphName,))
            

    def newCursor(self):
        self.cursor.close()
        self.cursor = self.conn.cursor()
        return self

    def _checkReady(self):
        if self.graphName == None:
            raise _EXCEPTION_GraphNotSet
        if self.conn == None:
            raise _EXCEPTION_NoConnection
        if self.cursor == None:
            raise _EXCEPTION_NoCursor

    def _checkGraphName(self):
        if self.graphName == None:
            raise _EXCEPTION_GraphNotSet
            

    def commit(self):
        self.conn.commit()
        
    def rollback(self):
        self.conn.rollback()
        
    def execSql(self, stmt, *args):
        self._checkReady()
        try:
            self.cursor.execute(stmt, *args)
            return self
        except Exception as cause:
            self.conn.rollback()
            raise SqlExcutionError(stmt,cause)
        
    
    def execCypher(self, cypherStmt, *args):
        self._checkGraphName()
        stmt = "SELECT * from cypher('"+self.graphName+"', $$ "+cypherStmt+" $$) as (v agtype);"
        return self.execSql(stmt, *args)

    def querySql(self, stmt, *args):
        return self.execSql(stmt, *args)
        

    def queryCypher(self, cypherStmt, *args):
        return self.execCypher(cypherStmt, *args)
    
    def graph(self, resultHandler:ResultHandler=None):
        return buildGraph(self.cursor, resultHandler=resultHandler)

    def rows(self):
        return getRows(self.cursor)


def execCypher(cursor, graphName, cypherStmt, *args):
    stmt = "SELECT * from cypher('"+ graphName +"', $$ "+cypherStmt+" $$) as (v agtype);"
    return cursor.execute(stmt, *args)

def queryCypher(cursor, graphName, cypherStmt, *args):
    stmt = "SELECT * from cypher('"+ graphName +"', $$ "+cypherStmt+" $$) as (v agtype);"
    return cursor.execute(stmt, *args)

