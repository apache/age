DELETE FROM t;
DELETE ;
DELETE FROM ONLY xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx;
DELETE FROM t WHERE x = 3;
DELETE FROM t WHERE x IN (select * from t1);
DELETE FROM foo WHERE a = 1 RETURNING *;
DELETE FROM foo WHERE a = 1 RETURNING a;
DELETE FROM foo WHERE a = 1 RETURNING a,b;
DELETE FROM foo WHERE CURRENT OF $1
DELETE FROM foo WHERE CURRENT OF cur
