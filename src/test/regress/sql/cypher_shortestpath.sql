--
-- Cypher Query Language - Shortestpath
--

-- prepare
DROP GRAPH sp CASCADE;
CREATE GRAPH sp;
-- shortestpath(), allshortestpaths()
SET graph_path = sp;

CREATE OR REPLACE FUNCTION ids(vertex[]) RETURNS int[] AS $$
DECLARE
  v vertex;
  vids int[];
BEGIN
  IF $1 IS NULL THEN
    RETURN ARRAY[]::int[];
  END IF;
  FOREACH v IN ARRAY $1 LOOP
    vids = array_append(vids, (v->>'id')::int);
  END LOOP;
  RETURN vids;
END;
$$ LANGUAGE plpgsql;

-- 1->2->3->4->5
CREATE	(:person {id: 1})-[:knows]->
		(:person {id: 2})-[:knows]->
		(:person {id: 3})-[:knows]->
		(:person {id: 4})-[:knows]->
		(:person {id: 5})-[:knows]->
		(:person {id: 6});

MATCH (p:person), (f:person) WHERE p.id = 3 AND f.id = 4
RETURN ids(nodes(shortestpath((p)-[:knows]->(f)))) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 3 AND f.id = 5
RETURN ids(nodes(shortestpath((p)-[:knows]->(f)))) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 3
RETURN ids(nodes(shortestpath((p)<-[:knows]-(f)))) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 3
RETURN ids(nodes(shortestpath((p)-[:knows*]->(f)))) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 3
RETURN ids(nodes(shortestpath((p)<-[:knows*]-(f)))) AS ids;

MATCH (p:person), (f:person), x=shortestpath((p)<-[:knows*]-(f))
WHERE p.id = 3
RETURN ids(nodes(x)) AS ids;

MATCH (p:person), (f:person), x=shortestpath((p)-[:knows*]->(f))
WHERE p.id = 3
RETURN ids(nodes(x)) AS ids;

MATCH x=shortestpath((p:person)-[:knows*]->(f:person))
WHERE p.id = 3
RETURN ids(nodes(x)) AS ids;

MATCH x=shortestpath((p:person)<-[:knows*]-(f:person))
WHERE p.id = 3
RETURN ids(nodes(x)) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 3
RETURN ids(nodes(shortestpath((p)-[:knows*0..1]-(f)))) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 1
RETURN ids(nodes(shortestpath((p)-[:knows*2..]->(f)))) AS ids;

MATCH (p:person), (f:person) WHERE p.id = 3 AND f.id = 5
CREATE (p)-[:knows]->(:person {id: 6})-[:knows]->(f);

MATCH (p:person), (f:person) WHERE p.id = 1 AND f.id = 5
RETURN length(allshortestpaths((p)-[:knows*]-(f))) AS cnt;

CREATE VLABEL v;
CREATE ELABEL e;

CREATE (:v {id: 0});
CREATE (:v {id: 1});
CREATE (:v {id: 2});
CREATE (:v {id: 3});
CREATE (:v {id: 4});
CREATE (:v {id: 5});
CREATE (:v {id: 6});

MATCH (v1:v {id: 0}), (v2:v {id: 4})
CREATE (v1)-[:e {weight: 3}]->(v2);
MATCH (v1:v {id: 0}), (v2:v {id: 1})
CREATE (v1)-[:e {weight: 7}]->(v2);
MATCH (v1:v {id: 0}), (v2:v {id: 5})
CREATE (v1)-[:e {weight: 10}]->(v2);

MATCH (v1:v {id: 4}), (v2:v {id: 6})
CREATE (v1)-[:e {weight: 5}]->(v2);
MATCH (v1:v {id: 4}), (v2:v {id: 3})
CREATE (v1)-[:e {weight: 11}]->(v2);
MATCH (v1:v {id: 4}), (v2:v {id: 1})
CREATE (v1)-[:e {weight: 2}]->(v2);

MATCH (v1:v {id: 1}), (v2:v {id: 3})
CREATE (v1)-[:e {weight: 10}]->(v2);
MATCH (v1:v {id: 1}), (v2:v {id: 2})
CREATE (v1)-[:e {weight: 4}]->(v2);
MATCH (v1:v {id: 1}), (v2:v {id: 5})
CREATE (v1)-[:e {weight: 6}]->(v2);

MATCH (v1:v {id: 5}), (v2:v {id: 3})
CREATE (v1)-[:e {weight: 9}]->(v2);

MATCH (v1:v {id: 6}), (v2:v {id: 3})
CREATE (v1)-[:e {weight: 4}]->(v2);

MATCH (v1:v {id: 2}), (v2:v {id: 3})
CREATE (v1)-[:e {weight: 2}]->(v2);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1)-[e:e]->(v2), e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v2)<-[e:e]-(v1), e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1)-[e:e]-(v2), e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v2)-[e:e]-(v1), e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1)-[e:e]->(v2), e.weight + 1)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1)-[e:e]->(v2), e.weight, e.weight >= 5)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1)-[e:e]->(v2), e.weight, e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1:v {id: 0})-[e:e]->(v2), e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      path=dijkstra((v1:v)-[e:e*1..]->(v2), e.weight)
RETURN nodes(path);

MATCH (v1:v {id: 6}), (v2:v {id: 2}),
      path=dijkstra((v1:v)-[e:e]->(v2), e.weight)
RETURN nodes(path);

MATCH (v1:v), (v2:v),
      path=dijkstra((v1:v)-[e:e]->(v2), e.weight)
WHERE v1.id = 6 AND v2.id = 2
RETURN nodes(path);

MATCH (v1:v {id: 0}), (v2:v {id: 3})
RETURN dijkstra((v1)-[e:e]->(v2), e.weight);

MATCH (v1:v {id: 0}), (v2:v {id: 0})
RETURN dijkstra((v1)-[e:e]->(v2), e.weight);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      p=dijkstra((v1)-[:e]->(v2), 1)
RETURN nodes(p);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
      p=dijkstra((v1)-[:e]->(v2), 1, LIMIT 10)
RETURN nodes(p);

MATCH (:v {id: 4})-[e:e]-(:v {id: 6})
SET e.weight = 4;

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
	  (path, x)=dijkstra((v1)-[e:e]->(v2), e.weight, LIMIT 2)
RETURN nodes(path), x;

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
	  (path, x)=dijkstra((v2)<-[e:e]->(v1), e.weight, LIMIT 2)
RETURN nodes(path), x;

MATCH (v1:v {id: 0}), (v2:v {id: 3})
RETURN dijkstra((v1)-[e:e]->(v2), e.weight, LIMIT 2);

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
	  (path, x)=dijkstra((v1)-[e:e]->(v2), e.weight, LIMIT 0)
RETURN nodes(path), x;

MATCH (:v {id: 4})-[e:e]-(:v {id: 6})
SET e.weight = -1;

MATCH (v1:v {id: 0}), (v2:v {id: 3}),
	  (path, x)=dijkstra((v1)-[e:e]->(v2), e.weight, LIMIT 10)
return nodes(path), x;

MATCH p= DIJKSTRA((a:v)-[:e]->(b:v), 1) WHERE a.id = 1
RETURN p;

MATCH p= DIJKSTRA((a:v)-[:e]->(b:v), 1, LIMIT 1) WHERE a.id = 1
RETURN p;

MATCH p= DIJKSTRA((a:v)-[r:e]->(b:v), 1, r.weight < 5, LIMIT 1) WHERE a.id = 1
RETURN p;
-- cleanup

DROP GRAPH sp CASCADE;
