-- autocommit

DROP TABLE autocommit;
CREATE TABLE autocommit (a int);
INSERT INTO autocommit SELECT generate_series(1, 10);

-- batch
DROP TABLE batch;
CREATE TABLE batch (a int);

-- column
DROP TABLE columntest;
CREATE TABLE columntest
(
  v1 int4,
  v2 int4,
  v3 int4,
  v4 int4,
  v5 int4,
  v6 int4,
  v7 int4,
  v8 int4,
  v9 int4,
  v10 int4,
  v11 int4,
  v12 int4,
  v13 int4,
  v14 int4,
  v15 int4)
WITHOUT OIDS;
INSERT INTO columntest VALUES (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);

-- lock
DROP TABLE locktest;
CREATE TABLE locktest (a int);
INSERT INTO locktest VALUES (1);
INSERT INTO locktest VALUES (2);
INSERT INTO locktest VALUES (3);
INSERT INTO locktest VALUES (4);
INSERT INTO locktest VALUES (5);

-- select
DROP TABLE sel;
CREATE TABLE sel (a int);
INSERT INTO sel SELECT generate_series(1, 100);

-- update
DROP TABLE up;
CREATE TABLE up (a int);
INSERT INTO up VALUES (1);
INSERT INTO up VALUES (2);

-- insert
DROP TABLE ins;
CREATE TABLE ins (a int);

-- prepareThreshold
DROP TABLE IF EXISTS prepare_threshold;
CREATE TABLE prepare_threshold (
  i int,
  d date,
  t timestamp with time zone DEFAULT now()
);
