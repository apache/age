# Generated from antlr/age.g4 by ANTLR 4.9.2
# encoding: utf-8
from antlr4 import *
from io import StringIO
import sys
if sys.version_info[1] > 5:
	from typing import TextIO
else:
	from typing.io import TextIO


def serializedATN():
    with StringIO() as buf:
        buf.write("\3\u608b\ua72a\u8133\ub9ed\u417c\u3be7\u7786\u5964\3\21")
        buf.write("Y\4\2\t\2\4\3\t\3\4\4\t\4\4\5\t\5\4\6\t\6\4\7\t\7\4\b")
        buf.write("\t\b\4\t\t\t\3\2\3\2\3\2\3\2\5\2\27\n\2\3\3\3\3\3\3\3")
        buf.write("\4\3\4\3\4\3\5\3\5\3\5\3\5\3\5\3\5\3\5\7\5&\n\5\f\5\16")
        buf.write("\5)\13\5\3\5\3\5\3\5\3\6\3\6\3\6\3\6\3\6\3\6\3\6\5\6\65")
        buf.write("\n\6\3\7\3\7\3\7\3\7\7\7;\n\7\f\7\16\7>\13\7\3\7\3\7\3")
        buf.write("\7\3\7\5\7D\n\7\3\b\3\b\3\b\3\b\3\t\3\t\3\t\3\t\7\tN\n")
        buf.write("\t\f\t\16\tQ\13\t\3\t\3\t\3\t\3\t\5\tW\n\t\3\t\2\2\n\2")
        buf.write("\4\6\b\n\f\16\20\2\2\2^\2\26\3\2\2\2\4\30\3\2\2\2\6\33")
        buf.write("\3\2\2\2\b\36\3\2\2\2\n\64\3\2\2\2\fC\3\2\2\2\16E\3\2")
        buf.write("\2\2\20V\3\2\2\2\22\27\5\n\6\2\23\27\5\4\3\2\24\27\5\6")
        buf.write("\4\2\25\27\5\b\5\2\26\22\3\2\2\2\26\23\3\2\2\2\26\24\3")
        buf.write("\2\2\2\26\25\3\2\2\2\27\3\3\2\2\2\30\31\5\f\7\2\31\32")
        buf.write("\7\f\2\2\32\5\3\2\2\2\33\34\5\f\7\2\34\35\7\r\2\2\35\7")
        buf.write("\3\2\2\2\36\37\7\3\2\2\37\'\5\4\3\2 !\7\4\2\2!\"\5\6\4")
        buf.write("\2\"#\7\4\2\2#$\5\4\3\2$&\3\2\2\2% \3\2\2\2&)\3\2\2\2")
        buf.write("\'%\3\2\2\2\'(\3\2\2\2(*\3\2\2\2)\'\3\2\2\2*+\7\5\2\2")
        buf.write("+,\7\16\2\2,\t\3\2\2\2-\65\7\17\2\2.\65\7\20\2\2/\65\5")
        buf.write("\f\7\2\60\65\5\20\t\2\61\65\7\6\2\2\62\65\7\7\2\2\63\65")
        buf.write("\7\b\2\2\64-\3\2\2\2\64.\3\2\2\2\64/\3\2\2\2\64\60\3\2")
        buf.write("\2\2\64\61\3\2\2\2\64\62\3\2\2\2\64\63\3\2\2\2\65\13\3")
        buf.write("\2\2\2\66\67\7\t\2\2\67<\5\16\b\289\7\4\2\29;\5\16\b\2")
        buf.write(":8\3\2\2\2;>\3\2\2\2<:\3\2\2\2<=\3\2\2\2=?\3\2\2\2><\3")
        buf.write("\2\2\2?@\7\n\2\2@D\3\2\2\2AB\7\t\2\2BD\7\n\2\2C\66\3\2")
        buf.write("\2\2CA\3\2\2\2D\r\3\2\2\2EF\7\17\2\2FG\7\13\2\2GH\5\n")
        buf.write("\6\2H\17\3\2\2\2IJ\7\3\2\2JO\5\n\6\2KL\7\4\2\2LN\5\n\6")
        buf.write("\2MK\3\2\2\2NQ\3\2\2\2OM\3\2\2\2OP\3\2\2\2PR\3\2\2\2Q")
        buf.write("O\3\2\2\2RS\7\5\2\2SW\3\2\2\2TU\7\3\2\2UW\7\5\2\2VI\3")
        buf.write("\2\2\2VT\3\2\2\2W\21\3\2\2\2\t\26\'\64<COV")
        return buf.getvalue()


class ageParser ( Parser ):

    grammarFileName = "age.g4"

    atn = ATNDeserializer().deserialize(serializedATN())

    decisionsToDFA = [ DFA(ds, i) for i, ds in enumerate(atn.decisionToState) ]

    sharedContextCache = PredictionContextCache()

    literalNames = [ "<INVALID>", "'['", "','", "']'", "'true'", "'false'", 
                     "'null'", "'{'", "'}'", "':'", "'::vertex'", "'::edge'", 
                     "'::path'" ]

    symbolicNames = [ "<INVALID>", "<INVALID>", "<INVALID>", "<INVALID>", 
                      "<INVALID>", "<INVALID>", "<INVALID>", "<INVALID>", 
                      "<INVALID>", "<INVALID>", "KW_VERTEX", "KW_EDGE", 
                      "KW_PATH", "STRING", "NUMBER", "WS" ]

    RULE_ageout = 0
    RULE_vertex = 1
    RULE_edge = 2
    RULE_path = 3
    RULE_value = 4
    RULE_properties = 5
    RULE_pair = 6
    RULE_arr = 7

    ruleNames =  [ "ageout", "vertex", "edge", "path", "value", "properties", 
                   "pair", "arr" ]

    EOF = Token.EOF
    T__0=1
    T__1=2
    T__2=3
    T__3=4
    T__4=5
    T__5=6
    T__6=7
    T__7=8
    T__8=9
    KW_VERTEX=10
    KW_EDGE=11
    KW_PATH=12
    STRING=13
    NUMBER=14
    WS=15

    def __init__(self, input:TokenStream, output:TextIO = sys.stdout):
        super().__init__(input, output)
        self.checkVersion("4.9.2")
        self._interp = ParserATNSimulator(self, self.atn, self.decisionsToDFA, self.sharedContextCache)
        self._predicates = None




    class AgeoutContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def value(self):
            return self.getTypedRuleContext(ageParser.ValueContext,0)


        def vertex(self):
            return self.getTypedRuleContext(ageParser.VertexContext,0)


        def edge(self):
            return self.getTypedRuleContext(ageParser.EdgeContext,0)


        def path(self):
            return self.getTypedRuleContext(ageParser.PathContext,0)


        def getRuleIndex(self):
            return ageParser.RULE_ageout

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterAgeout" ):
                listener.enterAgeout(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitAgeout" ):
                listener.exitAgeout(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitAgeout" ):
                return visitor.visitAgeout(self)
            else:
                return visitor.visitChildren(self)




    def ageout(self):

        localctx = ageParser.AgeoutContext(self, self._ctx, self.state)
        self.enterRule(localctx, 0, self.RULE_ageout)
        try:
            self.state = 20
            self._errHandler.sync(self)
            la_ = self._interp.adaptivePredict(self._input,0,self._ctx)
            if la_ == 1:
                self.enterOuterAlt(localctx, 1)
                self.state = 16
                self.value()
                pass

            elif la_ == 2:
                self.enterOuterAlt(localctx, 2)
                self.state = 17
                self.vertex()
                pass

            elif la_ == 3:
                self.enterOuterAlt(localctx, 3)
                self.state = 18
                self.edge()
                pass

            elif la_ == 4:
                self.enterOuterAlt(localctx, 4)
                self.state = 19
                self.path()
                pass


        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class VertexContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def properties(self):
            return self.getTypedRuleContext(ageParser.PropertiesContext,0)


        def KW_VERTEX(self):
            return self.getToken(ageParser.KW_VERTEX, 0)

        def getRuleIndex(self):
            return ageParser.RULE_vertex

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterVertex" ):
                listener.enterVertex(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitVertex" ):
                listener.exitVertex(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitVertex" ):
                return visitor.visitVertex(self)
            else:
                return visitor.visitChildren(self)




    def vertex(self):

        localctx = ageParser.VertexContext(self, self._ctx, self.state)
        self.enterRule(localctx, 2, self.RULE_vertex)
        try:
            self.enterOuterAlt(localctx, 1)
            self.state = 22
            self.properties()
            self.state = 23
            self.match(ageParser.KW_VERTEX)
        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class EdgeContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def properties(self):
            return self.getTypedRuleContext(ageParser.PropertiesContext,0)


        def KW_EDGE(self):
            return self.getToken(ageParser.KW_EDGE, 0)

        def getRuleIndex(self):
            return ageParser.RULE_edge

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterEdge" ):
                listener.enterEdge(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitEdge" ):
                listener.exitEdge(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitEdge" ):
                return visitor.visitEdge(self)
            else:
                return visitor.visitChildren(self)




    def edge(self):

        localctx = ageParser.EdgeContext(self, self._ctx, self.state)
        self.enterRule(localctx, 4, self.RULE_edge)
        try:
            self.enterOuterAlt(localctx, 1)
            self.state = 25
            self.properties()
            self.state = 26
            self.match(ageParser.KW_EDGE)
        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class PathContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def vertex(self, i:int=None):
            if i is None:
                return self.getTypedRuleContexts(ageParser.VertexContext)
            else:
                return self.getTypedRuleContext(ageParser.VertexContext,i)


        def KW_PATH(self):
            return self.getToken(ageParser.KW_PATH, 0)

        def edge(self, i:int=None):
            if i is None:
                return self.getTypedRuleContexts(ageParser.EdgeContext)
            else:
                return self.getTypedRuleContext(ageParser.EdgeContext,i)


        def getRuleIndex(self):
            return ageParser.RULE_path

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterPath" ):
                listener.enterPath(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitPath" ):
                listener.exitPath(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitPath" ):
                return visitor.visitPath(self)
            else:
                return visitor.visitChildren(self)




    def path(self):

        localctx = ageParser.PathContext(self, self._ctx, self.state)
        self.enterRule(localctx, 6, self.RULE_path)
        self._la = 0 # Token type
        try:
            self.enterOuterAlt(localctx, 1)
            self.state = 28
            self.match(ageParser.T__0)
            self.state = 29
            self.vertex()
            self.state = 37
            self._errHandler.sync(self)
            _la = self._input.LA(1)
            while _la==ageParser.T__1:
                self.state = 30
                self.match(ageParser.T__1)
                self.state = 31
                self.edge()
                self.state = 32
                self.match(ageParser.T__1)
                self.state = 33
                self.vertex()
                self.state = 39
                self._errHandler.sync(self)
                _la = self._input.LA(1)

            self.state = 40
            self.match(ageParser.T__2)
            self.state = 41
            self.match(ageParser.KW_PATH)
        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class ValueContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def STRING(self):
            return self.getToken(ageParser.STRING, 0)

        def NUMBER(self):
            return self.getToken(ageParser.NUMBER, 0)

        def properties(self):
            return self.getTypedRuleContext(ageParser.PropertiesContext,0)


        def arr(self):
            return self.getTypedRuleContext(ageParser.ArrContext,0)


        def getRuleIndex(self):
            return ageParser.RULE_value

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterValue" ):
                listener.enterValue(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitValue" ):
                listener.exitValue(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitValue" ):
                return visitor.visitValue(self)
            else:
                return visitor.visitChildren(self)




    def value(self):

        localctx = ageParser.ValueContext(self, self._ctx, self.state)
        self.enterRule(localctx, 8, self.RULE_value)
        try:
            self.state = 50
            self._errHandler.sync(self)
            token = self._input.LA(1)
            if token in [ageParser.STRING]:
                self.enterOuterAlt(localctx, 1)
                self.state = 43
                self.match(ageParser.STRING)
                pass
            elif token in [ageParser.NUMBER]:
                self.enterOuterAlt(localctx, 2)
                self.state = 44
                self.match(ageParser.NUMBER)
                pass
            elif token in [ageParser.T__6]:
                self.enterOuterAlt(localctx, 3)
                self.state = 45
                self.properties()
                pass
            elif token in [ageParser.T__0]:
                self.enterOuterAlt(localctx, 4)
                self.state = 46
                self.arr()
                pass
            elif token in [ageParser.T__3]:
                self.enterOuterAlt(localctx, 5)
                self.state = 47
                self.match(ageParser.T__3)
                pass
            elif token in [ageParser.T__4]:
                self.enterOuterAlt(localctx, 6)
                self.state = 48
                self.match(ageParser.T__4)
                pass
            elif token in [ageParser.T__5]:
                self.enterOuterAlt(localctx, 7)
                self.state = 49
                self.match(ageParser.T__5)
                pass
            else:
                raise NoViableAltException(self)

        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class PropertiesContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def pair(self, i:int=None):
            if i is None:
                return self.getTypedRuleContexts(ageParser.PairContext)
            else:
                return self.getTypedRuleContext(ageParser.PairContext,i)


        def getRuleIndex(self):
            return ageParser.RULE_properties

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterProperties" ):
                listener.enterProperties(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitProperties" ):
                listener.exitProperties(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitProperties" ):
                return visitor.visitProperties(self)
            else:
                return visitor.visitChildren(self)




    def properties(self):

        localctx = ageParser.PropertiesContext(self, self._ctx, self.state)
        self.enterRule(localctx, 10, self.RULE_properties)
        self._la = 0 # Token type
        try:
            self.state = 65
            self._errHandler.sync(self)
            la_ = self._interp.adaptivePredict(self._input,4,self._ctx)
            if la_ == 1:
                self.enterOuterAlt(localctx, 1)
                self.state = 52
                self.match(ageParser.T__6)
                self.state = 53
                self.pair()
                self.state = 58
                self._errHandler.sync(self)
                _la = self._input.LA(1)
                while _la==ageParser.T__1:
                    self.state = 54
                    self.match(ageParser.T__1)
                    self.state = 55
                    self.pair()
                    self.state = 60
                    self._errHandler.sync(self)
                    _la = self._input.LA(1)

                self.state = 61
                self.match(ageParser.T__7)
                pass

            elif la_ == 2:
                self.enterOuterAlt(localctx, 2)
                self.state = 63
                self.match(ageParser.T__6)
                self.state = 64
                self.match(ageParser.T__7)
                pass


        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class PairContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def STRING(self):
            return self.getToken(ageParser.STRING, 0)

        def value(self):
            return self.getTypedRuleContext(ageParser.ValueContext,0)


        def getRuleIndex(self):
            return ageParser.RULE_pair

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterPair" ):
                listener.enterPair(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitPair" ):
                listener.exitPair(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitPair" ):
                return visitor.visitPair(self)
            else:
                return visitor.visitChildren(self)




    def pair(self):

        localctx = ageParser.PairContext(self, self._ctx, self.state)
        self.enterRule(localctx, 12, self.RULE_pair)
        try:
            self.enterOuterAlt(localctx, 1)
            self.state = 67
            self.match(ageParser.STRING)
            self.state = 68
            self.match(ageParser.T__8)
            self.state = 69
            self.value()
        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx


    class ArrContext(ParserRuleContext):
        __slots__ = 'parser'

        def __init__(self, parser, parent:ParserRuleContext=None, invokingState:int=-1):
            super().__init__(parent, invokingState)
            self.parser = parser

        def value(self, i:int=None):
            if i is None:
                return self.getTypedRuleContexts(ageParser.ValueContext)
            else:
                return self.getTypedRuleContext(ageParser.ValueContext,i)


        def getRuleIndex(self):
            return ageParser.RULE_arr

        def enterRule(self, listener:ParseTreeListener):
            if hasattr( listener, "enterArr" ):
                listener.enterArr(self)

        def exitRule(self, listener:ParseTreeListener):
            if hasattr( listener, "exitArr" ):
                listener.exitArr(self)

        def accept(self, visitor:ParseTreeVisitor):
            if hasattr( visitor, "visitArr" ):
                return visitor.visitArr(self)
            else:
                return visitor.visitChildren(self)




    def arr(self):

        localctx = ageParser.ArrContext(self, self._ctx, self.state)
        self.enterRule(localctx, 14, self.RULE_arr)
        self._la = 0 # Token type
        try:
            self.state = 84
            self._errHandler.sync(self)
            la_ = self._interp.adaptivePredict(self._input,6,self._ctx)
            if la_ == 1:
                self.enterOuterAlt(localctx, 1)
                self.state = 71
                self.match(ageParser.T__0)
                self.state = 72
                self.value()
                self.state = 77
                self._errHandler.sync(self)
                _la = self._input.LA(1)
                while _la==ageParser.T__1:
                    self.state = 73
                    self.match(ageParser.T__1)
                    self.state = 74
                    self.value()
                    self.state = 79
                    self._errHandler.sync(self)
                    _la = self._input.LA(1)

                self.state = 80
                self.match(ageParser.T__2)
                pass

            elif la_ == 2:
                self.enterOuterAlt(localctx, 2)
                self.state = 82
                self.match(ageParser.T__0)
                self.state = 83
                self.match(ageParser.T__2)
                pass


        except RecognitionException as re:
            localctx.exception = re
            self._errHandler.reportError(self, re)
            self._errHandler.recover(self, re)
        finally:
            self.exitRule()
        return localctx





