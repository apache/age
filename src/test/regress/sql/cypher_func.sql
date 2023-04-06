--
-- vertex_labels()
--

-- prepare
DROP GRAPH IF EXISTS vertex_labels_simple CASCADE;
DROP GRAPH IF EXISTS vertex_labels_complex1 CASCADE;
DROP GRAPH IF EXISTS vertex_labels_complex2 CASCADE;
CREATE GRAPH vertex_labels_simple;
CREATE GRAPH vertex_labels_complex1;
CREATE GRAPH vertex_labels_complex2;

-- simple test

SET graph_path = vertex_labels_simple;

--         a
--         |
-- b       c
-- |       |
-- +-- d --+
CREATE VLABEL a;
CREATE VLABEL b;
CREATE VLABEL c INHERITS (a);
CREATE VLABEL d INHERITS (b, c);

CREATE (:a {name: 'a'});
CREATE (:b {name: 'b'});
CREATE (:c {name: 'c'});
CREATE (:d {name: 'd'});

MATCH (n) RETURN n.name, label(n);

MATCH (n) RETURN n.name, labels(n);
MATCH (n) RETURN n.name, labels(n)[0];
MATCH (n:c) RETURN n.name, labels(n)[1];
MATCH (n:d) RETURN n.name, labels(n)[2], labels(n)[3];

-- complex test 1

SET graph_path = vertex_labels_complex1;

--             a
--             |
--             b       c
--             |       |
-- +-- d --+   +-- e --+   f   +-- g
-- |   |   |       |       |   |   |
-- h   i   j       +------ k --+   |
--     |   |               |       |
--     +---+------ l ------+-------+
CREATE VLABEL a;
CREATE VLABEL b INHERITS (a);
CREATE VLABEL c;
CREATE VLABEL d;
CREATE VLABEL e INHERITS (b, c);
CREATE VLABEL f;
CREATE VLABEL g;
CREATE VLABEL h INHERITS (d);
CREATE VLABEL i INHERITS (d);
CREATE VLABEL j INHERITS (d);
CREATE VLABEL k INHERITS (e, f, g);
CREATE VLABEL l INHERITS (i, j, k, g);

CREATE (:a {name: 'a'});
CREATE (:b {name: 'b'});
CREATE (:c {name: 'c'});
CREATE (:d {name: 'd'});
CREATE (:e {name: 'e'});
CREATE (:f {name: 'f'});
CREATE (:g {name: 'g'});
CREATE (:h {name: 'h'});
CREATE (:i {name: 'i'});
CREATE (:j {name: 'j'});
CREATE (:k {name: 'k'});
CREATE (:l {name: 'l'});

MATCH (n) RETURN n.name, label(n), labels(n);

-- complex test 2

SET graph_path = vertex_labels_complex2;

-- +-- a ----------+
-- |   |       b   |
-- |   |       |   |
-- |   +-- d --+   |
-- |       |       |
-- |       e --+-- f
-- |           |
-- +-- c       g
--     |       |
--     +-- h --+-- i
--         |       |
--         +-- j --+
CREATE VLABEL a;
CREATE VLABEL b;
CREATE VLABEL c INHERITS (a);
CREATE VLABEL d INHERITS (a, b);
CREATE VLABEL e INHERITS (d);
CREATE VLABEL f INHERITS (a);
CREATE VLABEL g INHERITS (e, f);
CREATE VLABEL h INHERITS (c, g);
CREATE VLABEL i INHERITS (g);
CREATE VLABEL j INHERITS (h, i);

CREATE (:a {name: 'a'});
CREATE (:b {name: 'b'});
CREATE (:c {name: 'c'});
CREATE (:d {name: 'd'});
CREATE (:e {name: 'e'});
CREATE (:f {name: 'f'});
CREATE (:g {name: 'g'});
CREATE (:h {name: 'h'});
CREATE (:i {name: 'i'});
CREATE (:j {name: 'j'});

MATCH (n) RETURN n.name, label(n), labels(n);

-- cleanup
DROP GRAPH vertex_labels_complex2 CASCADE;
DROP GRAPH vertex_labels_complex1 CASCADE;
DROP GRAPH vertex_labels_simple CASCADE;

-- Added test for AG249, use ln() for all log() calls
-- Create initial graph
CREATE GRAPH ag249_log_to_ln;
SET graph_path = ag249_log_to_ln;
CREATE VLABEL numbers;
CREATE (:numbers {string: '10', numeric: 10});

-- These should fail as there is no rule to cast from string to numeric
MATCH (u:numbers) RETURN log(u.string);
MATCH (u:numbers) RETURN ln(u.string);
MATCH (u:numbers) RETURN log10(u.string);

-- Check that log() == ln() != log10
MATCH (u:numbers) RETURN log(u.numeric), ln(u.numeric), log10(u.numeric);

-- Check with a string constant
RETURN log('10'), ln('10'), log10('10');

-- Check with a numeric constant;
RETURN log(10), ln(10), log10(10);

-- Check hybrid query
return log10(10), (select log(10));

-- cleanup
DROP GRAPH ag249_log_to_ln CASCADE;

-- Added tests for AG222. These tests test the new function
-- get_last_graph_write_stats(). Which is provided to allow
-- access to statistics on the last graph write operation.
CREATE GRAPH ag222;
SET graph_path = ag222;

CREATE VLABEL vertices;
CREATE ELABEL edges;

-- Should return only 2 inserted vertices and 1 inserted edge
create (:vertices {name: 'Boston'})-[:edges]->(:vertices {name: 'Miami'});
SELECT * FROM get_last_graph_write_stats();

-- Should return only 2 updated properties
MATCH (u:vertices) SET u.property = true;
SELECT * FROM get_last_graph_write_stats();

-- Should return only 1 deleted edge
match (u:vertices)-[e:edges]-() delete e;
SELECT * FROM get_last_graph_write_stats();

-- Should return only 2 deleted vertices
match (u) delete u;
SELECT * FROM get_last_graph_write_stats();

-- cleanup
DROP GRAPH ag222 CASCADE;

-- Added test for AG-283
CREATE GRAPH AG283;

CREATE ({name: 'arc 0', degree: 0});
MATCH (v) RETURN radians(v.degree);

-- cleanup
DROP GRAPH AG283 CASCADE;


-- fix: array_tail did not work with integer type
-- test 1: integer type
SELECT tail(ARRAY[12,13,14,15,16]);
-- test 2: null values
SELECT tail(ARRAY['hi',null,null,'bye']);

-- toFloatList (array)
-- test 1: text
select toFloatList(ARRAY['12.5','-14.788','-0.004']);
-- test 2: integer type
select toFloatList(ARRAY[12,13,14,15]);
-- test 3: null and float type
select toFloatList(ARRAY[14.15, null, -45.78, null, null, 0.02]);
-- test 4: null and boolean type
select toFloatList(ARRAY[true, true, null]);

-- toFloatList (jsonb)
-- test 1: text
return toFloatList(['12.5','-14.788','-0.004']);
-- test 2: integer type
return toFloatList([12,13,14,15]);
-- test 3: null and float type
return toFloatList([14.15, null, -45.78, null, null, 0.02]);
-- test 4: null and boolean type
return toFloatList([true, true, null]);

-- Tests for toBoolean()
-- test Case-1 (Integer - Accepted Input(1 or 0) ):
return toBoolean(1);
-- test Case-2(Integer - Unaccepted value):
return toBoolean(12);

-- test Case-3(Text - accepted input (‘true’ or ‘false’)):
return toBoolean('true');
-- test Case-4(Text - unaccepted values):
return str_size('agensgraph'), toBoolean('agensgraph');

-- test Case-5(Unaccepted types):
return toBoolean(1.0);

---
-- Tests for toBooleanOrNull()
-- test Case-1 (Integer - Accepted Input(1 or 0) ):
return toBooleanOrNull(1);
-- test Case-2(Integer - Unaccepted value):
return toBooleanOrNull(12);
-- test Case-3(Text - accepted input (‘true’ or ‘false’)):
return toBooleanOrNull('true');
-- test Case-4(Text - unaccepted values):
return str_size('agensgraph'), toBooleanOrNull('agensgraph');
-- test Case-5(Unaccepted types):
return toBooleanOrNull(1.0);

-- Tests for toStringOrNull()
-- test Case-1 (Integers):
return toStringOrNull(123) as strornull;
-- test Case-2 (Float):
return toStringOrNull(123.5) as strornull;
-- test Case-3(Timestamp):
return toStringOrNull(datetime(2020,5,26,10,45,56,0,0,'GMT')) as strornull;

--Tests for toStringList()
-- test Case-1 (Integers and floats):
select toStringList(ARRAY[12,13.0,14.15,16,-17.0]);
-- test Case-2 (Bool and null):
select toStringList(ARRAY[true, null, null, false]);
-- test Case-3 (Timestamp):
select toStringList(ARRAY[datetime(2020,5,26,10,45,56,14,0,'GMT'),datetime(2017,2,13,10,52,10,87,10,'JST')]);