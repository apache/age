// Generated from c:\Users\Bitnine\github\rhizome-ai\incubator-age\drivers\golang\parser\Age.g4 by ANTLR 4.8
import org.antlr.v4.runtime.Lexer;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.TokenStream;
import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.atn.*;
import org.antlr.v4.runtime.dfa.DFA;
import org.antlr.v4.runtime.misc.*;

@SuppressWarnings({"all", "warnings", "unchecked", "unused", "cast"})
public class AgeLexer extends Lexer {
	static { RuntimeMetaData.checkVersion("4.8", RuntimeMetaData.VERSION); }

	protected static final DFA[] _decisionToDFA;
	protected static final PredictionContextCache _sharedContextCache =
		new PredictionContextCache();
	public static final int
		T__0=1, T__1=2, T__2=3, T__3=4, T__4=5, T__5=6, KW_VERTEX=7, KW_EDGE=8, 
		KW_PATH=9, STRING=10, BOOL=11, NULL=12, NUMBER=13, WS=14;
	public static String[] channelNames = {
		"DEFAULT_TOKEN_CHANNEL", "HIDDEN"
	};

	public static String[] modeNames = {
		"DEFAULT_MODE"
	};

	private static String[] makeRuleNames() {
		return new String[] {
			"T__0", "T__1", "T__2", "T__3", "T__4", "T__5", "KW_VERTEX", "KW_EDGE", 
			"KW_PATH", "STRING", "BOOL", "NULL", "ESC", "UNICODE", "HEX", "SAFECODEPOINT", 
			"NUMBER", "INT", "EXP", "WS"
		};
	}
	public static final String[] ruleNames = makeRuleNames();

	private static String[] makeLiteralNames() {
		return new String[] {
			null, "'['", "','", "']'", "'{'", "'}'", "':'", "'::vertex'", "'::edge'", 
			"'::path'", null, null, "'null'"
		};
	}
	private static final String[] _LITERAL_NAMES = makeLiteralNames();
	private static String[] makeSymbolicNames() {
		return new String[] {
			null, null, null, null, null, null, null, "KW_VERTEX", "KW_EDGE", "KW_PATH", 
			"STRING", "BOOL", "NULL", "NUMBER", "WS"
		};
	}
	private static final String[] _SYMBOLIC_NAMES = makeSymbolicNames();
	public static final Vocabulary VOCABULARY = new VocabularyImpl(_LITERAL_NAMES, _SYMBOLIC_NAMES);

	/**
	 * @deprecated Use {@link #VOCABULARY} instead.
	 */
	@Deprecated
	public static final String[] tokenNames;
	static {
		tokenNames = new String[_SYMBOLIC_NAMES.length];
		for (int i = 0; i < tokenNames.length; i++) {
			tokenNames[i] = VOCABULARY.getLiteralName(i);
			if (tokenNames[i] == null) {
				tokenNames[i] = VOCABULARY.getSymbolicName(i);
			}

			if (tokenNames[i] == null) {
				tokenNames[i] = "<INVALID>";
			}
		}
	}

	@Override
	@Deprecated
	public String[] getTokenNames() {
		return tokenNames;
	}

	@Override

	public Vocabulary getVocabulary() {
		return VOCABULARY;
	}


	public AgeLexer(CharStream input) {
		super(input);
		_interp = new LexerATNSimulator(this,_ATN,_decisionToDFA,_sharedContextCache);
	}

	@Override
	public String getGrammarFileName() { return "Age.g4"; }

	@Override
	public String[] getRuleNames() { return ruleNames; }

	@Override
	public String getSerializedATN() { return _serializedATN; }

	@Override
	public String[] getChannelNames() { return channelNames; }

	@Override
	public String[] getModeNames() { return modeNames; }

	@Override
	public ATN getATN() { return _ATN; }

	public static final String _serializedATN =
		"\3\u608b\ua72a\u8133\ub9ed\u417c\u3be7\u7786\u5964\2\20\u009d\b\1\4\2"+
		"\t\2\4\3\t\3\4\4\t\4\4\5\t\5\4\6\t\6\4\7\t\7\4\b\t\b\4\t\t\t\4\n\t\n\4"+
		"\13\t\13\4\f\t\f\4\r\t\r\4\16\t\16\4\17\t\17\4\20\t\20\4\21\t\21\4\22"+
		"\t\22\4\23\t\23\4\24\t\24\4\25\t\25\3\2\3\2\3\3\3\3\3\4\3\4\3\5\3\5\3"+
		"\6\3\6\3\7\3\7\3\b\3\b\3\b\3\b\3\b\3\b\3\b\3\b\3\b\3\t\3\t\3\t\3\t\3\t"+
		"\3\t\3\t\3\n\3\n\3\n\3\n\3\n\3\n\3\n\3\13\3\13\3\13\7\13R\n\13\f\13\16"+
		"\13U\13\13\3\13\3\13\3\f\3\f\3\f\3\f\3\f\3\f\3\f\3\f\3\f\5\fb\n\f\3\r"+
		"\3\r\3\r\3\r\3\r\3\16\3\16\3\16\5\16l\n\16\3\17\3\17\3\17\3\17\3\17\3"+
		"\17\3\20\3\20\3\21\3\21\3\22\5\22y\n\22\3\22\3\22\3\22\6\22~\n\22\r\22"+
		"\16\22\177\5\22\u0082\n\22\3\22\5\22\u0085\n\22\3\23\3\23\3\23\7\23\u008a"+
		"\n\23\f\23\16\23\u008d\13\23\5\23\u008f\n\23\3\24\3\24\5\24\u0093\n\24"+
		"\3\24\3\24\3\25\6\25\u0098\n\25\r\25\16\25\u0099\3\25\3\25\2\2\26\3\3"+
		"\5\4\7\5\t\6\13\7\r\b\17\t\21\n\23\13\25\f\27\r\31\16\33\2\35\2\37\2!"+
		"\2#\17%\2\'\2)\20\3\2\n\n\2$$\61\61^^ddhhppttvv\5\2\62;CHch\5\2\2!$$^"+
		"^\3\2\62;\3\2\63;\4\2GGgg\4\2--//\5\2\13\f\17\17\"\"\2\u00a2\2\3\3\2\2"+
		"\2\2\5\3\2\2\2\2\7\3\2\2\2\2\t\3\2\2\2\2\13\3\2\2\2\2\r\3\2\2\2\2\17\3"+
		"\2\2\2\2\21\3\2\2\2\2\23\3\2\2\2\2\25\3\2\2\2\2\27\3\2\2\2\2\31\3\2\2"+
		"\2\2#\3\2\2\2\2)\3\2\2\2\3+\3\2\2\2\5-\3\2\2\2\7/\3\2\2\2\t\61\3\2\2\2"+
		"\13\63\3\2\2\2\r\65\3\2\2\2\17\67\3\2\2\2\21@\3\2\2\2\23G\3\2\2\2\25N"+
		"\3\2\2\2\27a\3\2\2\2\31c\3\2\2\2\33h\3\2\2\2\35m\3\2\2\2\37s\3\2\2\2!"+
		"u\3\2\2\2#x\3\2\2\2%\u008e\3\2\2\2\'\u0090\3\2\2\2)\u0097\3\2\2\2+,\7"+
		"]\2\2,\4\3\2\2\2-.\7.\2\2.\6\3\2\2\2/\60\7_\2\2\60\b\3\2\2\2\61\62\7}"+
		"\2\2\62\n\3\2\2\2\63\64\7\177\2\2\64\f\3\2\2\2\65\66\7<\2\2\66\16\3\2"+
		"\2\2\678\7<\2\289\7<\2\29:\7x\2\2:;\7g\2\2;<\7t\2\2<=\7v\2\2=>\7g\2\2"+
		">?\7z\2\2?\20\3\2\2\2@A\7<\2\2AB\7<\2\2BC\7g\2\2CD\7f\2\2DE\7i\2\2EF\7"+
		"g\2\2F\22\3\2\2\2GH\7<\2\2HI\7<\2\2IJ\7r\2\2JK\7c\2\2KL\7v\2\2LM\7j\2"+
		"\2M\24\3\2\2\2NS\7$\2\2OR\5\33\16\2PR\5!\21\2QO\3\2\2\2QP\3\2\2\2RU\3"+
		"\2\2\2SQ\3\2\2\2ST\3\2\2\2TV\3\2\2\2US\3\2\2\2VW\7$\2\2W\26\3\2\2\2XY"+
		"\7v\2\2YZ\7t\2\2Z[\7w\2\2[b\7g\2\2\\]\7h\2\2]^\7c\2\2^_\7n\2\2_`\7u\2"+
		"\2`b\7g\2\2aX\3\2\2\2a\\\3\2\2\2b\30\3\2\2\2cd\7p\2\2de\7w\2\2ef\7n\2"+
		"\2fg\7n\2\2g\32\3\2\2\2hk\7^\2\2il\t\2\2\2jl\5\35\17\2ki\3\2\2\2kj\3\2"+
		"\2\2l\34\3\2\2\2mn\7w\2\2no\5\37\20\2op\5\37\20\2pq\5\37\20\2qr\5\37\20"+
		"\2r\36\3\2\2\2st\t\3\2\2t \3\2\2\2uv\n\4\2\2v\"\3\2\2\2wy\7/\2\2xw\3\2"+
		"\2\2xy\3\2\2\2yz\3\2\2\2z\u0081\5%\23\2{}\7\60\2\2|~\t\5\2\2}|\3\2\2\2"+
		"~\177\3\2\2\2\177}\3\2\2\2\177\u0080\3\2\2\2\u0080\u0082\3\2\2\2\u0081"+
		"{\3\2\2\2\u0081\u0082\3\2\2\2\u0082\u0084\3\2\2\2\u0083\u0085\5\'\24\2"+
		"\u0084\u0083\3\2\2\2\u0084\u0085\3\2\2\2\u0085$\3\2\2\2\u0086\u008f\7"+
		"\62\2\2\u0087\u008b\t\6\2\2\u0088\u008a\t\5\2\2\u0089\u0088\3\2\2\2\u008a"+
		"\u008d\3\2\2\2\u008b\u0089\3\2\2\2\u008b\u008c\3\2\2\2\u008c\u008f\3\2"+
		"\2\2\u008d\u008b\3\2\2\2\u008e\u0086\3\2\2\2\u008e\u0087\3\2\2\2\u008f"+
		"&\3\2\2\2\u0090\u0092\t\7\2\2\u0091\u0093\t\b\2\2\u0092\u0091\3\2\2\2"+
		"\u0092\u0093\3\2\2\2\u0093\u0094\3\2\2\2\u0094\u0095\5%\23\2\u0095(\3"+
		"\2\2\2\u0096\u0098\t\t\2\2\u0097\u0096\3\2\2\2\u0098\u0099\3\2\2\2\u0099"+
		"\u0097\3\2\2\2\u0099\u009a\3\2\2\2\u009a\u009b\3\2\2\2\u009b\u009c\b\25"+
		"\2\2\u009c*\3\2\2\2\17\2QSakx\177\u0081\u0084\u008b\u008e\u0092\u0099"+
		"\3\b\2\2";
	public static final ATN _ATN =
		new ATNDeserializer().deserialize(_serializedATN.toCharArray());
	static {
		_decisionToDFA = new DFA[_ATN.getNumberOfDecisions()];
		for (int i = 0; i < _ATN.getNumberOfDecisions(); i++) {
			_decisionToDFA[i] = new DFA(_ATN.getDecisionState(i), i);
		}
	}
}