--
-- Cypher Query Language
--
CREATE ROLE another_user;
CREATE USER another_user_01 WITH PASSWORD 'a';

GRANT USAGE ON SCHEMA ddl TO another_user;

ALTER DATABASE regression SET GRAPH_PATH TO ddl;
