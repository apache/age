import json
from io import StringIO 


TP_NONE = 0
TP_VERTEX = 1
TP_EDGE = 2
TP_PATH = 3


class Graph():
    def __init__(self, stmt=None) -> None:
        self.statement = stmt
        self.rows = list()
        self.vertices = dict()

    def __iter__(self):
        return self.rows.__iter__()

    def __len__(self):
        return self.rows.__len__()

    def __getitem__(self,index):
        return self.rows[index]
        
    def size(self):
        return self.rows.__len__()

    def append(self, agObj):
        self.rows.append(agObj)
    
    def getVertices(self):
        return self.vertices
    
    def getVertex(self, id):
        if id in self.vertices:
            return self.vertices[id]
        else:
            return None

class AGObj:
    @property
    def gtype(self):
        return TP_NONE


class Path(AGObj):
    def __init__(self, start=None, rel=None, end=None) -> None:
        self.start = start
        self.rel = rel
        self.end = end

    @property
    def gtype(self):
        return TP_PATH

    def __str__(self) -> str:
        return self.toString()

    def __repr__(self) -> str:
        return self.toString()

    def toString(self) -> str: 
        buf = StringIO()
        buf.write("[")
        if self.start != None:
            self.start._toString(buf)
   
        buf.write(",")
        if self.rel != None:
            self.rel._toString(buf)
            
        buf.write(",")

        if self.end != None:
            self.end._toString(buf)

        buf.write("]::PATH")

        return buf.getvalue()

    def toJson(self) -> str:
        buf = StringIO()
        buf.write("{\"gtype\": \"path\", ")
        
        buf.write("\"start\": ")
        if self.start != None:
            self.start._toJson(buf)

        buf.write(", \"rel\": ")
        if self.rel != None:
            self.rel._toJson(buf)
            
        buf.write(", \"end\": ")

        if self.end != None:
            self.end._toJson(buf)

        buf.write("}")

        return buf.getvalue()


    

class Vertex(AGObj):
    def __init__(self, id=None, label=None, properties=None) -> None:
        self.id = id
        self.label = label
        self.properties = properties

    @property
    def gtype(self):
        return TP_VERTEX

    def __setitem__(self,name, value):
        self.properties[name]=value
        
    def __getitem__(self,name):
        if name in self.properties:
            return self.properties[name]
        else:
            return None

    def __str__(self) -> str:
        return self.toString()

    def __repr__(self) -> str:
        return self.toString()

    def toString(self) -> str: 
        return nodeToString(self)

    def _toString(self, buf): 
        _nodeToString(self, buf)

    def toJson(self) -> str:
        return nodeToJson(self)

    def _toJson(self, buf): 
        _nodeToJson(self, buf)


class Edge(AGObj):
    def __init__(self, id=None, label=None, properties=None) -> None:
        self.id = id
        self.label = label
        self.start_id = None
        self.end_id = None
        self.properties = properties

    @property
    def gtype(self):
        return TP_EDGE

    def __setitem__(self,name, value):
        self.properties[name]=value
        
    def __getitem__(self,name):
        if name in self.properties:
            return self.properties[name]
        else:
            return None

    def __str__(self) -> str:
        return self.toString()

    def __repr__(self) -> str:
        return self.toString()

    def extraStrFormat(node, buf):
        if node.start_id != None:
            buf.write(", start_id:")
            buf.write(node.start_id)

        if node.end_id != None:
            buf.write(", end_id:")
            buf.write(node.end_id)


    def toString(self) -> str: 
        return nodeToString(self, Edge.extraStrFormat)

    def _toString(self, buf): 
        _nodeToString(self, buf, Edge.extraStrFormat)

    def extraJsonFormat(node, buf):
        if node.start_id != None:
            buf.write(", \"start_id\": \"")
            buf.write(node.start_id)
            buf.write("\"")

        if node.end_id != None:
            buf.write(", \"end_id\": \"")
            buf.write(node.end_id)
            buf.write("\"")

    def toJson(self) -> str:
        return nodeToJson(self, Edge.extraJsonFormat)

    def _toJson(self, buf): 
        _nodeToJson(self, buf, Edge.extraJsonFormat)


def nodeToString(node, extraFormatter=None):
    buf = StringIO() 
    _nodeToString(node,buf,extraFormatter=extraFormatter)
    return buf.getvalue()


def _nodeToString(node, buf, extraFormatter=None):
    buf.write("{")
    if node.label != None:
        buf.write("label:")
        buf.write(node.label)
        
    if node.id != None:
        buf.write(", id:")
        buf.write(node.id)
        
    if node.properties != None:
        buf.write(", properties:{")
        for k,v in node.properties.items():
            buf.write(k)
            buf.write(": ")
            buf.write(v)
            buf.write(",")
        buf.write("}")

    if extraFormatter != None:
        extraFormatter(node, buf)
        
    if node.gtype == TP_VERTEX:
        buf.write("}::VERTEX")
    if node.gtype == TP_EDGE:
        buf.write("}::EDGE")


def nodeToJson(node, extraFormatter=None):
    buf = StringIO()
    _nodeToJson(node, buf, extraFormatter=extraFormatter)
    return buf.getvalue()


def _nodeToJson(node, buf, extraFormatter=None):
    buf.write("{\"gtype\": ")
    if node.gtype == TP_VERTEX:
        buf.write("\"vertex\", ")
    if node.gtype == TP_EDGE:
        buf.write("\"edge\", ")

    if node.label != None:
        buf.write("\"label\":\"")
        buf.write(node.label)
        buf.write("\"")
        
    if node.id != None:
        buf.write(", \"id\":")
        buf.write(node.id)
        
    if extraFormatter != None:
        extraFormatter(node, buf)
        
    if node.properties != None:
        buf.write(", \"properties\":{")
        for k,v in node.properties.items():
            buf.write("\"")
            buf.write(k)
            buf.write("\": \"")
            buf.write(v)
            buf.write("\",")
        buf.write("}")
    buf.write("}")
    