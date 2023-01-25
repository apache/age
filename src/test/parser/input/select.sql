SELECT 1;
SELECT * FROM t;
SELECT x FROM a;
SELECT x.y FROM a;
SELECT x FROM a.b;
SeLeCt foo();
SELECT y FROM foo();
SELECT * FROM (SELECT * FROM foo);
select a as t from x;
SELECT x FROM a as b;
SELECT a as tttttttttttttttttttttttttttttttttttttttttttttttttttttttt, b as uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu from zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz;
SELECT ;
SELECT * FROM t FOR UPDATE
SELECT * FROM t FOR SHARE
SELECT distinct * FROM t
select distinct on (x) x from t;
select distinct on (foo(x)) x from t;
SELECT ALL * FROM t;
SELECT x FROM t WHERE x > 3;
SELECT count(*) FROM t GROUP BY x HAVING x > 111;
SELECT count(*) FROM t GROUP BY x HAVING x > 111 ORDER BY y;
SELECT count(*) FROM t GROUP BY x HAVING x > 111 ORDER BY y DESC;
SELECT count(*) FROM t GROUP BY x HAVING x > 111 ORDER BY y ASC;
SELECT x FROM t LIMIT ALL;
SELECT x FROM t LIMIT 1;
SELECT * from t offset 1;
SELECT * from t limit 2 offset 1;
SELECT * from t offset 1 limit 2;
SELECT * FROM t left outer join t1 on (x = y);
SELECT * FROM t right outer join t1 on (x = y);
SELECT * FROM t full outer join t1 on (x = y);
SELECT * FROM t left outer join t1 using (x);
SELECT * FROM t right outer join t1 using (x);
SELECT * FROM t full outer join t1 using (x);
SELECT * FROM t inner join t1 on x;
SELECT * FROM t natural left outer join t1;
SELECT * FROM t natural right outer join t1;
SELECT * FROM t natural full outer join t1;
select * from t where a IS NULL
select * from t where a IS NOT NULL
select * from t where a like 'abc';
select * from t where a not like 'abc';
SELECT * FROM t where a between 1 and 4;
SELECT * FROM t where a In (1,2,3,4);
SELECT * FROM t where a not In (1,2,3,4);
SELECT * FROM t WHERE EXISTS (select * from tt);
SELECT * FROM t WHERE NOT EXISTS (select * from tt);
(SELECT * FROM t1) UNION (SELECT * FROM t2);
(SELECT * FROM t1) UNION ALL (SELECT * FROM t2);
(SELECT * FROM t1) INTERSECT (SELECT * FROM t2);
(SELECT * FROM t1) INTERSECT ALL (SELECT * FROM t2);
(SELECT * FROM t1) EXCEPT (SELECT * FROM t2);
(SELECT * FROM t1) EXCEPT ALL (SELECT * FROM t2);
SELECT * INTO x FROM t;
select * from t1 where i <> ALL (select i from t2);
SELECT * FROM t1 UNION ALL SELECT * FROM t2 ORDER BY a;
SELECT * FROM t1 UNION ALL (SELECT * FROM t2 ORDER BY a);
select max(i)::int from test
select max(i)::int from test where a = 'aaa''bbb\\ccc'
select max(i)::int from test where a = 'aaa\'bbb\\ccc'
SELECT * FROM t WHERE 1 = any(arr)
SELECT * FROM t WHERE 1 = all(arr)
SELECT * FROM t WHERE 1 IS OF (int)
SELECT * FROM t WHERE 1 IS NOT OF (int)
SELECT * FROM t WHERE a IN ('a','b',1);
SELECT * FROM t WHERE a NOT IN ('a','b',1);
SELECT * FROM t WHERE a IS DOCUMENT;
SELECT * FROM t WHERE a IS NOT DOCUMENT;
SELECT XMLCONCAT(a,b);
SELECT XMLELEMENT(NAME a);
SELECT XMLELEMENT(NAME a,b,c);
SELECT XMLFOREST(a)
SELECT XMLPARSE(DOCUMENT 1 STRIP WHITESPACE)
SELECT XMLPARSE(DOCUMENT 1 PRESERVE WHITESPACE)
SELECT day '1900-1-1'
SELECT interval '1 year'
SELECT interval '1 year' YEAR
SELECT interval '1 year' MONTH
SELECT interval '1 year' DAY
SELECT interval '1 year' HOUR
SELECT interval '1 year' MINUTE
SELECT interval '1 year' SECOND
SELECT interval '1 year' YEAR TO MONTH
SELECT interval '1 year' DAY TO HOUR
SELECT interval '1 year' DAY TO MINUTE
SELECT interval '1 year' DAY TO SECOND
SELECT interval '1 year' HOUR TO MINUTE
SELECT interval '1 year' HOUR TO SECOND
VALUES (1, 2), (3, 4), (5, 6);
