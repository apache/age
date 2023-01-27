CREATE GRAPH cypher_dml2;

CREATE VLABEL v1;

EXPLAIN ( analyze false, verbose true, costs false, buffers false, timing false )
MATCH (p:v1)
return max(collect(p.name)) as col;

MATCH (p:v1)
return max(collect(p.name)) as col;

MATCH (p:v1)
with collect(p.name) as col
RETURN max(col);

CREATE ELABEL e1;

CREATE (a: v1 {id: 1})
CREATE (b: v1 {id: 2})
CREATE (a)-[r:e1 {text: 'text'}]->(b)
RETURN r;

CREATE (a: v1 {id: 3})
CREATE (b: v1 {id: 4})
CREATE (a)-[r:e1 {id: 5, text: 'text'}]->(b)
RETURN r;

-- AGV2-29, Predicates functions want jsonb, not list
MATCH p=(n1)-[r:e1*1]->(n2)
WHERE all(x in r where x.id is null)
RETURN count(p), p;

MATCH p=(n1)-[r:e1*1]->(n2)
WHERE all(x in r where x.text is not null)
RETURN count(p), p;

MATCH (n) DETACH DELETE n;

-- AGV2-26, head/tail/last returns array
CREATE(:v_user{name:'userA'});
CREATE(:v_title{name:'TitleA'});
CREATE(:v_type{name:'TypeA'});
CREATE(:v_sub{name:'SubA'});

MATCH(v1:v_user{name:'userA'}), (v2:v_title{name:'TitleA'})
CREATE (v1)-[:e_user_title{name:'(1)', val:1}]->(v2);

MATCH(v1:v_title{name:'TitleA'}), (v2:v_type{name:'TypeA'})
CREATE (v1)-[:e_title_type{name:'(2)', val:2}]->(v2);

MATCH(v1:v_type{name:'TypeA'}), (v2:v_sub{name:'SubA'})
CREATE (v1)-[:e_title_type{name:'(3)', val:3}]->(v2);

MATCH(n)-[e*3]->(n3) RETURN e;
MATCH(n)-[e*3]->(n3) RETURN head(e);
MATCH(n)-[e*3]->(n3) RETURN tail(e);
MATCH(n)-[e*3]->(n3) RETURN last(e);

MATCH (n) DETACH DELETE n;

CREATE (a:person);
create (a:person {name: 'Alice', age: 51, eyes: 'brown'}),
(b:person {name: 'Frank', age: 61, eyes: '', liked_colors: ['blue','green']}),
(c:person {name: 'Charlie', age: 53, eyes: 'green'}),
(d:person {name: 'Bob', age: 25, eyes: 'blue'}),
(e:person {name: 'Daniel', age: 54, eyes: 'brown', liked_colors: ''}),
(f:person {name: 'Eskil', age: 41, eyes: 'blue', liked_colors: ['pink','yellow','black']}),
(a)-[:knows]->(c),
(a)-[:knows]->(d),
(c)-[:knows]->(e),
(d)-[:knows]->(e),
(d)-[:married]->(f);

-- all(..)
MATCH p = (a)-[*1..3]->(b)
WHERE
a.name = 'Alice'
AND b.name = 'Daniel'
AND all(x IN nodes(p) WHERE x.age > 30)
RETURN [x in nodes(p) | x.age];

-- any(..)
MATCH (n)
WHERE any(color IN n.liked_colors WHERE color = 'yellow')
RETURN n ;

-- exists(..)
MATCH (n)
WHERE n.name IS NOT NULL
RETURN
n.name AS name,
exists((n)-[:MARRIED]->()) AS is_married;

-- isEmpty(..)
-- List
MATCH (n)
WHERE NOT isEmpty(n.liked_colors)
RETURN n ;

-- Map
MATCH (n)
WHERE isEmpty(properties(n))
RETURN n ;

MATCH (n)
WHERE NOT isEmpty(properties(n))
RETURN n ;

-- String
MATCH (n)
WHERE isEmpty(n.eyes)
RETURN n.age AS age ;

-- none(..)
MATCH p = (n)-[*1..3]->(b)
WHERE
n.name = 'Alice'
AND none(x IN nodes(p) WHERE x.age = 25)
RETURN p ;

-- single(..)
MATCH p = (n)-[]->(b)
WHERE
n.name = 'Alice'
AND single(var IN nodes(p) WHERE var.eyes = 'blue')
RETURN p ;

MATCH (n) DETACH DELETE n;
MATCH (n) RETURN n;

-- Trigger
CREATE TEMPORARY TABLE _trigger_history(
    id graphid,
    is_before boolean,
    CONSTRAINT pk_history PRIMARY KEY (id, is_before)
);

CREATE OR REPLACE FUNCTION v1_test_trigger_func_be()
returns trigger
AS $$
DECLARE
BEGIN
    raise notice 'Bf: % %', new, old;
    CASE WHEN new.id IS NULL
    THEN
        DELETE FROM _trigger_history WHERE _trigger_history.id = old.id AND
            _trigger_history.is_before = true;
        RETURN old;
    ELSE
        INSERT INTO _trigger_history(id, is_before) VALUES (new.id, true);
        RETURN new;
    END CASE;
END; $$
LANGUAGE 'plpgsql';

create trigger v1_test_trigger_be
    before insert or delete or update on cypher_dml2.v1
	for each row
    execute procedure v1_test_trigger_func_be();

CREATE OR REPLACE FUNCTION v1_test_trigger_func_af()
returns trigger
AS $$
DECLARE
BEGIN
    raise notice 'Af: % %', new, old;
    CASE WHEN new.id IS NULL THEN
        DELETE FROM _trigger_history WHERE _trigger_history.id = old.id AND
            _trigger_history.is_before = false;
        RETURN old;
    ELSE
        INSERT INTO _trigger_history(id, is_before) VALUES (new.id, false);
        RETURN new;
    END CASE;
END; $$
LANGUAGE 'plpgsql';

create trigger v1_test_trigger_af
    after insert or delete or update on cypher_dml2.v1
	for each row
    execute procedure v1_test_trigger_func_af();

CREATE (v1:v1 {name: 'trigger_item'}) RETURN v1;
SELECT * FROM _trigger_history;

-- Must fail
MATCH (n) SET n.name = 'trigger_item_updated' RETURN n;
SELECT * FROM _trigger_history;

-- Should pass
DELETE FROM _trigger_history;
MATCH (n) SET n.name = 'trigger_item_updated' RETURN n;
SELECT * FROM _trigger_history;

-- Must empty
MATCH (n) DETACH DELETE n;
SELECT * FROM _trigger_history;

DROP GRAPH cypher_dml2 CASCADE;

-- #589 ( Cypher read clauses cannot follow update clauses )
CREATE GRAPH cypher_dml2;
SET GRAPH_PATH to cypher_dml2;

CREATE VLABEL main;
CREATE ELABEL main2;

CREATE (n:another {id: 593}) RETURN n.id;

MERGE (n:main {id: 593})
ON CREATE SET n.id = 593
WITH n
MATCH (g: another)
WHERE g.id = 593
MERGE (g)-[:main2]->(n);

MATCH ()-[e:main2]-() RETURN e;
MATCH (g: another) RETURN g;
MATCH (g: main) RETURN g;

DROP GRAPH cypher_dml2 CASCADE;

-- fix: Includes necessary JOINs of vertices (#599)
CREATE GRAPH cypher_dml2;
SET GRAPH_PATH to cypher_dml2;

CREATE ({id: 1})-[:e1]->({id: 2})-[:e1]->({id: 3})-[:e1]->({id: 4})
RETURN *;

MATCH (a {id: 1}), (b {id: 1})
CREATE (b)-[:e1]->(a)
RETURN *;

MATCH (a)
RETURN *;

MATCH (a)-[]-(a) RETURN *;
MATCH p=(a)-[]-(a) RETURN *;

EXPLAIN VERBOSE MATCH (a)-[]-(a) RETURN a;
EXPLAIN VERBOSE MATCH (a)-[]-(a) RETURN *;
EXPLAIN VERBOSE MATCH p=(a)-[]-(a) RETURN *;

DROP GRAPH cypher_dml2 CASCADE;
