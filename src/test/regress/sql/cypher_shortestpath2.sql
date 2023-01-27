-- PREPARE

SET client_min_messages TO WARNING;

SET work_mem TO '1MB';

DROP GRAPH IF EXISTS shortestpath CASCADE;

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

create graph shortestpath;

create vlabel vp;
create vlabel vc inherits ( vp );
create vlabel o;
create vlabel s;
create vlabel c;
create vlabel l;
create vlabel r;
create elabel ep;
create elabel ec inherits ( ep );
create elabel e;


-- Neo4j
-- create index on :o ( id );
-- create index on :s ( id );
-- create index on :c ( id );
-- create index on :l ( id );
-- create index on :r ( id );
-- create index on :e ( id );

create property index on o ( id );
create property index on s ( id );
create property index on c ( id );
create property index on l ( id );
create property index on r ( id );
create property index on e ( id );


-- Inherit

create (:vc{id:1})-[:ec]->(:vc{id:2});


-- Orphan

create (:o{id:1})
create (:o{id:2})
create (:o{id:3})
create (:o{id:4})
create (:o{id:5})
create (:o{id:6})
create (:o{id:7})
create (:o{id:8})
create (:o{id:9});


-- Unforked

create (:s{id:1})
create (:s{id:2})
create (:s{id:3})
create (:s{id:4})
create (:s{id:5})
create (:s{id:6})
create (:s{id:7})
create (:s{id:8})
create (:s{id:9})
create (:s{id:10})
create (:s{id:11})
create (:s{id:12})
create (:s{id:13})
create (:s{id:14})
create (:s{id:15})
create (:s{id:16})
create (:s{id:17})
create (:s{id:18})
create (:s{id:19});
match (s1:s), (s2:s{id:s1.id+1})
create (s1)-[:e{id:'s:'+s1.id+'->s:'+s2.id}]->(s2);

-- No Labels

match shortestpath( (a)-[b]->(c) ) return a.id as a, c.id as c order by a, c;
match p = shortestpath( (a)-[b]->(c) ) return ids(nodes(p)) as p, a.id as a, c.id as c order by a, c;
match allshortestpaths( (a)-[b]->(c) ) return a.id as a, c.id as c order by a, c;
match p = allshortestpaths( (a)-[b]->(c) ) return ids(nodes(p)) as p, a.id as a, c.id as c order by a, c;

-- Cycle

match (o:o)
create (:c{id:10+o.id})
create (:c{id:20+o.id})
create (:c{id:30+o.id})
create (:c{id:40+o.id})
create (:c{id:50+o.id})
create (:c{id:60+o.id})
create (:c{id:70+o.id})
create (:c{id:80+o.id})
create (:c{id:90+o.id});

match (c1:c), (c2:c)
where c1.id >= 11 and c1.id <= 19 and c2.id >= 11 and c2.id <= 19
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 21 and c1.id <= 29 and c2.id >= 21 and c2.id <= 29
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 31 and c1.id <= 39 and c2.id >= 31 and c2.id <= 39 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 41 and c1.id <= 49 and c2.id >= 41 and c2.id <= 49 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 51 and c1.id <= 59 and c2.id >= 51 and c2.id <= 59 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 61 and c1.id <= 69 and c2.id >= 61 and c2.id <= 69 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 71 and c1.id <= 79 and c2.id >= 71 and c2.id <= 79 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 81 and c1.id <= 89 and c2.id >= 81 and c2.id <= 89 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c), (c2:c)
where c1.id >= 91 and c1.id <= 99 and c2.id >= 91 and c2.id <= 99 and c1.id <> c2.id
create (c1)-[:e{id:'c:'+c1.id+'->c:'+c2.id}]->(c2);

match (c1:c{id:59}), (c2:c{id:69})
create (c1)-[:e{id:'c:'+c1.id+'->c'+c2.id}]->(c2)
create (c2)-[:e{id:'c:'+c2.id+'->c'+c1.id}]->(c1);

match (c1:c{id:79}), (c2:c{id:89}), (c3:c{id:99})
create (c1)-[:e{id:'c:'+c1.id+'->c'+c2.id}]->(c2)
create (c2)-[:e{id:'c:'+c2.id+'->c'+c3.id}]->(c3)
create (c3)-[:e{id:'c:'+c3.id+'->c'+c1.id}]->(c1);


-- Spread

match (o:o) create (:l{id:o.id});

match (n:l) where n.id >= 1 and n.id <= 9
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+1)}]->(:l{id:n.id*10+1})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+2)}]->(:l{id:n.id*10+2})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+3)}]->(:l{id:n.id*10+3})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+4)}]->(:l{id:n.id*10+4})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+5)}]->(:l{id:n.id*10+5})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+6)}]->(:l{id:n.id*10+6})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+7)}]->(:l{id:n.id*10+7})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+8)}]->(:l{id:n.id*10+8})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+9)}]->(:l{id:n.id*10+9});

match (n:l) where n.id >= 11 and n.id <= 99
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+1)}]->(:l{id:n.id*10+1})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+2)}]->(:l{id:n.id*10+2})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+3)}]->(:l{id:n.id*10+3})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+4)}]->(:l{id:n.id*10+4})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+5)}]->(:l{id:n.id*10+5})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+6)}]->(:l{id:n.id*10+6})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+7)}]->(:l{id:n.id*10+7})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+8)}]->(:l{id:n.id*10+8})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+9)}]->(:l{id:n.id*10+9});

match (n:l) where n.id >= 111 and n.id <= 999
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+1)}]->(:l{id:n.id*10+1})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+2)}]->(:l{id:n.id*10+2})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+3)}]->(:l{id:n.id*10+3})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+4)}]->(:l{id:n.id*10+4})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+5)}]->(:l{id:n.id*10+5})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+6)}]->(:l{id:n.id*10+6})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+7)}]->(:l{id:n.id*10+7})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+8)}]->(:l{id:n.id*10+8})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+9)}]->(:l{id:n.id*10+9});

match (n:l) where n.id >= 1111 and n.id <= 9999
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+1)}]->(:l{id:n.id*10+1})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+2)}]->(:l{id:n.id*10+2})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+3)}]->(:l{id:n.id*10+3})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+4)}]->(:l{id:n.id*10+4})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+5)}]->(:l{id:n.id*10+5})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+6)}]->(:l{id:n.id*10+6})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+7)}]->(:l{id:n.id*10+7})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+8)}]->(:l{id:n.id*10+8})
create (n)-[:e{id:'l:'+n.id+'->l:'+(n.id*10+9)}]->(:l{id:n.id*10+9});


-- Convergence

match (o:o) create (:r{id:o.id});

match (n:r) where n.id >= 1 and n.id <= 9
create (:r{id:n.id*10+1})-[:e{id:'r:'+(n.id*10+1)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+2})-[:e{id:'r:'+(n.id*10+2)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+3})-[:e{id:'r:'+(n.id*10+3)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+4})-[:e{id:'r:'+(n.id*10+4)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+5})-[:e{id:'r:'+(n.id*10+5)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+6})-[:e{id:'r:'+(n.id*10+6)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+7})-[:e{id:'r:'+(n.id*10+7)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+8})-[:e{id:'r:'+(n.id*10+8)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+9})-[:e{id:'r:'+(n.id*10+9)+'->r:'+n.id}]->(n);

match (n:r) where n.id >= 11 and n.id <= 99
create (:r{id:n.id*10+1})-[:e{id:'r:'+(n.id*10+1)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+2})-[:e{id:'r:'+(n.id*10+2)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+3})-[:e{id:'r:'+(n.id*10+3)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+4})-[:e{id:'r:'+(n.id*10+4)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+5})-[:e{id:'r:'+(n.id*10+5)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+6})-[:e{id:'r:'+(n.id*10+6)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+7})-[:e{id:'r:'+(n.id*10+7)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+8})-[:e{id:'r:'+(n.id*10+8)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+9})-[:e{id:'r:'+(n.id*10+9)+'->r:'+n.id}]->(n);

match (n:r) where n.id >= 111 and n.id <= 999
create (:r{id:n.id*10+1})-[:e{id:'r:'+(n.id*10+1)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+2})-[:e{id:'r:'+(n.id*10+2)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+3})-[:e{id:'r:'+(n.id*10+3)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+4})-[:e{id:'r:'+(n.id*10+4)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+5})-[:e{id:'r:'+(n.id*10+5)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+6})-[:e{id:'r:'+(n.id*10+6)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+7})-[:e{id:'r:'+(n.id*10+7)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+8})-[:e{id:'r:'+(n.id*10+8)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+9})-[:e{id:'r:'+(n.id*10+9)+'->r:'+n.id}]->(n);

match (n:r) where n.id >= 1111 and n.id <= 9999
create (:r{id:n.id*10+1})-[:e{id:'r:'+(n.id*10+1)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+2})-[:e{id:'r:'+(n.id*10+2)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+3})-[:e{id:'r:'+(n.id*10+3)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+4})-[:e{id:'r:'+(n.id*10+4)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+5})-[:e{id:'r:'+(n.id*10+5)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+6})-[:e{id:'r:'+(n.id*10+6)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+7})-[:e{id:'r:'+(n.id*10+7)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+8})-[:e{id:'r:'+(n.id*10+8)+'->r:'+n.id}]->(n)
create (:r{id:n.id*10+9})-[:e{id:'r:'+(n.id*10+9)+'->r:'+n.id}]->(n);


-- Vibration

match (l:l), (r:r{id:l.id}) where l.id = 1 and l.id <= 1
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);
match (l:l), (r:r{id:l.id}) where l.id >= 22 and l.id <= 28
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);
match (l:l), (r:r{id:l.id}) where l.id >= 321 and l.id <= 389
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);
match (l:l), (r:r{id:l.id}) where l.id >= 4211 and l.id <= 4899
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);
match (l:l), (r:r{id:l.id}) where l.id >= 52111 and l.id <= 58999
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);
match (l:l), (r:r{id:l.id}) where l.id = 6
create(r)-[:e{id:'r:'+l.id+'->l:'+r.id}]->(l);
match (l:l), (r:r{id:l.id}) where l.id >= 81111 and l.id <= 89999
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);
match (r:r{id:8}), (l:l{id:9})
create(r)-[:e{id:'r:'+r.id+'->l:'+l.id}]->(l);
match (l:l), (r:r{id:l.id}) where l.id >= 91111 and l.id <= 99999
create(l)-[:e{id:'l:'+l.id+'->r:'+r.id}]->(r);


-- VACUUM

vacuum full analyze shortestpath.vp;
vacuum full analyze shortestpath.vc;
vacuum full analyze shortestpath.o;
vacuum full analyze shortestpath.s;
vacuum full analyze shortestpath.c;
vacuum full analyze shortestpath.l;
vacuum full analyze shortestpath.r;
vacuum full analyze shortestpath.ep;
vacuum full analyze shortestpath.ec;
vacuum full analyze shortestpath.e;


-- Orphan

match p = shortestpath( (o1:o)-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = shortestpath( (o1:o)-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;

match p = shortestpath( (o1:o)<-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)<-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)<-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = shortestpath( (o1:o)<-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)<-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)<-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)<-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)<-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;

match p = shortestpath( (o1:o)-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = shortestpath( (o1:o)-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)-[:e*0..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)-[:e*1..]-(o2:o) ) where o1.id = 1 and o2.id = 2 return p;

match p = shortestpath( (o1:o)<-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)<-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = shortestpath( (o1:o)<-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = shortestpath( (o1:o)<-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)<-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)<-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
match p = allshortestpaths( (o1:o)<-[:e*0..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;
match p = allshortestpaths( (o1:o)<-[:e*1..]->(o2:o) ) where o1.id = 1 and o2.id = 2 return p;

match (o1:o), (o2:o) where o1.id >= 1 and o1.id <= 2 and o2.id >= 1 and o2.id <= 2 return o1.id as o1id, o2.id as o2id, shortestpath((o1)-[:e]->(o2)) order by o1id, o2id;
match (o1:o), (o2:o) where o1.id >= 1 and o1.id <= 2 and o2.id >= 1 and o2.id <= 2 return o1.id as o1id, o2.id as o2id, shortestpath((o1)-[:e*]->(o2)) order by o1id, o2id;
match (o1:o), (o2:o) where o1.id >= 1 and o1.id <= 2 and o2.id >= 1 and o2.id <= 2 return o1.id as o1id, o2.id as o2id, shortestpath((o1)-[:e*0..]->(o2)) order by o1id, o2id;
match (o1:o), (o2:o) where o1.id >= 1 and o1.id <= 2 and o2.id >= 1 and o2.id <= 2 return o1.id as o1id, o2.id as o2id, allshortestpaths((o1)-[:e]->(o2)) order by o1id, o2id;
match (o1:o), (o2:o) where o1.id >= 1 and o1.id <= 2 and o2.id >= 1 and o2.id <= 2 return o1.id as o1id, o2.id as o2id, allshortestpaths((o1)-[:e*]->(o2)) order by o1id, o2id;
match (o1:o), (o2:o) where o1.id >= 1 and o1.id <= 2 and o2.id >= 1 and o2.id <= 2 return o1.id as o1id, o2.id as o2id, allshortestpaths((o1)-[:e*0..]->(o2)) order by o1id, o2id;


-- Unforked

match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*0..]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..2]->(s2)))) order by s1id, s2id;

match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)<-[:e]-(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)<-[:e*0..]-(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)<-[:e*]-(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)<-[:e*..2]-(s2)))) order by s1id, s2id;

match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, size(allshortestpaths((s1)-[:e]-(s2))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, size(allshortestpaths((s1)-[:e*0..]-(s2))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, size(allshortestpaths((s1)-[:e*]-(s2))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, size(allshortestpaths((s1)-[:e*..2]-(s2))) order by s1id, s2id;

match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*0..0]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..1]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..2]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..3]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..4]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..5]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..6]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..7]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..8]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..9]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..10]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..11]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..12]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..13]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..14]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..15]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..16]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..17]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..18]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..19]->(s2)))) order by s1id, s2id;
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e*..20]->(s2)))) order by s1id, s2id;


-- Cycle

match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 11 and c2.id <= 19 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 21 and c2.id <= 29 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 31 and c1.id <= 39 and c2.id >= 31 and c2.id <= 39 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 31 and c1.id <= 39 and c2.id >= 41 and c2.id <= 49 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;

match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 11 and c2.id <= 19 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)<-[:e*]-(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 21 and c2.id <= 29 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)<-[:e*]-(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 31 and c1.id <= 39 and c2.id >= 31 and c2.id <= 39 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)<-[:e*]-(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 31 and c1.id <= 39 and c2.id >= 41 and c2.id <= 49 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)<-[:e*]-(c2)) order by c1id, c2id;

match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 11 and c2.id <= 19 return c1.id as c1id, c2.id as c2id, size(allshortestpaths((c1)-[:e*]-(c2))) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 21 and c2.id <= 29 return c1.id as c1id, c2.id as c2id, size(allshortestpaths((c1)-[:e*]-(c2))) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 31 and c1.id <= 39 and c2.id >= 31 and c2.id <= 39 return c1.id as c1id, c2.id as c2id, size(allshortestpaths((c1)-[:e*]-(c2))) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 31 and c1.id <= 39 and c2.id >= 41 and c2.id <= 49 return c1.id as c1id, c2.id as c2id, size(allshortestpaths((c1)-[:e*]-(c2))) order by c1id, c2id;

match (c1:c), (c2:c) where c1.id >= 51 and c1.id <= 59 and c2.id >= 61 and c2.id <= 69 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 71 and c1.id <= 79 and c2.id >= 81 and c2.id <= 99 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;

match (c1:c), (c2:c) where c1.id >= 51 and c1.id <= 59 and c2.id >= 61 and c2.id <= 69 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)<-[:e*]-(c2)) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 71 and c1.id <= 79 and c2.id >= 81 and c2.id <= 99 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)<-[:e*]-(c2)) order by c1id, c2id;

match (c1:c), (c2:c) where c1.id >= 51 and c1.id <= 59 and c2.id >= 61 and c2.id <= 69 return c1.id as c1id, c2.id as c2id, size(allshortestpaths((c1)-[:e*]-(c2))) order by c1id, c2id;
match (c1:c), (c2:c) where c1.id >= 71 and c1.id <= 79 and c2.id >= 81 and c2.id <= 99 return c1.id as c1id, c2.id as c2id, size(allshortestpaths((c1)-[:e*]-(c2))) order by c1id, c2id;


-- Spread

match (l1:l), (l2:l) where l1.id = 1 and l2.id = 11111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l1)-[:e*]->(l2)) order by l1id, l2id;
match (l1:l), (l2:l) where l1.id = 1 and l2.id = 11111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l1)<-[:e*]-(l2)) order by l1id, l2id;
match (l1:l), (l2:l) where l1.id = 1 and l2.id = 11111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l1)-[:e*]-(l2)) order by l1id, l2id;
match (l1:l), (l2:l) where l1.id = 1 and l2.id = 11111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l2)<-[:e*]-(l1)) order by l1id, l2id;
match (l1:l), (l2:l) where l1.id = 1 and l2.id = 11111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l2)-[:e*]->(l1)) order by l1id, l2id;
match (l1:l), (l2:l) where l1.id = 1 and l2.id = 11111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l2)-[:e*]-(l1)) order by l1id, l2id;


-- Convergence

match (r1:r), (r2:r) where r1.id = 1 and r2.id = 11111 return r1.id as r1id, r2.id as r2id, allshortestpaths((r1)-[:e*]->(r2)) order by r1id, r2id;
match (r1:r), (r2:r) where r1.id = 1 and r2.id = 11111 return r1.id as r1id, r2.id as r2id, allshortestpaths((r1)<-[:e*]-(r2)) order by r1id, r2id;
match (r1:r), (r2:r) where r1.id = 1 and r2.id = 11111 return r1.id as r1id, r2.id as r2id, allshortestpaths((r1)-[:e*]-(r2)) order by r1id, r2id;
match (r1:r), (r2:r) where r1.id = 1 and r2.id = 11111 return r1.id as r1id, r2.id as r2id, allshortestpaths((r2)<-[:e*]-(r1)) order by r1id, r2id;
match (r1:r), (r2:r) where r1.id = 1 and r2.id = 11111 return r1.id as r1id, r2.id as r2id, allshortestpaths((r2)-[:e*]->(r1)) order by r1id, r2id;
match (r1:r), (r2:r) where r1.id = 1 and r2.id = 11111 return r1.id as r1id, r2.id as r2id, allshortestpaths((r2)-[:e*]-(r1)) order by r1id, r2id;


-- Vibration

match p = allshortestpaths( (l1:l)-[:e*]->(r2:r) ) where l1.id = 8 and r2.id = 8 return l1.id as l1id, r2.id as r2id, count(p) order by l1id, r2id;
match p = allshortestpaths( (l1:l)<-[:e*]-(r2:r) ) where l1.id = 8 and r2.id = 8 return l1.id as l1id, r2.id as r2id, count(p) order by l1id, r2id;
match p = allshortestpaths( (l1:l)-[:e*]-(r2:r) ) where l1.id = 8 and r2.id = 8 return l1.id as l1id, r2.id as r2id, count(p) order by l1id, r2id;

match p = allshortestpaths( (r1:l)-[:e*]->(l2:r) ) where r1.id = 88888 and l2.id = 99999 return r1.id as r1id, l2.id as l2id, count(p) order by r1id, l2id;
match p = allshortestpaths( (r1:l)<-[:e*]-(l2:r) ) where r1.id = 88888 and l2.id = 99999 return r1.id as r1id, l2.id as l2id, count(p) order by r1id, l2id;
match p = allshortestpaths( (r1:l)-[:e*]-(l2:r) ) where r1.id = 88888 and l2.id = 99999 return r1.id as r1id, l2.id as l2id, count(p) order by r1id, l2id;

match p = allshortestpaths( (l1:l)-[:e*]->(r2:r) ) where l1.id = 888 and r2.id = 999 return l1.id as l1id, r2.id as r2id, count(p) order by l1id, r2id;
match p = allshortestpaths( (l1:l)<-[:e*]-(r2:r) ) where l1.id = 888 and r2.id = 999 return l1.id as l1id, r2.id as r2id, count(p) order by l1id, r2id;
match p = allshortestpaths( (l1:l)-[:e*]-(r2:r) ) where l1.id = 888 and r2.id = 999 return l1.id as l1id, r2.id as r2id, count(p) order by l1id, r2id;


-- Filter

match p = shortestpath( (:o)-[:e]->(:o) ) return p;
match p = shortestpath( (o1:o)-[:e]->(o2:o) ) return o1, o2, p;
match p = shortestpath( (o1:o)-[:e]->(o2:o) ) where o1.id = 1 return o1, o2, p;
match p = shortestpath( (o1:o)-[:e]->(o2:o) ) where o2.id = 1 return o1, o2, p;
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e]->(o2) ) return o1, o2, p;
match (o1:o), (o2:o) match p = shortestpath( (o1)-[:e]->(o2) ) where o1.id = 1 and o2.id = 1 return o1, o2, p;


-- Inherit

match p = shortestpath( (p1:vp)-[:ep]->(p2:vp) ) return p1, p2, p;
match p = shortestpath( (p1:vp only)-[:ep only]->(p2:vp only) ) return p1, p2, p;
match p = shortestpath( (p1:vc only)-[:ec only]->(p2:vc only) ) return p1, p2, p;
match p = shortestpath( (p1:vp only)-[:ep]->(p2:vp only) ) return p1, p2, p;
match p = shortestpath( (p1:vc only)-[:ep]->(p2:vc only) ) return p1, p2, p;
match p = shortestpath( (p1:vp)-[:ep only]->(p2:vp) ) return p1, p2, p;
match p = shortestpath( (p1:vp)-[:ec only]->(p2:vp) ) return p1, p2, p;


-- Property Constraint

match p = shortestpath( (s1:s{id:1})-[:e]->(s2:s{id:2}) ) return p;
match (s1:s), (s2:s) match p = shortestpath( (s1{id:1})-[:e]->(s2{id:2}) ) return p;


-- Edge Reference

match p = shortestpath( (s1:s{id:1})-[e:e*]->(s2:s{id:19}) ) return ids(nodes(p)) as p, e;


-- No Path Variable

match shortestpath( (s1:s{id:1})-[:e*]->(s2:s{id:19}) ) return s1.id, s2.id;
match shortestpath( (s1:s{id:1})-[e:e*]->(s2:s{id:19}) ) return s1.id, s2.id, e;


-- Failure

match p = shortestpath( (:o) ) return p;
match p = shortestpath( ()-[:e]->() ) return p;
match p = shortestpath( (:o)-[:e]->(:o)-[:e]->(:o) ) return p;
match p = shortestpath( (:o{id:1})-[:e]->(:o{id:1}) ) return p;
match p = shortestpath( (o1:o)-[:e{id:1}]->(o1:o) ) return p;
match p = shortestpath( (:o)-[r:e]->(:o) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e*-1]->(o2) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e*-1..]->(o2) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e*2..]->(o2) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e*..0]->(o2) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e*..-1]->(o2) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) return shortestpath( (o1{id:1})-[:e]->(o2{id:1}) );
match (o1:o{id:1})-[e:e]->(o2:o{id:1}) match p = shortestpath( (o1)-[e:e*]->(o2) ) return p;
match (o1:o{id:1}), (o2:o{id:1}) return shortestpath( (o1)-[e:e*]->(o2) );

-- Plan

SET enable_bitmapscan to off;
SET enable_material to off;

explain ( analyze false, verbose true, costs false, buffers false, timing false )
match p = shortestpath( (o1:o)-[:e]->(o2:o) ) where o1.id = 1 and o2.id = 1 return p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match p = allshortestpaths( (o1:o)-[:e*0..]->(o2:o) ) where o1.id = 1 and o1.id = 1 return p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match (o1:o{id:1}), (o2:o{id:1}) match p = shortestpath( (o1)-[:e]->(o2) ) return o1, o2, p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match (o1:o), (o2:o) match p = shortestpath( (o1)-[:e]->(o2) ) where o1.id = 1 and o2.id = 1 return o1, o2, p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match p = shortestpath( (o1:o)<-[:e*0..]->(o2:o) ) where o1.id = 1 and o1.id = 1 return p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match p = shortestpath( (o1:o)<-[:e*1..]->(o2:o) ) where o1.id = 1 and o1.id = 1 return p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match p = shortestpath( (o1:o)<-[:e*0..1]->(o2:o) ) where o1.id = 1 and o1.id = 1 return p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match p = shortestpath( (o1:o)<-[:e*1..2]->(o2:o) ) where o1.id = 1 and o1.id = 1 return p;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match (s1:s), (s2:s) return s1.id as s1id, s2.id as s2id, ids(nodes(shortestpath((s1)-[:e]->(s2)))) order by s1id, s2id;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match (c1:c), (c2:c) where c1.id >= 11 and c1.id <= 19 and c2.id >= 11 and c2.id <= 19 return c1.id as c1id, c2.id as c2id, allshortestpaths((c1)-[:e*]->(c2)) order by c1id, c2id;
explain ( analyze false, verbose true, costs false, buffers false, timing false )
match (l1:l), (l2:l) where l1.id = 1 and l2.id = 111111 return l1.id as l1id, l2.id as l2id, allshortestpaths((l1)-[:e*]->(l2)) order by l1id, l2id;


-- DESTROY

DROP FUNCTION ids(vertex[]);

DROP GRAPH IF EXISTS shortestpath CASCADE;
