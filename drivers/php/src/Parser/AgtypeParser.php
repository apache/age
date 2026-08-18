<?php

/*
 * Generated from work\webdev\apacheage\age\drivers\php\src\parser\Agtype.g4 by ANTLR 4.11.1
 */

namespace Apache\AgePhp\Parser {
	use Antlr\Antlr4\Runtime\Atn\ATN;
	use Antlr\Antlr4\Runtime\Atn\ATNDeserializer;
	use Antlr\Antlr4\Runtime\Atn\ParserATNSimulator;
	use Antlr\Antlr4\Runtime\Dfa\DFA;
	use Antlr\Antlr4\Runtime\Error\Exceptions\FailedPredicateException;
	use Antlr\Antlr4\Runtime\Error\Exceptions\NoViableAltException;
	use Antlr\Antlr4\Runtime\PredictionContexts\PredictionContextCache;
	use Antlr\Antlr4\Runtime\Error\Exceptions\RecognitionException;
	use Antlr\Antlr4\Runtime\RuleContext;
	use Antlr\Antlr4\Runtime\Token;
	use Antlr\Antlr4\Runtime\TokenStream;
	use Antlr\Antlr4\Runtime\Vocabulary;
	use Antlr\Antlr4\Runtime\VocabularyImpl;
	use Antlr\Antlr4\Runtime\RuntimeMetaData;
	use Antlr\Antlr4\Runtime\Parser;

	final class AgtypeParser extends Parser
	{
		public const T__0 = 1, T__1 = 2, T__2 = 3, T__3 = 4, T__4 = 5, T__5 = 6, 
               T__6 = 7, T__7 = 8, T__8 = 9, T__9 = 10, T__10 = 11, T__11 = 12, 
               T__12 = 13, IDENT = 14, STRING = 15, INTEGER = 16, RegularFloat = 17, 
               ExponentFloat = 18, WS = 19;

		public const RULE_agType = 0, RULE_agValue = 1, RULE_value = 2, RULE_obj = 3, 
               RULE_pair = 4, RULE_array = 5, RULE_typeAnnotation = 6, RULE_floatLiteral = 7;

		/**
		 * @var array<string>
		 */
		public const RULE_NAMES = [
			'agType', 'agValue', 'value', 'obj', 'pair', 'array', 'typeAnnotation', 
			'floatLiteral'
		];

		/**
		 * @var array<string|null>
		 */
		private const LITERAL_NAMES = [
		    null, "'true'", "'false'", "'null'", "'{'", "','", "'}'", "':'", "'['", 
		    "']'", "'::'", "'-'", "'Infinity'", "'NaN'"
		];

		/**
		 * @var array<string>
		 */
		private const SYMBOLIC_NAMES = [
		    null, null, null, null, null, null, null, null, null, null, null, 
		    null, null, null, "IDENT", "STRING", "INTEGER", "RegularFloat", "ExponentFloat", 
		    "WS"
		];

		private const SERIALIZED_ATN =
			[4, 1, 19, 80, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 2, 2, 3, 7, 3, 2, 4, 
		    7, 4, 2, 5, 7, 5, 2, 6, 7, 6, 2, 7, 7, 7, 1, 0, 1, 0, 1, 0, 1, 1, 
		    1, 1, 3, 1, 22, 8, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 
		    2, 3, 2, 32, 8, 2, 1, 3, 1, 3, 1, 3, 1, 3, 5, 3, 38, 8, 3, 10, 3, 
		    12, 3, 41, 9, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 3, 47, 8, 3, 1, 4, 1, 
		    4, 1, 4, 1, 4, 1, 5, 1, 5, 1, 5, 1, 5, 5, 5, 57, 8, 5, 10, 5, 12, 
		    5, 60, 9, 5, 1, 5, 1, 5, 1, 5, 1, 5, 3, 5, 66, 8, 5, 1, 6, 1, 6, 1, 
		    6, 1, 7, 1, 7, 1, 7, 3, 7, 74, 8, 7, 1, 7, 1, 7, 3, 7, 78, 8, 7, 1, 
		    7, 0, 0, 8, 0, 2, 4, 6, 8, 10, 12, 14, 0, 0, 87, 0, 16, 1, 0, 0, 0, 
		    2, 19, 1, 0, 0, 0, 4, 31, 1, 0, 0, 0, 6, 46, 1, 0, 0, 0, 8, 48, 1, 
		    0, 0, 0, 10, 65, 1, 0, 0, 0, 12, 67, 1, 0, 0, 0, 14, 77, 1, 0, 0, 
		    0, 16, 17, 3, 2, 1, 0, 17, 18, 5, 0, 0, 1, 18, 1, 1, 0, 0, 0, 19, 
		    21, 3, 4, 2, 0, 20, 22, 3, 12, 6, 0, 21, 20, 1, 0, 0, 0, 21, 22, 1, 
		    0, 0, 0, 22, 3, 1, 0, 0, 0, 23, 32, 5, 15, 0, 0, 24, 32, 5, 16, 0, 
		    0, 25, 32, 3, 14, 7, 0, 26, 32, 5, 1, 0, 0, 27, 32, 5, 2, 0, 0, 28, 
		    32, 5, 3, 0, 0, 29, 32, 3, 6, 3, 0, 30, 32, 3, 10, 5, 0, 31, 23, 1, 
		    0, 0, 0, 31, 24, 1, 0, 0, 0, 31, 25, 1, 0, 0, 0, 31, 26, 1, 0, 0, 
		    0, 31, 27, 1, 0, 0, 0, 31, 28, 1, 0, 0, 0, 31, 29, 1, 0, 0, 0, 31, 
		    30, 1, 0, 0, 0, 32, 5, 1, 0, 0, 0, 33, 34, 5, 4, 0, 0, 34, 39, 3, 
		    8, 4, 0, 35, 36, 5, 5, 0, 0, 36, 38, 3, 8, 4, 0, 37, 35, 1, 0, 0, 
		    0, 38, 41, 1, 0, 0, 0, 39, 37, 1, 0, 0, 0, 39, 40, 1, 0, 0, 0, 40, 
		    42, 1, 0, 0, 0, 41, 39, 1, 0, 0, 0, 42, 43, 5, 6, 0, 0, 43, 47, 1, 
		    0, 0, 0, 44, 45, 5, 4, 0, 0, 45, 47, 5, 6, 0, 0, 46, 33, 1, 0, 0, 
		    0, 46, 44, 1, 0, 0, 0, 47, 7, 1, 0, 0, 0, 48, 49, 5, 15, 0, 0, 49, 
		    50, 5, 7, 0, 0, 50, 51, 3, 2, 1, 0, 51, 9, 1, 0, 0, 0, 52, 53, 5, 
		    8, 0, 0, 53, 58, 3, 2, 1, 0, 54, 55, 5, 5, 0, 0, 55, 57, 3, 2, 1, 
		    0, 56, 54, 1, 0, 0, 0, 57, 60, 1, 0, 0, 0, 58, 56, 1, 0, 0, 0, 58, 
		    59, 1, 0, 0, 0, 59, 61, 1, 0, 0, 0, 60, 58, 1, 0, 0, 0, 61, 62, 5, 
		    9, 0, 0, 62, 66, 1, 0, 0, 0, 63, 64, 5, 8, 0, 0, 64, 66, 5, 9, 0, 
		    0, 65, 52, 1, 0, 0, 0, 65, 63, 1, 0, 0, 0, 66, 11, 1, 0, 0, 0, 67, 
		    68, 5, 10, 0, 0, 68, 69, 5, 14, 0, 0, 69, 13, 1, 0, 0, 0, 70, 78, 
		    5, 17, 0, 0, 71, 78, 5, 18, 0, 0, 72, 74, 5, 11, 0, 0, 73, 72, 1, 
		    0, 0, 0, 73, 74, 1, 0, 0, 0, 74, 75, 1, 0, 0, 0, 75, 78, 5, 12, 0, 
		    0, 76, 78, 5, 13, 0, 0, 77, 70, 1, 0, 0, 0, 77, 71, 1, 0, 0, 0, 77, 
		    73, 1, 0, 0, 0, 77, 76, 1, 0, 0, 0, 78, 15, 1, 0, 0, 0, 8, 21, 31, 
		    39, 46, 58, 65, 73, 77];
		protected static $atn;
		protected static $decisionToDFA;
		protected static $sharedContextCache;

		public function __construct(TokenStream $input)
		{
			parent::__construct($input);

			self::initialize();

			$this->interp = new ParserATNSimulator($this, self::$atn, self::$decisionToDFA, self::$sharedContextCache);
		}

		private static function initialize(): void
		{
			if (self::$atn !== null) {
				return;
			}

			RuntimeMetaData::checkVersion('4.11.1', RuntimeMetaData::VERSION);

			$atn = (new ATNDeserializer())->deserialize(self::SERIALIZED_ATN);

			$decisionToDFA = [];
			for ($i = 0, $count = $atn->getNumberOfDecisions(); $i < $count; $i++) {
				$decisionToDFA[] = new DFA($atn->getDecisionState($i), $i);
			}

			self::$atn = $atn;
			self::$decisionToDFA = $decisionToDFA;
			self::$sharedContextCache = new PredictionContextCache();
		}

		public function getGrammarFileName(): string
		{
			return "Agtype.g4";
		}

		public function getRuleNames(): array
		{
			return self::RULE_NAMES;
		}

		public function getSerializedATN(): array
		{
			return self::SERIALIZED_ATN;
		}

		public function getATN(): ATN
		{
			return self::$atn;
		}

		public function getVocabulary(): Vocabulary
        {
            static $vocabulary;

			return $vocabulary = $vocabulary ?? new VocabularyImpl(self::LITERAL_NAMES, self::SYMBOLIC_NAMES);
        }

		/**
		 * @throws RecognitionException
		 */
		public function agType(): Context\AgTypeContext
		{
		    $localContext = new Context\AgTypeContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 0, self::RULE_agType);

		    try {
		        $this->enterOuterAlt($localContext, 1);
		        $this->setState(16);
		        $this->agValue();
		        $this->setState(17);
		        $this->match(self::EOF);
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function agValue(): Context\AgValueContext
		{
		    $localContext = new Context\AgValueContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 2, self::RULE_agValue);

		    try {
		        $this->enterOuterAlt($localContext, 1);
		        $this->setState(19);
		        $this->value();
		        $this->setState(21);
		        $this->errorHandler->sync($this);
		        $_la = $this->input->LA(1);

		        if ($_la === self::T__9) {
		        	$this->setState(20);
		        	$this->typeAnnotation();
		        }
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function value(): Context\ValueContext
		{
		    $localContext = new Context\ValueContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 4, self::RULE_value);

		    try {
		        $this->setState(31);
		        $this->errorHandler->sync($this);

		        switch ($this->input->LA(1)) {
		            case self::STRING:
		            	$localContext = new Context\StringValueContext($localContext);
		            	$this->enterOuterAlt($localContext, 1);
		            	$this->setState(23);
		            	$this->match(self::STRING);
		            	break;

		            case self::INTEGER:
		            	$localContext = new Context\IntegerValueContext($localContext);
		            	$this->enterOuterAlt($localContext, 2);
		            	$this->setState(24);
		            	$this->match(self::INTEGER);
		            	break;

		            case self::T__10:
		            case self::T__11:
		            case self::T__12:
		            case self::RegularFloat:
		            case self::ExponentFloat:
		            	$localContext = new Context\FloatValueContext($localContext);
		            	$this->enterOuterAlt($localContext, 3);
		            	$this->setState(25);
		            	$this->floatLiteral();
		            	break;

		            case self::T__0:
		            	$localContext = new Context\TrueBooleanContext($localContext);
		            	$this->enterOuterAlt($localContext, 4);
		            	$this->setState(26);
		            	$this->match(self::T__0);
		            	break;

		            case self::T__1:
		            	$localContext = new Context\FalseBooleanContext($localContext);
		            	$this->enterOuterAlt($localContext, 5);
		            	$this->setState(27);
		            	$this->match(self::T__1);
		            	break;

		            case self::T__2:
		            	$localContext = new Context\NullValueContext($localContext);
		            	$this->enterOuterAlt($localContext, 6);
		            	$this->setState(28);
		            	$this->match(self::T__2);
		            	break;

		            case self::T__3:
		            	$localContext = new Context\ObjectValueContext($localContext);
		            	$this->enterOuterAlt($localContext, 7);
		            	$this->setState(29);
		            	$this->obj();
		            	break;

		            case self::T__7:
		            	$localContext = new Context\ArrayValueContext($localContext);
		            	$this->enterOuterAlt($localContext, 8);
		            	$this->setState(30);
		            	$this->array();
		            	break;

		        default:
		        	throw new NoViableAltException($this);
		        }
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function obj(): Context\ObjContext
		{
		    $localContext = new Context\ObjContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 6, self::RULE_obj);

		    try {
		        $this->setState(46);
		        $this->errorHandler->sync($this);

		        switch ($this->getInterpreter()->adaptivePredict($this->input, 3, $this->ctx)) {
		        	case 1:
		        	    $this->enterOuterAlt($localContext, 1);
		        	    $this->setState(33);
		        	    $this->match(self::T__3);
		        	    $this->setState(34);
		        	    $this->pair();
		        	    $this->setState(39);
		        	    $this->errorHandler->sync($this);

		        	    $_la = $this->input->LA(1);
		        	    while ($_la === self::T__4) {
		        	    	$this->setState(35);
		        	    	$this->match(self::T__4);
		        	    	$this->setState(36);
		        	    	$this->pair();
		        	    	$this->setState(41);
		        	    	$this->errorHandler->sync($this);
		        	    	$_la = $this->input->LA(1);
		        	    }
		        	    $this->setState(42);
		        	    $this->match(self::T__5);
		        	break;

		        	case 2:
		        	    $this->enterOuterAlt($localContext, 2);
		        	    $this->setState(44);
		        	    $this->match(self::T__3);
		        	    $this->setState(45);
		        	    $this->match(self::T__5);
		        	break;
		        }
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function pair(): Context\PairContext
		{
		    $localContext = new Context\PairContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 8, self::RULE_pair);

		    try {
		        $this->enterOuterAlt($localContext, 1);
		        $this->setState(48);
		        $this->match(self::STRING);
		        $this->setState(49);
		        $this->match(self::T__6);
		        $this->setState(50);
		        $this->agValue();
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function array(): Context\ArrayContext
		{
		    $localContext = new Context\ArrayContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 10, self::RULE_array);

		    try {
		        $this->setState(65);
		        $this->errorHandler->sync($this);

		        switch ($this->getInterpreter()->adaptivePredict($this->input, 5, $this->ctx)) {
		        	case 1:
		        	    $this->enterOuterAlt($localContext, 1);
		        	    $this->setState(52);
		        	    $this->match(self::T__7);
		        	    $this->setState(53);
		        	    $this->agValue();
		        	    $this->setState(58);
		        	    $this->errorHandler->sync($this);

		        	    $_la = $this->input->LA(1);
		        	    while ($_la === self::T__4) {
		        	    	$this->setState(54);
		        	    	$this->match(self::T__4);
		        	    	$this->setState(55);
		        	    	$this->agValue();
		        	    	$this->setState(60);
		        	    	$this->errorHandler->sync($this);
		        	    	$_la = $this->input->LA(1);
		        	    }
		        	    $this->setState(61);
		        	    $this->match(self::T__8);
		        	break;

		        	case 2:
		        	    $this->enterOuterAlt($localContext, 2);
		        	    $this->setState(63);
		        	    $this->match(self::T__7);
		        	    $this->setState(64);
		        	    $this->match(self::T__8);
		        	break;
		        }
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function typeAnnotation(): Context\TypeAnnotationContext
		{
		    $localContext = new Context\TypeAnnotationContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 12, self::RULE_typeAnnotation);

		    try {
		        $this->enterOuterAlt($localContext, 1);
		        $this->setState(67);
		        $this->match(self::T__9);
		        $this->setState(68);
		        $this->match(self::IDENT);
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}

		/**
		 * @throws RecognitionException
		 */
		public function floatLiteral(): Context\FloatLiteralContext
		{
		    $localContext = new Context\FloatLiteralContext($this->ctx, $this->getState());

		    $this->enterRule($localContext, 14, self::RULE_floatLiteral);

		    try {
		        $this->setState(77);
		        $this->errorHandler->sync($this);

		        switch ($this->input->LA(1)) {
		            case self::RegularFloat:
		            	$this->enterOuterAlt($localContext, 1);
		            	$this->setState(70);
		            	$this->match(self::RegularFloat);
		            	break;

		            case self::ExponentFloat:
		            	$this->enterOuterAlt($localContext, 2);
		            	$this->setState(71);
		            	$this->match(self::ExponentFloat);
		            	break;

		            case self::T__10:
		            case self::T__11:
		            	$this->enterOuterAlt($localContext, 3);
		            	$this->setState(73);
		            	$this->errorHandler->sync($this);
		            	$_la = $this->input->LA(1);

		            	if ($_la === self::T__10) {
		            		$this->setState(72);
		            		$this->match(self::T__10);
		            	}
		            	$this->setState(75);
		            	$this->match(self::T__11);
		            	break;

		            case self::T__12:
		            	$this->enterOuterAlt($localContext, 4);
		            	$this->setState(76);
		            	$this->match(self::T__12);
		            	break;

		        default:
		        	throw new NoViableAltException($this);
		        }
		    } catch (RecognitionException $exception) {
		        $localContext->exception = $exception;
		        $this->errorHandler->reportError($this, $exception);
		        $this->errorHandler->recover($this, $exception);
		    } finally {
		        $this->exitRule();
		    }

		    return $localContext;
		}
	}
}

namespace Apache\AgePhp\Parser\Context {
	use Antlr\Antlr4\Runtime\ParserRuleContext;
	use Antlr\Antlr4\Runtime\Token;
	use Antlr\Antlr4\Runtime\Tree\ParseTreeVisitor;
	use Antlr\Antlr4\Runtime\Tree\TerminalNode;
	use Antlr\Antlr4\Runtime\Tree\ParseTreeListener;
	use Apache\AgePhp\Parser\AgtypeParser;
	use Apache\AgePhp\Parser\AgtypeVisitor;
	use Apache\AgePhp\Parser\AgtypeListener;

	class AgTypeContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_agType;
	    }

	    public function agValue(): ?AgValueContext
	    {
	    	return $this->getTypedRuleContext(AgValueContext::class, 0);
	    }

	    public function EOF(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::EOF, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterAgType($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitAgType($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitAgType($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class AgValueContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_agValue;
	    }

	    public function value(): ?ValueContext
	    {
	    	return $this->getTypedRuleContext(ValueContext::class, 0);
	    }

	    public function typeAnnotation(): ?TypeAnnotationContext
	    {
	    	return $this->getTypedRuleContext(TypeAnnotationContext::class, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterAgValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitAgValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitAgValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class ValueContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_value;
	    }
	 
		public function copyFrom(ParserRuleContext $context): void
		{
			parent::copyFrom($context);

		}
	}

	class NullValueContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterNullValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitNullValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitNullValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class ObjectValueContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

	    public function obj(): ?ObjContext
	    {
	    	return $this->getTypedRuleContext(ObjContext::class, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterObjectValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitObjectValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitObjectValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class IntegerValueContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

	    public function INTEGER(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::INTEGER, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterIntegerValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitIntegerValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitIntegerValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class TrueBooleanContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterTrueBoolean($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitTrueBoolean($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitTrueBoolean($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class FalseBooleanContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterFalseBoolean($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitFalseBoolean($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitFalseBoolean($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class FloatValueContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

	    public function floatLiteral(): ?FloatLiteralContext
	    {
	    	return $this->getTypedRuleContext(FloatLiteralContext::class, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterFloatValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitFloatValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitFloatValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class StringValueContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

	    public function STRING(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::STRING, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterStringValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitStringValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitStringValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	}

	class ArrayValueContext extends ValueContext
	{
		public function __construct(ValueContext $context)
		{
		    parent::__construct($context);

		    $this->copyFrom($context);
	    }

	    public function array(): ?ArrayContext
	    {
	    	return $this->getTypedRuleContext(ArrayContext::class, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterArrayValue($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitArrayValue($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitArrayValue($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class ObjContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_obj;
	    }

	    /**
	     * @return array<PairContext>|PairContext|null
	     */
	    public function pair(?int $index = null)
	    {
	    	if ($index === null) {
	    		return $this->getTypedRuleContexts(PairContext::class);
	    	}

	        return $this->getTypedRuleContext(PairContext::class, $index);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterObj($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitObj($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitObj($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class PairContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_pair;
	    }

	    public function STRING(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::STRING, 0);
	    }

	    public function agValue(): ?AgValueContext
	    {
	    	return $this->getTypedRuleContext(AgValueContext::class, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterPair($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitPair($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitPair($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class ArrayContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_array;
	    }

	    /**
	     * @return array<AgValueContext>|AgValueContext|null
	     */
	    public function agValue(?int $index = null)
	    {
	    	if ($index === null) {
	    		return $this->getTypedRuleContexts(AgValueContext::class);
	    	}

	        return $this->getTypedRuleContext(AgValueContext::class, $index);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterArray($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitArray($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitArray($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class TypeAnnotationContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_typeAnnotation;
	    }

	    public function IDENT(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::IDENT, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterTypeAnnotation($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitTypeAnnotation($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitTypeAnnotation($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 

	class FloatLiteralContext extends ParserRuleContext
	{
		public function __construct(?ParserRuleContext $parent, ?int $invokingState = null)
		{
			parent::__construct($parent, $invokingState);
		}

		public function getRuleIndex(): int
		{
		    return AgtypeParser::RULE_floatLiteral;
	    }

	    public function RegularFloat(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::RegularFloat, 0);
	    }

	    public function ExponentFloat(): ?TerminalNode
	    {
	        return $this->getToken(AgtypeParser::ExponentFloat, 0);
	    }

		public function enterRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->enterFloatLiteral($this);
		    }
		}

		public function exitRule(ParseTreeListener $listener): void
		{
			if ($listener instanceof AgtypeListener) {
			    $listener->exitFloatLiteral($this);
		    }
		}

		public function accept(ParseTreeVisitor $visitor): mixed
		{
			if ($visitor instanceof AgtypeVisitor) {
			    return $visitor->visitFloatLiteral($this);
		    }

			return $visitor->visitChildren($this);
		}
	} 
}