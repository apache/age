/* Apache AGE output data grammer */

grammar age;

ageout
   : value
   | vertex
   | edge
   | path
   ;

vertex
   : properties KW_VERTEX
   ;

edge
   : properties KW_EDGE
   ;

path
   : '[' vertex (',' edge ',' vertex)* ']' KW_PATH
   ;

//Keywords
KW_VERTEX : '::vertex';
KW_EDGE : '::edge';
KW_PATH : '::path';

// Common Values Rule
value
   : STRING
   | NUMBER
   | properties
   | arr
   | 'true'
   | 'false'
   | 'null'
   ;

properties
   : '{' pair (',' pair)* '}'
   | '{' '}'
   ;

pair
   : STRING ':' value
   ;

arr
   : '[' value (',' value)* ']'
   | '[' ']'
   ;

STRING
   : '"' (ESC | SAFECODEPOINT)* '"'
   ;


fragment ESC
   : '\\' (["\\/bfnrt] | UNICODE)
   ;
fragment UNICODE
   : 'u' HEX HEX HEX HEX
   ;
fragment HEX
   : [0-9a-fA-F]
   ;
fragment SAFECODEPOINT
   : ~ ["\\\u0000-\u001F]
   ;


NUMBER
   : '-'? INT ('.' [0-9] +)? EXP?
   ;


fragment INT
   : '0' | [1-9] [0-9]*
   ;

// no leading zeros

fragment EXP
   : [Ee] [+\-]? INT
   ;

// \- since - means "range" inside [...]

WS
   : [ \t\n\r] + -> skip
   ;