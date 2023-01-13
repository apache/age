# named parameter
SELECT func(a := 7, b := 12);
# order by aggrigator
SELECT array_agg(a ORDER BY b, c);
# variadic from html/xfunc-sql.html
SELECT mleast(VARIADIC ARRAY[10, -1, 5, 4.4]);
# window function
SELECT avg(c1) OVER (ROWS 1 PRECEDING) FROM r1;
SELECT avg(c1) OVER w1 FROM r1 WINDOW w1 AS (ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
# offset
SELECT * FROM t1 OFFSET 1 ROWS FETCH FIRST 2 ROWS ONLY;
SELECT * FROM t1 FETCH NEXT 2 ROWS ONLY;
# DO
DO LANGUAGE PLPGSQL $$BEGIN RAISE NOTICE 'hello'; END$$;

\pset server_version 9.0.0
# explain
# old syntax
EXPLAIN ANALYZE VERBOSE SELECT 1;
# new syntax
EXPLAIN (ANALYZE true,VERBOSE true,COSTS true,BUFFERS true,FORMAT XML) SELECT 1;
EXPLAIN (ANALYZE false,VERBOSE false,COSTS false,BUFFERS false,FORMAT JSON) SELECT 1;
# copy
# old syntax
COPY t FROM stdin;
COPY t FROM 'xxx' BINARY;
COPY t FROM 'xxx' CSV HEADER QUOTE AS '#' ESCAPE AS '$' FORCE NOT NULL foo,bar;
COPY t FROM 'xxx' OIDS;
COPY t FROM 'xxx' DELIMITER ',';
COPY t FROM 'xxx' NULL 'x';
# new syntax
COPY t FROM stdin (FORMAT 'text');
COPY t FROM stdin (FORMAT 'csv');
COPY t FROM stdin (FORMAT 'binary');
COPY t FROM 'f' (OIDS false);
COPY t FROM 'f' (OIDS true, DELIMITER '|', NULL '<N>', HEADER false);
COPY t FROM 'f' (HEADER true, QUOTE '"', ESCAPE '@');
COPY t FROM 'f' (FORCE_QUOTE *, FORCE_NOT_NULL(one, two, three));
