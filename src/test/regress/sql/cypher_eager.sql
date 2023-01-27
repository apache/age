--
-- Cypher Query Language - Eager plan test
--

--
-- prepare
--

CREATE GRAPH eager_graph;
SET GRAPH_PATH = eager_graph;
CREATE VLABEL v1;
CREATE VLABEL v2;
CREATE VLABEL v3 INHERITS (v2);
CREATE VLABEL v4;
CREATE ELABEL e1;

--
-- Eager plan
--
CREATE (:v1 {no: 1}), (:v1 {no: 2}), (:v1 {no: 3});

-- CREATE - CREATE
SET enable_eager = false;

MATCH (a:v1)
CREATE (b:v3 {no: a.no})
CREATE (c:v2 {no: a.no + 2});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- CREATE - MERGE
SET enable_eager = false;

MATCH (a:v1)
CREATE (b:v3 {no: a.no})
MERGE (c:v2 {no: a.no + 2});

SET enable_eager = true;

MATCH (a:v1)
CREATE (b:v3 {no: a.no})
MERGE (c:v2 {no: a.no + 2});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- CREATE - SET
MATCH (a:v1) CREATE (:v2 =properties(a));

SET enable_eager = false;

MATCH (a:v2)
CREATE (:v3 =properties(a))
SET a.no = a.no + 1;

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- CREATE - DELETE
MATCH (a:v1) CREATE (:v2 =properties(a));

SET enable_eager = false;

MATCH (a:v2)
CREATE (:v3 =properties(a))
DELETE a;

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- MERGE - CREATE
SET enable_eager = true;

MATCH (a:v1)
MERGE (b:v2 {no: a.no}) -- variable 'c' is invisible in this clause.
CREATE (c:v3 {no: a.no + 2});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- MERGE - MERGE
SET enable_eager = false;

MATCH (a:v1)
MERGE (b:v3 {no: a.no})
MERGE (c:v2 {no: a.no + 2});

SET enable_eager = true;

MATCH (a:v1)
MERGE (b:v3 {no: a.no})
MERGE (c:v2 {no: a.no + 2});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- MERGE - SET
MATCH (a:v1) CREATE (:v3 =properties(a));

--SET enable_eager = false;

MATCH (a:v2)
MERGE (b:v3 {no:a.no + 2})
SET a.no = a.no + 3;

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- MERGE - SET - MERGE
MATCH (:v1)
MERGE (a:v2 {no:1})
	ON MATCH SET a.cnt = a.cnt + 1
	ON CREATE SET a.cnt = 0
MERGE (b:v2 {cnt:2})
RETURN a=b;

MATCH (a:v2) DETACH DELETE a;

MATCH (a:v1)
MERGE (b:v2 {no:a.no})
	ON MATCH SET b.matched = true
	ON CREATE SET b.created = true
MERGE (c:v2 {no:4 - a.no})
RETURN properties(b), properties(c);

MATCH (a:v1)
MERGE (b:v2 {no:a.no})
	ON MATCH SET b.matched = true, b.created = NULL
	ON CREATE SET b.created = true
MERGE (c:v2 {no:4 - a.no})
RETURN properties(b), properties(c);

MATCH (a:v2) DETACH DELETE a;

-- MERGE - DELETE
MATCH (a:v1) CREATE (:v2 =properties(a));

--SET enable_eager = false;

MATCH (a:v2)
MERGE (b:v2 {no:a.no -1}) -- deleted vertices are invisible in this clause.
DELETE a;

MATCH (a:v2) RETURN a.no;
MATCH (a:v2) DETACH DELETE a;

-- SET - CREATE
-- Modifyiing label that modified in the previous clause
MATCH (a:v1) CREATE (:v3 =properties(a));

--SET enable_eager = false;

MATCH (a:v3)
SET a.no = a.no - 2
CREATE (b:v2 {no: 1});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- Referring to label that modified in the previous clause.
MATCH (a:v1) CREATE (:v3 =properties(a));

SET enable_eager = false;

MATCH (a:v2), (b:v3)
SET a.no = a.no - 2
CREATE (:v3 {no:b.no + 3});

SET enable_eager = true;

MATCH (a:v2), (b:v3)
SET a.no = a.no - 2
CREATE (:v3 {no:b.no + 3});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- SET - MERGE
-- Modifyiing label that modified in the previous clause
MATCH (a:v1) CREATE (:v3 =properties(a));

SET enable_eager = false;

MATCH (a:v3)
SET a.no = a.no - 2
MERGE (b:v2 {no: 1});

SET enable_eager = true;

MATCH (a:v3)
SET a.no = a.no - 2
MERGE (b:v2 {no: 1});

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;

-- Referring to label that modified in the previous clause.
MATCH (a:v1) CREATE (:v3 =properties(a));

SET enable_eager = false;

MATCH (a:v2), (b:v3)
SET a.no = a.no - 2
MERGE (:v4 {no:b.no});

SET enable_eager = true;

MATCH (a:v2), (b:v3)
SET a.no = a.no - 2
MERGE (:v4 {no:b.no});

MATCH (a:v4) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;
MATCH (a:v4) DETACH DELETE a;

-- SET - SET
MATCH (a:v1) CREATE (:v3 =properties(a));
MATCH (a:v1) CREATE (:v4 {no:a.no + 3});

--SET enable_eager = false;

MATCH (a:v2), (b:v3), (c:v4)
SET a.no = a.no - 2
SET c.no = b.no;

MATCH (a:v2) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v4) RETURN label(a) as label, a.no AS no ORDER BY label, no;
MATCH (a:v2) DETACH DELETE a;
MATCH (a:v4) DETACH DELETE a;

-- SET - DELETE
MATCH (a:v1) CREATE (:v3 =properties(a));

--SET enable_eager = false;

MATCH (a:v2), (b:v3)
SET a.no = a.no - 2
DELETE b;

MATCH (a:v2) RETURN a.no AS no ORDER BY no;
MATCH (a:v2) DETACH DELETE a;

-- DELETE - CREATE
-- Modifyiing label that modified in the previous clause
MATCH (a:v1) CREATE (:v3 =properties(a));

--SET enable_eager = false;

MATCH (a:v2)
	WHERE a.no < 3
DELETE a
CREATE (b:v3 {no: 2});

MATCH (a:v2) RETURN a.no AS no ORDER BY no;
MATCH (a:v2) DETACH DELETE a;

-- Referring to label that modified in the previous clause.
MATCH (a:v1) CREATE (:v3 =properties(a));

SET enable_eager = false;

MATCH (a:v2), (b:v3)
	WHERE a.no < 3
DELETE a
CREATE (c:v4 {no: b.no});

SET enable_eager = true;

MATCH (a:v2), (b:v3)
	WHERE a.no < 3
DELETE a
CREATE (c:v4 {no: b.no});

MATCH (a:v4) RETURN a.no;
MATCH (a:v2) DETACH DELETE a;
MATCH (a:v4) DETACH DELETE a;

-- DELETE - MERGE
-- Modifyiing label that modified in the previous clause
MATCH (a:v1) CREATE (:v3 =properties(a));

SET enable_eager = false;

MATCH (a:v2)
	WHERE a.no < 3
DELETE a
MERGE (b:v3 {no: 2});

SET enable_eager = true;

MATCH (a:v2)
	WHERE a.no < 3
DELETE a
MERGE (b:v3 {no: 2});

MATCH (a:v2) RETURN a.no AS no ORDER BY no;
MATCH (a:v2) DETACH DELETE a;

-- Referring to label that modified in the previous clause.
MATCH (a:v1) CREATE (:v3 =properties(a));

SET enable_eager = false;

MATCH (a:v2), (b:v3)
	WHERE a.no < 3
DELETE a
MERGE (c:v4 {no: b.no});

SET enable_eager = true;

MATCH (a:v2), (b:v3)
	WHERE a.no < 3
DELETE a
MERGE (c:v4 {no: b.no});

MATCH (a:v4) RETURN a.no;
MATCH (a:v2) DETACH DELETE a;
MATCH (a:v4) DETACH DELETE a;

-- DELETE - SET
-- Modifyiing label that modified in the previous clause
MATCH (a:v1) CREATE (:v3 =properties(a));

--SET enable_eager = false;

MATCH (a:v2), (b:v3)
	WHERE a.no < 3 AND b.no = 3
DELETE a
SET b.no = 1;

MATCH (a:v2) RETURN a.no AS no ORDER BY no;
MATCH (a:v2) DETACH DELETE a;

-- Referring to label that modified in the previous clause.
MATCH (a:v1) CREATE (:v3 =properties(a));
MATCH (a:v1) CREATE (:v4 =properties(a));

SET enable_eager = false;

MATCH (a:v2), (b:v3), (c:v4)
DELETE a
SET c.no = b.no;

SET enable_eager = true;

MATCH (a:v2), (b:v3), (c:v4)
DELETE a
SET c.no = b.no;

MATCH (a:v4) RETURN a.no;
MATCH (a:v2) DETACH DELETE a;
MATCH (a:v4) DETACH DELETE a;

-- DELETE - DELETE
-- Modifyiing label that modified in the previous clause
MATCH (a:v1) CREATE (:v3 =properties(a));

--SET enable_eager = false;

MATCH (a:v2), (b:v3)
	WHERE a.no < 3 AND b.no = 3
DELETE a
DELETE b;

MATCH (a:v2) RETURN a.no AS no ORDER BY no;
MATCH (a:v2) DETACH DELETE a;

-- MATCH - SET - RETURN
--SET enable_eager = true;

MATCH (a) DETACH DELETE a;
CREATE (:v1 {no: 1})-[:e]->(:v1 {no: 2})-[:e]->(:v1 {no: 3});

MATCH (a)-[]->(b)
  SET b.no = a.no + b.no
  RETURN properties(a) AS a, properties(b) AS b;

MATCH (a)-[]->(b)
  RETURN properties(a) AS a, properties(b) AS b;

MATCH (a)-[]->(b)
  SET b.no = a.no + b.no
  CREATE (:v2 {ano: a.no}), (d:v2 {bno: b.no});

MATCH (a:v2) RETURN properties(a);

-- MATCH - DELETE - RETURN
MATCH (a) DETACH DELETE a;
CREATE (:v1 {no: 1}), (:v1 {no: 2}), (:v1 {no: 3});

MATCH (a), (b) WHERE a.no = 2
  DELETE a
  RETURN properties(a) AS a, properties(b) AS b;
MATCH (a) RETURN properties(a);

-- wrong case
MERGE (a:v1) MERGE (b:v2 {name: a.notexistent});
MERGE (a:v1) MATCH (b:v2 {name: a.name}) RETURN a, b;
MERGE (a:v1) MERGE (b:v2 {name: a.name}) MERGE (a);
MERGE (a)-[r]->(b);
MERGE (a)-[r:e1]->(b) MERGE (a);
MERGE (a)-[r:e1]->(b) MERGE (a)-[r:e1]->(b);
MERGE (a)-[:e1]->(a:v1);
MERGE (=10);
MERGE ()-[:e1 =10]->();

-- cleanup

DROP GRAPH eager_graph CASCADE;
