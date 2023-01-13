UPDATE t set a = 1;
UPDATE t set a = 3, b = 'a', c = NULL, d = DEFAULT;
UPDATE t set a = 1 FROM x;
UPDATE t set a = 1 FROM x,y,z;
UPDATE t set a = 1 FROM x where x < 100 and y > 200;
UPDATE ONLY t set a = 1;
UPDATE foo SET a = 1 RETURNING *;
UPDATE foo SET a = 1 RETURNING a;
UPDATE foo SET a = 1 RETURNING a,b;
UPDATE foo SET a = 1 WHERE CURRENT OF $1
UPDATE foo SET a = 1 WHERE CURRENT OF cur
