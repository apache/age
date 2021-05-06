grammar Agtype;

agType
  : agValue EOF
  ;

agValue
  : value typeAnnotation?
  ;

value
  : STRING #StringValue
  | INTEGER #IntegerValue
  | floatLiteral #FloatValue
  | 'true' #TrueBoolean
  | 'false' #FalseBoolean
  | 'null' #NullValue
  | obj #ObjectValue
  | array #ArrayValue
  ;

obj
  : '{' pair (',' pair)* '}'
  | '{' '}'
  ;

pair
  : STRING ':' agValue
  ;

array
  : '[' agValue (',' agValue)* ']'
  | '[' ']'
  ;

typeAnnotation
  : '::' IDENT
  ;

IDENT
  : [A-Z_a-z][$0-9A-Z_a-z]*
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

INTEGER
  : '-'? INT
  ;

fragment INT
  : '0' | [1-9] [0-9]*
  ;

floatLiteral
  : RegularFloat
  | ExponentFloat
  | '-'? 'Infinity'
  | 'NaN'
  ;

RegularFloat
  : '-'? INT DECIMAL
  ;

ExponentFloat
  : '-'? INT DECIMAL? SCIENTIFIC
  ;

fragment DECIMAL
  : '.' [0-9]+
  ;

fragment SCIENTIFIC
  : [Ee][+-]? [0-9]+
  ;

WS
  : [ \t\n\r] + -> skip
  ;
