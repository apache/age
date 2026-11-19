<?php 

namespace Apache\AgePhp\AgeClient;

use Apache\AgePhp\Parser\AgtypeLexer;
use Apache\AgePhp\Parser\AgtypeParser;
use Antlr\Antlr4\Runtime\CommonTokenStream;
use Antlr\Antlr4\Runtime\InputStream;
use Antlr\Antlr4\Runtime\Tree\ParseTreeWalker;
use Apache\AgePhp\Parser\CustomAgTypeListener;

class AGTypeParse
{
    public static function parse(string $string): mixed
    {
        $charStream = InputStream::fromString($string);
        $lexer = new AgtypeLexer($charStream);
        $tokens = new CommonTokenStream($lexer);
        $parser = new AgtypeParser($tokens);
        $tree = $parser->agType();
        $printer = new CustomAgTypeListener();
        ParseTreeWalker::default()->walk($printer, $tree);

        return $printer->getResult();
    }
}