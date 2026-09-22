<?php

/*
 * Generated from work\webdev\apacheage\age\drivers\php\src\parser\Agtype.g4 by ANTLR 4.11.1
 */
namespace Apache\AgePhp\Parser;

use Antlr\Antlr4\Runtime\Tree\ParseTreeVisitor;

/**
 * This interface defines a complete generic visitor for a parse tree produced by {@see AgtypeParser}.
 */
interface AgtypeVisitor extends ParseTreeVisitor
{
	/**
	 * Visit a parse tree produced by {@see AgtypeParser::agType()}.
	 *
	 * @param Context\AgTypeContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitAgType(Context\AgTypeContext $context);

	/**
	 * Visit a parse tree produced by {@see AgtypeParser::agValue()}.
	 *
	 * @param Context\AgValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitAgValue(Context\AgValueContext $context);

	/**
	 * Visit a parse tree produced by the `StringValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\StringValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitStringValue(Context\StringValueContext $context);

	/**
	 * Visit a parse tree produced by the `IntegerValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\IntegerValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitIntegerValue(Context\IntegerValueContext $context);

	/**
	 * Visit a parse tree produced by the `FloatValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\FloatValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitFloatValue(Context\FloatValueContext $context);

	/**
	 * Visit a parse tree produced by the `TrueBoolean` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\TrueBooleanContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitTrueBoolean(Context\TrueBooleanContext $context);

	/**
	 * Visit a parse tree produced by the `FalseBoolean` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\FalseBooleanContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitFalseBoolean(Context\FalseBooleanContext $context);

	/**
	 * Visit a parse tree produced by the `NullValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\NullValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitNullValue(Context\NullValueContext $context);

	/**
	 * Visit a parse tree produced by the `ObjectValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\ObjectValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitObjectValue(Context\ObjectValueContext $context);

	/**
	 * Visit a parse tree produced by the `ArrayValue` labeled alternative
	 * in {@see AgtypeParser::value()}.
	 *
	 * @param Context\ArrayValueContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitArrayValue(Context\ArrayValueContext $context);

	/**
	 * Visit a parse tree produced by {@see AgtypeParser::obj()}.
	 *
	 * @param Context\ObjContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitObj(Context\ObjContext $context);

	/**
	 * Visit a parse tree produced by {@see AgtypeParser::pair()}.
	 *
	 * @param Context\PairContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitPair(Context\PairContext $context);

	/**
	 * Visit a parse tree produced by {@see AgtypeParser::array()}.
	 *
	 * @param Context\ArrayContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitArray(Context\ArrayContext $context);

	/**
	 * Visit a parse tree produced by {@see AgtypeParser::typeAnnotation()}.
	 *
	 * @param Context\TypeAnnotationContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitTypeAnnotation(Context\TypeAnnotationContext $context);

	/**
	 * Visit a parse tree produced by {@see AgtypeParser::floatLiteral()}.
	 *
	 * @param Context\FloatLiteralContext $context The parse tree.
	 *
	 * @return mixed The visitor result.
	 */
	public function visitFloatLiteral(Context\FloatLiteralContext $context);
}