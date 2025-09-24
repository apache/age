from contextlib import contextmanager
import psycopg2
from psycopg2 import sql
import threading

from concurrent.futures import ThreadPoolExecutor
from threading import Semaphore

sqls = [
    """ SELECT * FROM cypher('test_graph', $$
	            MERGE (n:DDDDD {doc_id: 'f5ce4dc2'})
	            SET  n.embedset_id='ae1b9b73', n.doc_id='f5ce4dc2', n.doc_hash='977b56ef'
	             $$) as (result agtype) """,
    """ SELECT * FROM cypher('test_graph', $$
	            MERGE (n:EEEEE {doc_id: 'f5ce4dc2'})
	            SET  n.embedset_id='ae1b9b73', n.doc_id='f5ce4dc2', n.doc_hash='1d7e79a0'
	             $$) as (result agtype) """,
    """ SELECT * FROM cypher('test_graph', $$
	            MATCH (source:EEEEE {doc_id:'f5ce4dc2'})
	            MATCH (target:DDDDD {doc_id:'f5ce4dc2'})
	            WITH source, target
	            MERGE (source)-[r:DIRECTED]->(target)
	            SET  r.embedset_id='ae1b9b73', r.doc_id='f5ce4dc2'
	            RETURN r
	             $$) as (result agtype) """,
    """ SELECT * FROM cypher('test_graph', $$
	            MATCH (source:EEEEE {doc_id:'f5ce4dc2'})
	            MATCH (target:DDDDD {doc_id:'f5ce4dc2'})
	            WITH source, target
	            MERGE (source)-[r:DIRECTED]->(target)
	            SET  r.embedset_id='ae1b9b73', r.doc_id='f5ce4dc2'
	            RETURN r
	             $$) as (result agtype) """,
    """ SELECT * FROM cypher('test_graph', $$
	            MATCH (source:EEEEE {doc_id:'f5ce4dc2'})
	            MATCH (target:DDDDD {doc_id:'f5ce4dc2'})
	            WITH source, target
	            MERGE (source)-[r:DIRECTED]->(target)
	            SET  r.embedset_id='ae1b9b73', r.doc_id='f5ce4dc2'
	            RETURN r
	             $$) as (result agtype) """,
]


class PieGraphConnector:
    host: str
    port: str
    user: str
    password: str
    database: str
    warehouse: str

    def __init__(self, global_config: dict):
        self.host = global_config.get("host", "")
        self.port = global_config.get("port", "")
        self.user = global_config.get("user", "")
        self.password = global_config.get("password", "")
        self.database = global_config.get("database", "")
        self.warehouse = global_config.get("warehouse", "")

    @contextmanager
    def conn(self):
        conn = None
        if self.warehouse and self.warehouse != "":
            options = "'-c warehouse=" + self.warehouse + "'"
            conn = psycopg2.connect(
                dbname=self.database,
                user=self.user,
                password=self.password,
                host=self.host,
                port=self.port,
                options=options,
            )
        else:
            conn = psycopg2.connect(
                dbname=self.database,
                user=self.user,
                password=self.password,
                host=self.host,
                port=self.port,
            )
        conn.autocommit = True
        with conn.cursor() as cursor:
            cursor.execute("CREATE EXTENSION IF NOT EXISTS age;")
            cursor.execute("LOAD 'age';")
            cursor.execute("SET search_path = ag_catalog, '$user', public;")
        try:
            yield conn
        finally:
            conn.close()


class BoundedThreadPoolExecutor(ThreadPoolExecutor):
    def __init__(self, max_workers=5, max_task_size=32, *args, **kwargs):
        if max_task_size < max_workers:
            raise ValueError(
                "max_task_size should be greater than or equal to max_workers"
            )
        if max_workers is not None:
            kwargs["max_workers"] = max_workers
        super().__init__(*args, **kwargs)
        self._semaphore = Semaphore(max_task_size)

    def submit(self, fn, /, *args, **kwargs):
        timeout = kwargs.get("timeout", None)
        if self._semaphore.acquire(timeout=timeout):
            future = super().submit(fn, *args, **kwargs)
            future.add_done_callback(lambda _: self._semaphore.release())
            return future
        else:
            raise TimeoutError("waiting for semaphore timeout")


db_config = {
    "host": "127.0.0.1",
    "database": "postgres",
    "user": "postgres",
    "password": "",
    "port": "5432",
}

connector = PieGraphConnector(db_config)

def execute_sql(query):
    try:
        connection = psycopg2.connect(**db_config)
        cursor = connection.cursor()
        cursor.execute("CREATE EXTENSION IF NOT EXISTS age;")
        cursor.execute("LOAD 'age';")
        cursor.execute("SET search_path = ag_catalog, '$user', public;")

        cursor.execute(query)

        connection.commit()

        result = cursor.fetchall()

    except Exception as e:
        print(f"Error executing query '{query}': {e}")
    finally:
        if connection:
            cursor.close()
            connection.close()

semaphore_graph = threading.Semaphore(20)

drop_graph = "SELECT * FROM drop_graph('test_graph', true)"
create_graph = "SELECT * FROM create_graph('test_graph')"

# execute_sql(drop_graph)
execute_sql(create_graph)

def _merge_exec_sql(query: str):
    with semaphore_graph:
        with connector.conn() as conn:
            with conn.cursor() as cursor:
                try:
                    cursor.execute(query)
                    result = cursor.fetchall()
                    print(f"Result: OK\n")
                except Exception as e:
                    print(f"Error executing query '{query}': {e}")
        conn.commit()

with BoundedThreadPoolExecutor() as executor:
    executor.map(lambda q: _merge_exec_sql(q), sqls)

print("All threads have finished execution.")

execute_sql(drop_graph)
