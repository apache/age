<?php

/*
 * Generated from work\webdev\apacheage\age\drivers\php\src\parser\Agtype.g4 by ANTLR 4.11.1
 */

namespace Apache\AgePhp\Parser;

use Antlr\Antlr4\Runtime\Tree\ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@see AgtypeParser}.
 */
interface AgtypeListener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::agType()}.
	 * @param $context The parse tree.
	 */
	public function enterAgType(Context\AgTypeContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::agType()}.
	 * @param $context The parse tree.
	 */
	public function exitAgType(Context\AgTypeContext $context): void;
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::agValue()}.
	 * @param $context The parse tree.
	 */
	public function enterAgValue(Context\AgValueContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::agValue()}.
	 * @param $context The parse tree.
	 */
	public function exitAgValue(Context\AgValueContext $context): void;
	/**
	 * Enter a parse tree produced by the `StringValue`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterStringValue(Context\StringValueContext $context): void;
	/**
	 * Exit a parse tree produced by the `StringValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitStringValue(Context\StringValueContext $context): void;
	/**
	 * Enter a parse tree produced by the `IntegerValue`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterIntegerValue(Context\IntegerValueContext $context): void;
	/**
	 * Exit a parse tree produced by the `IntegerValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitIntegerValue(Context\IntegerValueContext $context): void;
	/**
	 * Enter a parse tree produced by the `FloatValue`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterFloatValue(Context\FloatValueContext $context): void;
	/**
	 * Exit a parse tree produced by the `FloatValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitFloatValue(Context\FloatValueContext $context): void;
	/**
	 * Enter a parse tree produced by the `TrueBoolean`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterTrueBoolean(Context\TrueBooleanContext $context): void;
	/**
	 * Exit a parse tree produced by the `TrueBoolean` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitTrueBoolean(Context\TrueBooleanContext $context): void;
	/**
	 * Enter a parse tree produced by the `FalseBoolean`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterFalseBoolean(Context\FalseBooleanContext $context): void;
	/**
	 * Exit a parse tree produced by the `FalseBoolean` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitFalseBoolean(Context\FalseBooleanContext $context): void;
	/**
	 * Enter a parse tree produced by the `NullValue`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterNullValue(Context\NullValueContext $context): void;
	/**
	 * Exit a parse tree produced by the `NullValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitNullValue(Context\NullValueContext $context): void;
	/**
	 * Enter a parse tree produced by the `ObjectValue`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterObjectValue(Context\ObjectValueContext $context): void;
	/**
	 * Exit a parse tree produced by the `ObjectValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitObjectValue(Context\ObjectValueContext $context): void;
	/**
	 * Enter a parse tree produced by the `ArrayValue`
	 * labeled alternative in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function enterArrayValue(Context\ArrayValueContext $context): void;
	/**
	 * Exit a parse tree produced by the `ArrayValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 * @param $context The parse tree.
	 */
	public function exitArrayValue(Context\ArrayValueContext $context): void;
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::obj()}.
	 * @param $context The parse tree.
	 */
	public function enterObj(Context\ObjContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::obj()}.
	 * @param $context The parse tree.
	 */
	public function exitObj(Context\ObjContext $context): void;
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::pair()}.
	 * @param $context The parse tree.
	 */
	public function enterPair(Context\PairContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::pair()}.
	 * @param $context The parse tree.
	 */
	public function exitPair(Context\PairContext $context): void;
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::array()}.
	 * @param $context The parse tree.
	 */
	public function enterArray(Context\ArrayContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::array()}.
	 * @param $context The parse tree.
	 */
	public function exitArray(Context\ArrayContext $context): void;
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::typeAnnotation()}.
	 * @param $context The parse tree.
	 */
	public function enterTypeAnnotation(Context\TypeAnnotationContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::typeAnnotation()}.
	 * @param $context The parse tree.
	 */
	public function exitTypeAnnotation(Context\TypeAnnotationContext $context): void;
	/**
	 * Enter a parse tree produced by {@see AgtypeParser::floatLiteral()}.
	 * @param $context The parse tree.
	 */
	public function enterFloatLiteral(Context\FloatLiteralContext $context): void;
	/**
	 * Exit a parse tree produced by {@see AgtypeParser::floatLiteral()}.
	 * @param $context The parse tree.
	 */
	public function exitFloatLiteral(Context\FloatLiteralContext $context): void;
}