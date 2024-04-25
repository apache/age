--
-- pg_graph catalog ag_graphmeta test
--
CREATE GRAPH graphmeta;
SET graph_path = graphmeta;
SET auto_gather_graphmeta = true;

-- create edge

CREATE (:human)-[:know]->(:human {age:1});
MERGE (:human)-[:know]->(:human {age:2});
MERGE (:human)-[:know]->(:human {age:3});
CREATE (:dog)-[:follow]->(:human);
CREATE (:dog)-[:likes]->(:dog);

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- create multiple edges

CREATE (:human)-[:know]->(:human)-[:follow]->(:human)-[:hate]->(:human)-[:love]->(:human);
CREATE (:human)-[:know]->(:human)-[:follow]->(:human)-[:hate]->(:human)-[:love]->(:human);
CREATE (:human)-[:know]->(:human)-[:follow]->(:human)-[:hate]->(:human)-[:love]->(:human);

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- create repeated edges;

CREATE (:human)-[:know]->(:human)-[:know]->(:human)-[:know]->(:human)-[:know]->(:human);

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- delete edge

MATCH (a)-[r:love]->(b)
DELETE r;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- drop elabel

DROP ELABEL hate CASCADE;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- drop vlabel

DROP VLABEL human CASCADE;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- drop graph

DROP GRAPH graphmeta CASCADE;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- Sub Transaction
CREATE GRAPH graphmeta;

-- Before Commit;
BEGIN;
	CREATE (:dog)-[:follow]->(:human);
	SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";
COMMIT;

-- Rollback
BEGIN;
	CREATE (:dog)-[:follow]->(:human);
	SAVEPOINT sv1;
	CREATE (:dog)-[:likes]->(:cat);
	ROLLBACK TO SAVEPOINT sv1;
	CREATE (:human)-[:love]->(:dog);
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

MATCH (a) DETACH DELETE a;

BEGIN;
	CREATE (:dog)-[:follow]->(:human);
	SAVEPOINT sv1;
	CREATE (:dog)-[:likes]->(:cat);
	SAVEPOINT sv2;
	CREATE (:dog)-[:likes]->(:ball);
	ROLLBACK TO SAVEPOINT sv1;
	CREATE (:human)-[:love]->(:dog);
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

MATCH (a) DETACH DELETE a;

-- Release
BEGIN;
	CREATE (:dog)-[:follow]->(:human);
	SAVEPOINT sv1;
	CREATE (:dog)-[:likes]->(:cat);
	RELEASE SAVEPOINT sv1;
	CREATE (:human)-[:love]->(:dog);
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

MATCH (a) DETACH DELETE a;

-- RELEASE and ROLLBACK
BEGIN;
	CREATE (:dog)-[:follow]->(:human);
	SAVEPOINT sv1;
	CREATE (:dog)-[:likes]->(:cat);
	SAVEPOINT sv2;
	CREATE (:dog)-[:likes]->(:ball);
	RELEASE SAVEPOINT sv2;
	ROLLBACK TO SAVEPOINT sv1;
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

MATCH (a) DETACH DELETE a;

-- ROLLBACK and RELEASE error
BEGIN;
	CREATE (:dog)-[:follow]->(:human);
	SAVEPOINT sv1;
	CREATE (:dog)-[:likes]->(:cat);
	SAVEPOINT sv2;
	CREATE (:dog)-[:likes]->(:ball);
	RELEASE SAVEPOINT sv1;
	ROLLBACK TO SAVEPOINT sv2;
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- If main transcantion was READ ONLY
BEGIN;
	SAVEPOINT sv1;
	CREATE (:dog)-[:likes]->(:cat);
	ROLLBACK TO SAVEPOINT sv1;
	CREATE (:human)-[:love]->(:dog);
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

BEGIN;
	SAVEPOINT sv1;
	SAVEPOINT sv2;
	CREATE (:dog)-[:likes]->(:cat);
	RELEASE SAVEPOINT sv2;
	ROLLBACK TO SAVEPOINT sv1;
	CREATE (:human)-[:love]->(:dog);
COMMIT;

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- regather_graphmeta()
MATCH (a) DETACH DELETE a;
SET auto_gather_graphmeta = false;

CREATE (:human)-[:know]->(:human {age:1});
MERGE (:human)-[:know]->(:human {age:2});
MERGE (:human)-[:know]->(:human {age:3});
CREATE (:dog)-[:follow]->(:human);
CREATE (:dog)-[:likes]->(:dog);

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

SELECT regather_graphmeta();

SELECT * FROM ag_graphmeta_view ORDER BY start, edge, "end";

-- cleanup

DROP GRAPH graphmeta CASCADE;
