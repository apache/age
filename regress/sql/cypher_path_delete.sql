
LOAD 'age';

SET search_path TO ag_catalog;

SELECT create_graph('cypher_path');

-- Create vertex
SELECT * FROM cypher('cypher_path', $$
                CREATE (:label_name_1 {i: 0})
$$) as (a agtype);

-- Create a path to test our create, set and delete on.
SELECT * 
FROM cypher('cypher_path', $$
    CREATE p = (andres {name:'Andres'})-[:WORKS_AT]->(neo)<-[:WORKS_AT]-(michael {name:'Michael'})-[:WORKS_WITH]->(jordan {name: 'Jordan'})
    RETURN p
$$) as (p agtype);

-- Now delete one of the relationships nodes and the output should be return unchanged.
SELECT *
FROM cypher('cypher_path', $$
    MATCH p = ()-[]->()<-[]-()-[]->(j)
    DETACH DELETE j
    RETURN p
$$) AS (a agtype);

-- Now delete one of the edges and the path should be updated but output is still unchanged
SELECT *
FROM cypher('cypher_path', $$
    MATCH p = (andres {name: 'Andres'})-[r:WORKS_AT]->(neo)<-[g:WORKS_AT]-(michael {name: 'Michael'})
    DELETE g
    RETURN p
$$) as (a agtype);

-- Create a path to test our create, set and delete on.
SELECT * 
FROM cypher('cypher_path', $$
    CREATE p = (andres {name:'Andres'})-[:CHILLS_AT]->(neo)<-[:CHILLS_SOME_MORE]-(michael {name:'Michael'}) 
    RETURN p
$$) as (p agtype);

-- Attempt to delete anything other than vertex or edge. This should fail
SELECT *
FROM cypher('cypher_path', $$
    MATCH p = (andres {name: 'Andres'})-[]->()
    DELETE p
    RETURN p
$$) as (a agtype);

-- Cleanup
SELECT drop_graph('cypher_path', true);
