\pset standard_conforming_strings off
SELECT '\'';
SELECT '''';
SELECT 'A\nBC';
SELECT E'A\nBC';
SELECT U&'d\0061t\+000061'; --error
\pset standard_conforming_strings on
SELECT '\''; -- error
SELECT '''';
SELECT 'A\nBC';
SELECT E'A\nBC';

\pset server_encoding SQL_ASCII
# unicode ident
SELECT * FROM U&"d\0061t\+000061";
# unicode literal
SELECT E'd\u0061t\U00000061';
SELECT U&'d\0061t\+000061';
SELECT U&'d!0061t!+000061' UESCAPE '!';
SELECT E'\u3042\u3044\u3046\u3048\u304a'; -- error
SELECT U&'\3042\3044\3046\3048\304a'; -- error

\pset server_encoding UTF8
# unicode ident
SELECT * FROM U&"d\0061t\+000061";
# unicode literal
SELECT E'd\u0061t\U00000061';
SELECT U&'d\0061t\+000061';
SELECT U&'d!0061t!+000061' UESCAPE '!';
SELECT E'\u3042\u3044\u3046\u3048\u304a';
SELECT U&'\3042\3044\3046\3048\304a';
# surrogate pair
SELECT E'\ud800\udc00';
SELECT U&'\d800\dc00';
