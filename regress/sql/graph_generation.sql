-- Load Apache AGE extension
LOAD 'age';

-- Set search path to use AGE catalog
SET search_path = ag_catalog;

-- Create a complete graph (5 vertices, 'edges' and 'vertices' labels)
SELECT * FROM create_complete_graph('gp1', 5, 'edges', 'vertices');
SELECT COUNT(*) FROM gp1."edges";
SELECT COUNT(*) FROM gp1."vertices";

-- Query the edges using Cypher
SELECT * FROM cypher('gp1', $$MATCH (a)-[e]->(b) RETURN e$$) AS (n agtype);

-- Drop the graph after use to avoid conflicts
SELECT drop_graph('gp1', true);

-- Test invalid graph creation (expect failure)
-- Attempt to create a graph without edges or vertices labels
SELECT * FROM create_complete_graph('gp3', 5, NULL) -- SHOULD FAIL;
SELECT * FROM create_complete_graph('gp4', NULL, NULL) -- SHOULD FAIL;
SELECT * FROM create_complete_graph(NULL, NULL, NULL) -- SHOULD FAIL;

-- Test for conflicting labels (should error out)
SELECT * FROM create_complete_graph('gp5', 5, 'label', 'label') -- SHOULD FAIL;

-- Drop any remaining test graphs
SELECT drop_graph('gp3', true);
SELECT drop_graph('gp4', true);
SELECT drop_graph('gp5', true);

-- Test barbell graph generation (5 vertices per side, no intermediate vertices)
SELECT * FROM age_create_barbell_graph('gp1', 5, 0, 'vertices', NULL, 'edges', NULL);
SELECT COUNT(*) FROM gp1."edges";
SELECT COUNT(*) FROM gp1."vertices";

-- Run Cypher query to retrieve edges in the barbell graph
SELECT * FROM cypher('gp1', $$MATCH (a)-[e]->(b) RETURN e$$) AS (n agtype);

-- Drop the graph to avoid conflicts
SELECT drop_graph('gp1', true);

-- Test barbell graph with 10 intermediate vertices
SELECT * FROM age_create_barbell_graph('gp2', 5, 10, 'vertices', NULL, 'edges', NULL);
SELECT COUNT(*) FROM gp2."edges";
SELECT COUNT(*) FROM gp2."vertices";

-- Test invalid barbell graph creation (should fail)
SELECT * FROM age_create_barbell_graph(NULL, NULL, NULL, NULL, NULL, NULL, NULL) -- SHOULD FAIL;
SELECT * FROM age_create_barbell_graph('gp2', NULL, 0, 'vertices', NULL, 'edges', NULL) -- SHOULD FAIL;
SELECT * FROM age_create_barbell_graph('gp3', 5, NULL, 'vertices', NULL, 'edges', NULL) -- SHOULD FAIL;
SELECT * FROM age_create_barbell_graph('gp4', NULL, 0, 'vertices', NULL, 'edges', NULL) -- SHOULD FAIL;
SELECT * FROM age_create_barbell_graph('gp5', 5, 0, 'vertices', NULL, NULL, NULL) -- SHOULD FAIL;

-- Test conflicting labels in barbell graph (should error out)
SELECT * FROM age_create_barbell_graph('gp6', 5, 10, 'label', NULL, 'label', NULL) -- SHOULD FAIL;

-- Drop remaining graphs
SELECT drop_graph('gp2', true);
SELECT drop_graph('gp3', true);
SELECT drop_graph('gp4', true);
SELECT drop_graph('gp5', true);
SELECT drop_graph('gp6', true);
