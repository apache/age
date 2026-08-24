LOAD 'age';
SET search_path = ag_catalog, public;

SELECT create_graph('dbg_test2');

SELECT * FROM cypher('dbg_test2', $$
CREATE (a:base {id: 1})-[r1:r {k0: false}]->
       (n:base {id: 2})-[r0:s {k5: 'x'}]->(b:base {id: 3})
$$) AS (created agtype);

SELECT * FROM cypher('dbg_test2', $$
MATCH (a:base)-[r1:r]->(n:base)-[r0:s]->(b:base)
SET r0.k5 = 'J', r1.k0 = true
WITH range(0, 0) + [0] AS keep, r1 AS old
WHERE 0 IN keep
DELETE old
CREATE (:x)
MERGE p0 = ({id: 129})-[r2:r]->(n2 {id: 130})-[:s]->(:c {id: 131})
RETURN 1
$$) AS (value agtype);

SELECT * FROM cypher('dbg_test2', $$ RETURN 1 $$) AS (v agtype);