CREATE TABLE t1(i int);
INSERT INTO T1 VALUES(1);
CREATE TEMP TABLE t1(i int);
SELECT * FROM t1;	-- should return 0 row
DROP TABLE t1;
SELECT * FROM t1;	-- should return 1 row

BEGIN;
CREATE TEMP TABLE t1(i int);
SELECT * FROM t1;	-- should return 0 row
DROP TABLE t1;
SELECT * FROM t1;	-- should return 1 row
END;

BEGIN;
CREATE TEMP TABLE t1(i int);
SELECT * FROM t1;	-- should return 0 row
aaa;
ABORT;

SELECT * FROM t1;	-- should return 1 row

CREATE TEMP TABLE t1(i int);
CREATE TEMP TABLE t2(i int);
CREATE TEMP TABLE t3(i int);
SELECT * FROM t1;	-- should return 0 row
DROP TABLE t1, t2, t3;
SELECT * FROM t1;	-- should return 1 row

BEGIN;
CREATE TEMP TABLE t1(i int);
CREATE TEMP TABLE t2(i int);
CREATE TEMP TABLE t3(i int);
SELECT * FROM t1;	-- should return 0 row
DROP TABLE t1, t2, t3;
SELECT * FROM t1;	-- should return 1 row
END;

DROP TABLE t1;
