<?php

namespace Apache\AgePhp\Parser;

use Antlr\Antlr4\Runtime\ParserRuleContext;
use Antlr\Antlr4\Runtime\Tree\ErrorNode;
use Antlr\Antlr4\Runtime\Tree\ParseTreeVisitor;
use Antlr\Antlr4\Runtime\Tree\TerminalNode;
use Apache\AgePhp\Parser\Context\AgTypeContext;
use Apache\AgePhp\Parser\Context\ArrayValueContext;
use Apache\AgePhp\Parser\Context\FalseBooleanContext;
use Apache\AgePhp\Parser\Context\FloatLiteralContext;
use Apache\AgePhp\Parser\Context\FloatValueContext;
use Apache\AgePhp\Parser\Context\IntegerValueContext;
use Apache\AgePhp\Parser\Context\NullValueContext;
use Apache\AgePhp\Parser\Context\ObjectValueContext;
use Apache\AgePhp\Parser\Context\PairContext;
use Apache\AgePhp\Parser\Context\StringValueContext;
use Apache\AgePhp\Parser\Context\TrueBooleanContext;
use stdClass;

class CustomAgTypeListener extends AgtypeBaseListener
{
    private $rootObject;
    private $objectInsider = [];
    private $prevObject;
    private $lastObject;
    private $lastValue;

    public function mergeArrayOrObject(string $key): void
    {
        if (is_array($this->prevObject)) {
            $this->mergeArray();
        } else {
            $this->mergeObject($key);
        }
    }

    public function mergeArray(): void
    {
        if (isset($this->prevObject) && isset($this->lastObject) && is_array($this->prevObject)) {
            $this->prevObject[] = $this->lastObject;
            $this->lastObject = &$this->prevObject;
            array_shift($this->objectInsider);
            
            if (isset($this->objectInsider[1])) {
                $this->prevObject = &$this->objectInsider[1];
            } else {
                unset($this->prevObject);
            }
        }
    }

    public function mergeObject(string $key): void
    {
        if (isset($this->prevObject) && isset($this->lastObject) && is_object($this->prevObject)) {
            $this->prevObject->{$key} = $this->lastObject;
            $this->lastObject = &$this->prevObject;
            array_shift($this->objectInsider);

            if (isset($this->objectInsider[1])) {
                $this->prevObject = &$this->objectInsider[1];
            } else {
                unset($this->prevObject);
            }
        }
    }

    public function createNewObject(): void 
    {
        $newObject = new stdClass();
        array_unshift($this->objectInsider, $newObject);
        $this->prevObject = &$this->lastObject;
        $this->lastObject = &$newObject;
    }

    public function createNewArray(): void 
    {
        $newObject = [];
        array_unshift($this->objectInsider, $newObject);
        $this->prevObject = &$this->lastObject;
        $this->lastObject = &$newObject;
    }

    public function pushIfArray($value): bool
    {
        if (is_array($this->lastObject)) {
            $this->lastObject[] = $value;
            return true;
        }

        return false;
    }

    public function exitStringValue(StringValueContext $context): void 
    {
        $value = $this->stripQuotes($context->getText());
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function exitIntegerValue(IntegerValueContext $context): void 
    {
        $value = intval($context->getText());
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function exitFloatValue(FloatValueContext $context): void 
    {
        $value = floatval($context->getText());
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function exitTrueBoolean(TrueBooleanContext $context): void 
    {
        $value = true;
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function exitFalseBoolean(FalseBooleanContext $context): void
    {
        $value = false;
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function exitNullValue(NullValueContext $context): void
    {
        $value = null;
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function exitFloatLiteral(FloatLiteralContext $context): void
    {
        $value = $context->getText();
        if (!$this->pushIfArray($value)) {
            $this->lastValue = $value;
        }
    }

    public function enterObjectValue(ObjectValueContext $context): void
    {
        $this->createNewObject();
    }

    public function enterArrayValue(ArrayValueContext $context): void
    {
        $this->createNewArray();
    }

    public function exitObjectValue(ObjectValueContext $context): void
    {
        $this->mergeArray();
    }

    public function exitPair(PairContext $context): void
    {
        $name = $this->stripQuotes($context->STRING()->getText());

        if (isset($this->lastValue)) {
            $this->lastObject->{$name} = $this->lastValue;
            unset($this->lastValue);
        } else {
            $this->mergeArrayOrObject($name);
        }
    }

    public function exitAgType(AgTypeContext $context): void
    {
        $this->rootObject = array_shift($this->objectInsider);
    }

    public function stripQuotes(string $quotesString): mixed
    {
        return json_decode($quotesString);
    }

    public function getResult(): mixed
    {
        $this->objectInsider = [];
        unset($this->prevObject);
        unset($this->lastObject);
        
        if (!$this->rootObject) {
            if (isset($this->lastValue)) {
                $this->rootObject = $this->lastValue;
            }
        }
        unset($this->lastValue);

        return $this->rootObject;
    }

    public function enterEveryRule(ParserRuleContext $context): void
    {
        
    }

    public function exitEveryRule(ParserRuleContext $context): void
    {
        
    }

    public function visitErrorNode(ErrorNode $node): void
    {
        
    }

    public function visitTerminal(TerminalNode $node): void
    {
        
    }
}