#!/usr/bin/env python3
"""Reproduce graph generator INSERT/RLS bypasses in a disposable AGE database."""

import argparse
import json
import uuid

import psycopg


def outcome(conn, sql):
    try:
        conn.execute(sql)
        return {"sqlstate": "00000"}
    except psycopg.Error as exc:
        return {"sqlstate": exc.sqlstate, "message": exc.diag.message_primary}


def counts(conn, graph):
    return {
        label: conn.execute(f'SELECT count(*) FROM {graph}."{label}"').fetchone()[0]
        for label in ("Node", "LINK")
    }


def run_case(connection, function, rls):
    suffix = uuid.uuid4().hex[:10]
    graph = f"generator_repro_{suffix}"
    role = f"generator_reader_{suffix}"
    report = {"function": function, "rls": rls}

    with psycopg.connect(user=connection["admin_user"], host=connection["host"],
                         dbname=connection["dbname"], autocommit=True) as admin:
        admin.execute("SET search_path=ag_catalog,public")
        admin.execute(f"CREATE ROLE {role} LOGIN NOSUPERUSER NOBYPASSRLS NOINHERIT")
        try:
            admin.execute(f"SELECT ag_catalog.create_graph('{graph}')")
            admin.execute(f"SELECT * FROM ag_catalog.cypher('{graph}', "
                          "$cy$CREATE (:Node {key:'root'})-[:LINK]->"
                          "(:Node {key:'target'})$cy$) AS (v agtype)")
            admin.execute(f"GRANT USAGE ON SCHEMA ag_catalog,{graph} TO {role}")
            admin.execute(f"GRANT USAGE ON ALL SEQUENCES IN SCHEMA {graph} TO {role}")
            if rls:
                admin.execute(f"GRANT INSERT ON ALL TABLES IN SCHEMA {graph} TO {role}")
                for label in ("_ag_label_vertex", "Node", "_ag_label_edge", "LINK"):
                    admin.execute(f'ALTER TABLE {graph}."{label}" ENABLE ROW LEVEL SECURITY')
                    admin.execute(f'ALTER TABLE {graph}."{label}" FORCE ROW LEVEL SECURITY')
            report["before"] = counts(admin, graph)

            with psycopg.connect(user=role, host=connection["host"],
                                 dbname=connection["dbname"], autocommit=True) as reader:
                reader.execute("SET search_path=ag_catalog,public")
                reader.execute("SET statement_timeout='2s'")
                report["identity"] = reader.execute(
                    "SELECT rolsuper,rolbypassrls FROM pg_roles WHERE rolname=current_user"
                ).fetchone()
                report["table_insert"] = {
                    label: reader.execute(
                        f"SELECT has_table_privilege(current_user,'{graph}.\"{label}\"','INSERT')"
                    ).fetchone()[0]
                    for label in ("Node", "LINK")
                }
                report["direct_insert"] = outcome(
                    reader, f'INSERT INTO {graph}."Node" DEFAULT VALUES')
                if function == "create_complete_graph":
                    call = (f"SELECT ag_catalog.create_complete_graph('{graph}', "
                            "2, 'LINK', 'Node')")
                else:
                    call = (f"SELECT ag_catalog.age_create_barbell_graph('{graph}', "
                            "3, 0, 'Node', NULL, 'LINK', NULL)")
                report["generator"] = outcome(reader, call)

            # A separate connection checks committed rows after the caller exits.
            with psycopg.connect(user=connection["admin_user"],
                                 host=connection["host"],
                                 dbname=connection["dbname"], autocommit=True) as verify:
                report["after"] = counts(verify, graph)
            report["bypass"] = (report["direct_insert"]["sqlstate"] == "42501"
                                and report["generator"]["sqlstate"] == "00000"
                                and report["after"] != report["before"])
        finally:
            admin.execute(f"SELECT ag_catalog.drop_graph('{graph}', true)")
            admin.execute(f"DROP OWNED BY {role}")
            admin.execute(f"DROP ROLE {role}")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--dbname", required=True)
    parser.add_argument("--admin-user", default="postgres")
    args = parser.parse_args()
    connection = vars(args)
    results = [
        run_case(connection, function, rls)
        for rls in (False, True)
        for function in ("create_complete_graph", "age_create_barbell_graph")
    ]
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
