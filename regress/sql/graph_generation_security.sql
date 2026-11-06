/* Graph generators must respect table INSERT privileges and RLS. */
LOAD 'age';
SET search_path = ag_catalog, public;

SELECT create_graph('gen_security');
SELECT * FROM cypher('gen_security',
    $$CREATE (:Node {key:'root'})-[:LINK]->(:Node {key:'target'})$$)
    AS (v agtype);

CREATE ROLE gen_reader;
GRANT USAGE ON SCHEMA ag_catalog, gen_security TO gen_reader;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA gen_security TO gen_reader;

SET ROLE gen_reader;
SELECT create_complete_graph('gen_security', 2, 'LINK', 'Node');
SELECT age_create_barbell_graph('gen_security', 3, 0, 'Node', NULL,
                                'LINK', NULL);
RESET ROLE;
SELECT count(*) FROM gen_security."Node";
SELECT count(*) FROM gen_security."LINK";

/* A missing edge INSERT grant must fail before any vertex is written. */
GRANT INSERT ON gen_security."Node" TO gen_reader;
SET ROLE gen_reader;
SELECT create_complete_graph('gen_security', 2, 'LINK', 'Node');
RESET ROLE;
SELECT count(*) FROM gen_security."Node";
SELECT count(*) FROM gen_security."LINK";

/* Column-level INSERT grants on every written column are sufficient. */
CREATE ROLE gen_columns;
GRANT USAGE ON SCHEMA ag_catalog, gen_security TO gen_columns;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA gen_security TO gen_columns;
GRANT INSERT (id, properties) ON gen_security."Node" TO gen_columns;
GRANT INSERT (id, start_id, end_id, properties)
    ON gen_security."LINK" TO gen_columns;

SET ROLE gen_columns;
SELECT create_complete_graph('gen_security', 2, 'LINK', 'Node');
RESET ROLE;
SELECT count(*) FROM gen_security."Node";
SELECT count(*) FROM gen_security."LINK";

/* Direct tuple insertion cannot apply the graph tables' INSERT policies. */
GRANT INSERT ON ALL TABLES IN SCHEMA gen_security TO gen_reader;
ALTER TABLE gen_security."_ag_label_vertex" ENABLE ROW LEVEL SECURITY;
ALTER TABLE gen_security."_ag_label_vertex" FORCE ROW LEVEL SECURITY;
ALTER TABLE gen_security."Node" ENABLE ROW LEVEL SECURITY;
ALTER TABLE gen_security."Node" FORCE ROW LEVEL SECURITY;
ALTER TABLE gen_security."_ag_label_edge" ENABLE ROW LEVEL SECURITY;
ALTER TABLE gen_security."_ag_label_edge" FORCE ROW LEVEL SECURITY;
ALTER TABLE gen_security."LINK" ENABLE ROW LEVEL SECURITY;
ALTER TABLE gen_security."LINK" FORCE ROW LEVEL SECURITY;

SET ROLE gen_reader;
SELECT create_complete_graph('gen_security', 2, 'LINK', 'Node');
SELECT age_create_barbell_graph('gen_security', 3, 0, 'Node', NULL,
                                'LINK', NULL);
RESET ROLE;
SELECT count(*) FROM gen_security."Node";
SELECT count(*) FROM gen_security."LINK";

SELECT drop_graph('gen_security', true);
DROP OWNED BY gen_reader, gen_columns;
DROP ROLE gen_reader, gen_columns;
