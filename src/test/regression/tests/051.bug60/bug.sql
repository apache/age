-- This script shows the problem. When run in a psql connected to
-- pgpool with one primary and one read-only replica, this will cause
-- the pgpool process to exit because the exception found in the
-- commit (in the primary node) does not match the success code for the
-- commit in the read-only replica.

-- The expected behavior would be to continue execution normally
-- without dropping the connection.

begin;
insert into my_table ( col1 ) values ( 'ouch' );
commit;
