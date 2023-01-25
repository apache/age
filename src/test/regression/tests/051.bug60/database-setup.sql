-- This is a simple -*- sql -*- file that sets up a simple table to
-- which inserts cannot happen. This is enforced by a deferred
-- trigger, which allows for the easy reproduction of an issue
-- observed with commit raises an exception.

create table my_table (
  col1 text not null primary key
);

-- The p1() function simply takes the place of a trigger that would
-- perform semantic or integrity validations that must occur at the
-- end of the transaction. In our case, it simply raises an exception
-- (ie, always fails).

create function p1() returns trigger as
$$
begin
        raise exception 'some integrity violation';
end;
$$
language plpgsql;

-- A simple delayed constraint that insures that the p1() function is
-- invoked at the commit stage.

create constraint trigger t1 after insert on my_table 
deferrable initially deferred 
for each row execute procedure p1();
