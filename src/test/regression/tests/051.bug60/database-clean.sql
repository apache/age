-- This is -*- sql -*- code that removes the test data from the
-- database, to leave things as clean as we found them.

drop trigger t1 on my_table;
drop function p1();
drop table my_table;
