--
-- Cypher Query Language - Expression
--

-- Set up
CREATE GRAPH test_cypher_expr;
SET graph_path = test_cypher_expr;

-- String (jsonb)
RETURN '"', '\"', '\\', '\/', '\b', '\f', '\n', '\r', '\t';

-- Decimal (int4, int8, numeric)
RETURN -2147483648, 2147483647;
RETURN -9223372036854775808, 9223372036854775807;
RETURN -9223372036854775809, 9223372036854775808;
-- Hexadecimal (int4)
RETURN -0x7fffffff, 0x7fffffff;
-- Octal (int4)
RETURN -017777777777, 017777777777;
-- Float (numeric)
RETURN 3.14, -3.14, 6.02E23;

-- true, false, null
RETURN true, false, null;

-- String (text)
RETURN '"'::text, '\"'::text, '\\'::text, '\/'::text,
       '\b'::text, '\f'::text, '\n'::text, '\r'::text, '\t'::text;

-- Parameter - UNKNOWNOID::jsonb (string)
PREPARE tmp AS RETURN $1;
EXECUTE tmp ('"\""');
DEALLOCATE tmp;

-- Parameter - UNKNOWNOID::text
PREPARE tmp AS RETURN $1::text;
EXECUTE tmp ('\"');
DEALLOCATE tmp;

-- ::bool
RETURN ''::jsonb::bool, 0::jsonb::bool, false::jsonb::bool,
       []::bool, {}::bool;
RETURN 's'::jsonb::bool, 1::jsonb::bool, true::jsonb::bool,
       [0]::bool, {p: 0}::bool;

-- List and map literal
RETURN [7, 7.0, '"list\nliteral\"', true, false, NULL, [0, 1, 2], {p: 'p'}];
RETURN {i: 7, r: 7.0, s: '"map\nliteral\"', t: true, f: false, 'z': NULL,
        '\n': '\n', l: [0, 1, 2], o: {p: 'p'}};

-- String concatenation
RETURN '1' + '1', '1' + 1, 1 + '1';

-- Arithmetic operation
RETURN 1 + 1, 1 - 1, 2 * 2, 2 / 2, 2 % 2, 2 ^ 2, +1, -1;

-- List concatenation
RETURN 's' + [], 0 + [], true + [],
       [] + 's', [] + 0, [] + true,
       [0] + [1], [] + {}, {} + [];

-- Invalid expression
RETURN '' + false;
RETURN '' + {};
RETURN 0 + false;
RETURN 0 + {};
RETURN false + '';
RETURN false + 0;
RETURN false + false;
RETURN false + {};
RETURN {} + '';
RETURN {} + 0;
RETURN {} + false;
RETURN {} + {};
RETURN '' - '';
RETURN '' - 0;
RETURN '' - false;
RETURN '' - [];
RETURN '' - {};
RETURN 0 - '';
RETURN 0 - false;
RETURN 0 - [];
RETURN 0 - {};
RETURN false - '';
RETURN false - 0;
RETURN false - false;
RETURN false - [];
RETURN false - {};
RETURN [] - '';
RETURN [] - 0;
RETURN [] - false;
RETURN [] - [];
RETURN [] - {};
RETURN {} - '';
RETURN {} - 0;
RETURN {} - false;
RETURN {} - [];
RETURN {} - {};
RETURN '' * '';
RETURN '' * 0;
RETURN '' * false;
RETURN '' * [];
RETURN '' * {};
RETURN 0 * '';
RETURN 0 * false;
RETURN 0 * [];
RETURN 0 * {};
RETURN false * '';
RETURN false * 0;
RETURN false * false;
RETURN false * [];
RETURN false * {};
RETURN [] * '';
RETURN [] * 0;
RETURN [] * false;
RETURN [] * [];
RETURN [] * {};
RETURN {} * '';
RETURN {} * 0;
RETURN {} * false;
RETURN {} * [];
RETURN {} * {};
RETURN '' / '';
RETURN '' / 0;
RETURN '' / false;
RETURN '' / [];
RETURN '' / {};
RETURN 0 / '';
RETURN 0 / false;
RETURN 0 / [];
RETURN 0 / {};
RETURN false / '';
RETURN false / 0;
RETURN false / false;
RETURN false / [];
RETURN false / {};
RETURN [] / '';
RETURN [] / 0;
RETURN [] / false;
RETURN [] / [];
RETURN [] / {};
RETURN {} / '';
RETURN {} / 0;
RETURN {} / false;
RETURN {} / [];
RETURN {} / {};
RETURN '' % '';
RETURN '' % 0;
RETURN '' % false;
RETURN '' % [];
RETURN '' % {};
RETURN 0 % '';
RETURN 0 % false;
RETURN 0 % [];
RETURN 0 % {};
RETURN false % '';
RETURN false % 0;
RETURN false % false;
RETURN false % [];
RETURN false % {};
RETURN [] % '';
RETURN [] % 0;
RETURN [] % false;
RETURN [] % [];
RETURN [] % {};
RETURN {} % '';
RETURN {} % 0;
RETURN {} % false;
RETURN {} % [];
RETURN {} % {};
RETURN '' ^ '';
RETURN '' ^ 0;
RETURN '' ^ false;
RETURN '' ^ [];
RETURN '' ^ {};
RETURN 0 ^ '';
RETURN 0 ^ false;
RETURN 0 ^ [];
RETURN 0 ^ {};
RETURN false ^ '';
RETURN false ^ 0;
RETURN false ^ false;
RETURN false ^ [];
RETURN false ^ {};
RETURN [] ^ '';
RETURN [] ^ 0;
RETURN [] ^ false;
RETURN [] ^ [];
RETURN [] ^ {};
RETURN {} ^ '';
RETURN {} ^ 0;
RETURN {} ^ false;
RETURN {} ^ [];
RETURN {} ^ {};
RETURN +'';
RETURN +false;
RETURN +[];
RETURN +{};
RETURN -'';
RETURN -false;
RETURN -[];
RETURN -{};

CREATE (:v0 {
  o: {i: 7, r: 7.0, s: '"map\nliteral\"', t: true, f: false, 'z': NULL,
      '\n': '\n'},
  l: [7, 7.0, '"list\nliteral\"', true, false, NULL, [0, 1, 2, 3, 4], {p: 'p'}],
  t: {i: 1, s: 's', b: true, l: [0], o: {p: 'p'}},
  f: {i: 0, s: '', b: false, l: [], o: {}}
});

-- Property access
MATCH (n:v0) RETURN n.o.i, n.o.'i', n.o['i'];
MATCH (n:v0) RETURN n.l[0], n.l[6][0],
                    n.l[6][1..], n.l[6][..3], n.l[6][1..3],
                    n.l[6][-4..], n.l[6][..-2], n.l[6][-4..-2],
                    n.l[6][1..6], n.l[6][-7..-2], n.l[6][1..3][0],
                    n.l[7].p,n.l[7].'p', n.l[7]['p'];

-- Null test
RETURN '' IS NULL, '' IS NOT NULL, NULL IS NULL, NULL IS NOT NULL;
MATCH (n:v0) RETURN n.o.z IS NULL, n.l[5] IS NOT NULL;

-- Boolean
MATCH (n:v0) WHERE n.t.i RETURN COUNT(*);
MATCH (n:v0) WHERE n.t.s RETURN COUNT(*);
MATCH (n:v0) WHERE n.t.b RETURN COUNT(*);
MATCH (n:v0) WHERE n.t.l RETURN COUNT(*);
MATCH (n:v0) WHERE n.t.o RETURN COUNT(*);
MATCH (n:v0) WHERE n.f.i RETURN COUNT(*);
MATCH (n:v0) WHERE n.f.s RETURN COUNT(*);
MATCH (n:v0) WHERE n.f.b RETURN COUNT(*);
MATCH (n:v0) WHERE n.f.l RETURN COUNT(*);
MATCH (n:v0) WHERE n.f.o RETURN COUNT(*);

-- Case expression

CREATE (:v1 {i: -1}), (:v1 {i: 0}), (:v1 {i: 1});

MATCH (n:v1)
RETURN CASE n.i WHEN 0 THEN true ELSE false END,
       CASE WHEN n.i = 0 THEN true ELSE false END;

-- IN expression

MATCH (n:v0) RETURN true IN n.l;
MATCH (n:v0) RETURN 0 IN n.l;
MATCH (n:v0) RETURN NULL IN n.l;
MATCH (n:v0) WITH n.l[0] AS i RETURN [(i IN [0, 1, 2, 3, 4]), true];

CREATE (:v2 {i: 0}), (:v2 {i: 1}), (:v2 {i: 2}), (:v2 {i: 3}),
       (:v2 {i: 4}), (:v2 {i: 5}), (:v2 {i: 6}), (:v2 {i: 7}),
       (:v2 {i: 8}), (:v2 {i: 9}), (:v2 {i: 10}), (:v2 {i: 11}),
       (:v2 {i: 12}), (:v2 {i: 13}), (:v2 {i: 14}), (:v2 {i: 15});
CREATE (:v2 {i: 7, name: 'seven'}), (:v2 {i: 9, name: 'nine'});
CREATE PROPERTY INDEX ON v2 (i);

-- check grammar
RETURN 1 IN 1;
RETURN 1 IN [1];

-- SubLink
CREATE TABLE t1 (i int);
INSERT INTO t1 VALUES (1), (2), (3);
MATCH (n:v2) WHERE n.i IN (SELECT to_jsonb(i) FROM t1)
RETURN count(n);

-- plan : index scan
SET enable_seqscan = off;
EXPLAIN (costs off)
  MATCH (n:v2) WHERE n.i IN [1, 2, 3]
  RETURN n;
EXPLAIN (costs off)
  MATCH (n1:v2 {name: 'seven'}), (n2:v2 {name: 'nine'})
  MATCH (n:v2) WHERE n.i IN [n1.i, 8, n2.i]
  RETURN n;
SET enable_seqscan = on;

-- plan : seq scan
CREATE (:v3 {a: [1, 2, 3, 4, 5, 6, 7, 8, 9]});
EXPLAIN (costs off)
  MATCH (n1:v2), (n2:v3) WHERE n1.i IN n2.a
  RETURN n1;
EXPLAIN (costs off)
  MATCH (n2:v3) WITH n2.a AS a
  MATCH (n1:v2) WHERE n1.i IN a
  RETURN n1;

-- List comprehension
RETURN [x IN [0, 1, 2, 3, 4]];
RETURN [x IN [0, 1, 2, 3, 4] WHERE x % 2 = 0];
RETURN [x IN [0, 1, 2, 3, 4] | x + 1];
RETURN [x IN [0, 1, 2, 3, 4] WHERE x % 2 = 0 | x + 1];
-- nested use of variables
RETURN [x IN [[0], [1]] WHERE length([y IN x]) = 1 | [y IN x]];

-- List predicate functions
RETURN ALL(x in [] WHERE x = 0);
RETURN ALL(x in [0] WHERE x = 0);
RETURN ALL(x in [0, 1, 2, 3, 4] WHERE x = 0);
RETURN ALL(x in [0, 1, 2, 3, 4] WHERE x >= 0);
RETURN ALL(x in [0, 1, 2, 3, 4] WHERE x = 5);
RETURN ANY(x in [] WHERE x = 0);
RETURN ANY(x in [0] WHERE x = 0);
RETURN ANY(x in [0, 1, 2, 3, 4] WHERE x = 0);
RETURN ANY(x in [0, 1, 2, 3, 4] WHERE x >= 0);
RETURN ANY(x in [0, 1, 2, 3, 4] WHERE x = 5);
RETURN NONE(x in [] WHERE x = 0);
RETURN NONE(x in [0] WHERE x = 0);
RETURN NONE(x in [0, 1, 2, 3, 4] WHERE x = 0);
RETURN NONE(x in [0, 1, 2, 3, 4] WHERE x >= 0);
RETURN NONE(x in [0, 1, 2, 3, 4] WHERE x = 5);
RETURN SINGLE(x in [] WHERE x = 0);
RETURN SINGLE(x in [0] WHERE x = 0);
RETURN SINGLE(x in [0, 1, 2, 3, 4] WHERE x = 0);
RETURN SINGLE(x in [0, 1, 2, 3, 4] WHERE x >= 0);
RETURN SINGLE(x in [0, 1, 2, 3, 4] WHERE x = 5);

-- Functions

CREATE (:coll {name: 'Pg_Graph'});
MATCH (n:coll) SET n.l = tolower(n.name);
MATCH (n:coll) SET n.u = toupper(n.name);
MATCH (n:coll) RETURN n;

-- Text matching

CREATE (:ts {v: 'a fat cat sat on a mat and ate a fat rat'::tsvector});
MATCH (n:ts) WHERE n.v::tsvector @@ 'cat & rat'::tsquery RETURN n;

-- Tear down
DROP TABLE t1;
DROP GRAPH test_cypher_expr CASCADE;
