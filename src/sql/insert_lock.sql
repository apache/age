-- Create lock control table for tables using sequence in native replication mode.

DROP TABLE pgpool_catalog.insert_lock;

CREATE SCHEMA pgpool_catalog;
CREATE TABLE pgpool_catalog.insert_lock(reloid OID PRIMARY KEY);
 
-- this row is used as the row lock target when pgpool inserts new oid
INSERT INTO pgpool_catalog.insert_lock VALUES (0);

-- allow "SELECT ... FOR UPDATE" and "INSERT ..." to all roles
GRANT USAGE ON SCHEMA pgpool_catalog TO PUBLIC;
GRANT SELECT ON pgpool_catalog.insert_lock TO PUBLIC;
GRANT UPDATE ON pgpool_catalog.insert_lock TO PUBLIC;
GRANT INSERT ON pgpool_catalog.insert_lock TO PUBLIC;
