-- Verify SQL and Cypher substring functions. SQL is added more as
-- a sanity check.

-- Create a graph with one node containing a text and numeric value
CREATE GRAPH substring_ag246;
SET graph_path = substring_ag246;
CREATE VLABEL string;
CREATE (:string {sval: '123', nval: 123});

-- Check Cypher substring(string, start, length) from graph
MATCH (u:string) RETURN substring(u.sval, -1, 1);
MATCH (u:string) RETURN substring(u.sval, 0, 1);
MATCH (u:string) RETURN substring(u.sval, 1, 1);
MATCH (u:string) RETURN substring(u.sval, 2, 1);
MATCH (u:string) RETURN substring(u.sval, 3, 1);

-- Sanity check for cast to text -- this should fail now
MATCH (u:string) RETURN substring(u.nval, 1, 1);

-- Check Cypher substring(string, start) from graph
MATCH (u:string) RETURN substring(u.sval, -1);
MATCH (u:string) RETURN substring(u.sval, 0);
MATCH (u:string) RETURN substring(u.sval, 1);
MATCH (u:string) RETURN substring(u.sval, 2);
MATCH (u:string) RETURN substring(u.sval, 3);

-- Sanity check for cast to text -- this should fail now
MATCH (u:string) RETURN substring(u.nval, 1);

-- Check Cypher substring(string, start, length) from constant
RETURN substring('123', -1, 1);
RETURN substring('123', 0, 1);
RETURN substring('123', 1, 1);
RETURN substring('123', 2, 1);
RETURN substring('123', 3, 1);

-- Check Cypher substring(string, start) from constant
RETURN substring('123', -1);
RETURN substring('123', 0);
RETURN substring('123', 1);
RETURN substring('123', 2);
RETURN substring('123', 3);

-- Create a table with one row containing a text value
CREATE TABLE substring_ag246 (tval text);
INSERT INTO substring_ag246 (tval) VALUES ('123');

-- Check SQL substring(string, start, length) from table
-- The first SELECT should return a blank row
SELECT substring(tval, 0, 1) FROM substring_ag246;
SELECT substring(tval, 1, 1) FROM substring_ag246;
SELECT substring(tval, 2, 1) FROM substring_ag246;
SELECT substring(tval, 3, 1) FROM substring_ag246;
SELECT substring(tval, 4, 1) FROM substring_ag246;

-- Check SQL substring(string, start) from table
-- The first and second SELECT should have identical output
SELECT substring(tval, 0) FROM substring_ag246;
SELECT substring(tval, 1) FROM substring_ag246;
SELECT substring(tval, 2) FROM substring_ag246;
SELECT substring(tval, 3) FROM substring_ag246;
SELECT substring(tval, 4) FROM substring_ag246;

-- Check SQL substring(string, start, length) from value
-- Same as above, except with a text constant
SELECT substring('123', 0, 1);
SELECT substring('123', 1, 1);
SELECT substring('123', 2, 1);
SELECT substring('123', 3, 1);
SELECT substring('123', 4, 1);

-- Check SQL substring(string, start) from table
-- Same as above, except with a text constant
SELECT substring('123', 0);
SELECT substring('123', 1);
SELECT substring('123', 2);
SELECT substring('123', 3);
SELECT substring('123', 4);

-- Test hybrid line SQL + Cypher
SELECT substring('12345', 2, 3); -- '234'
RETURN substring((SELECT substring('12345', 2, 3)), 1, 1); -- '3'
