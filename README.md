<br>

<p align="center">
     <img src="https://age.apache.org/age-manual/master/_static/logo.png" width="30%" height="30%">
</p>
<br>

<h3 align="center">
    <a href="https://age.apache.org/age-manual/master/_static/logo.png" target="_blank">
        <img src="https://age.apache.org/age-manual/master/_static/logo.png" height="25" height="30% alt="Apache AGE style="margin: 0 0 -3px 0">
    </a>
    <a href="https://age.apache.org/age-manual/master/_static/logo.png" target="_blank">
    </a>
     is a leading multi-model graph database </h3>
     
</h3>

<h3 align="center">Graph Processing & Analytics for Relational Databases</h3>

<br>


</br>



<p align="center">                                                                                                    
  <a href="https://github.com/apache/age/blob/master/LICENSE">
    <img src="https://img.shields.io/github/license/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/releases">
    <img src="https://img.shields.io/badge/Release-v1.7.0-FFA500?labelColor=gray&style=flat&link=https://github.com/apache/age/releases"/>
  </a>
  &nbsp;
  <a href="https://www.postgresql.org/docs/18/index.html">
    <img src="https://img.shields.io/badge/Version-Postgresql 18-00008B?labelColor=gray&style=flat&link=https://www.postgresql.org/docs/18/index.html"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/issues">
    <img src="https://img.shields.io/github/issues/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/network/members">
    <img src="https://img.shields.io/github/forks/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/stargazers">
    <img src="https://img.shields.io/github/stars/apache/age"/>
  </a>

</p>

<br>


<h2><img height="30" src="/img/AGE.png">&nbsp;&nbsp;What is Apache AGE?</h2>

[Apache AGE](https://age.apache.org/#) is an extension for PostgreSQL that enables users to leverage a graph database on top of the existing relational databases. AGE is an acronym for A Graph Extension and is inspired by Bitnine's AgensGraph, a multi-model database fork of PostgreSQL. The basic principle of the project is to create a single storage that handles both the relational and graph data model so that the users can use the standard ANSI SQL along with openCypher, one of the most popular graph query languages today. There is a strong need for cohesive, easy-to-implement multi-model databases. As an extension of PostgreSQL, AGE supports all the functionalities and features of PostgreSQL while also offering a graph model to boot.
</br>
</br>
</br>

<p align="center">
<img src="/img/age-01.png" width="80%" height="80%">
</p>

</br>


<h2><img height="30" src="/img/tick.svg">&nbsp;&nbsp;Overview</h2>

Apache AGE is :

- **Powerful**: adds graph database support to the already popular PostgreSQL database: PostgreSQL is used by organizations including Apple, Spotify, and NASA.
- **Flexible**: allows you to perform openCypher queries, which makes complex queries much easier to write. It also enables querying multiple graphs at the same time.
- **Intelligent**: allows you to perform graph queries that are the basis for many next-level web services such as fraud detection, master data management, product recommendations, identity and relationship management, experience personalization, knowledge management, and more.

<h2><img height="30" src="/img/features.svg">&nbsp;&nbsp;Features</h2>
</br>
</br>

<p align="center">
<img src="/img/age-03.png" width="80%" height="80%">
</p>
</br>

- **Cypher Query**: supports graph query language
- **Hybrid Querying**: enables SQL and/or Cypher
- **Querying**: enables multiple graphs
- **Hierarchical**: graph label organization
- **Property Indexes**: on both vertices(nodes) and edges
- **Full PostgreSQL**: supports PG features



<h2><img height="30" src="/img/documentation.svg">&nbsp;&nbsp;Documentation</h2>

Refer to our latest [Apache AGE documentation](https://age.apache.org/age-manual/master/index.html) to learn about installation, features, built-in functions, and  Cypher queries.



<h2><img height="30" src="/img/installation.svg">&nbsp;&nbsp;Pre-Installation</h2>

Install the following essential libraries according to each OS. Building AGE from the source depends on the following Linux libraries (Ubuntu package names shown below):

- **CentOS**
```bash
yum install gcc glibc glib-common readline readline-devel zlib zlib-devel flex bison
```
- **Fedora**
```bash
dnf install gcc glibc bison flex readline readline-devel zlib zlib-devel
```
- **Ubuntu**
```bash
sudo apt-get install build-essential libreadline-dev zlib1g-dev flex bison
```

<h2><img height="30" src="/img/installation.svg">&nbsp;&nbsp;Installation</h2>

Apache AGE is intended to be simple to install and run. It can be installed with Docker and other traditional ways. 

<h4><a><img width="20" src="/img/pg.svg"></a>
&nbsp;Install PostgreSQL
</h4>

You will need to install an AGE compatible version of Postgres<a>, for now AGE supports Postgres 11, 12, 13, 14, 15, 16, 17 & 18. Supporting the latest versions is on AGE roadmap.

<h4>
&nbsp;Installation via Package Manager
</h4>

You can use a <a href="https://www.postgresql.org/download/">package management </a> that your OS provides to download PostgreSQL.

<br>

```bash
sudo apt install postgresql

```
<h4>
&nbsp;Installation From Source Code
</h4>

You can <a href="https://www.postgresql.org/ftp/source/"> download the Postgres </a> source code and install your own instance of Postgres. You can read instructions on how to install from source code for different versions on the <a href="https://www.postgresql.org/docs/18/installation.html">official Postgres Website.</a>



<h4><img width="20" src="/img/tux.svg"><img width="20" src="/img/apple.svg"> &nbsp;Install AGE on Linux and MacOS
</h4>

Clone the <a href="https://github.com/apache/age">github repository</a> or download the <a href="https://github.com/apache/age/releases">download an official release.
</a>
Run the pg_config utility and check the version of PostgreSQL. Currently, only PostgreSQL versions 11, 12, 13, 14, 15, 16, 17 & 18 are supported. If you have any other version of Postgres, you will need to install PostgreSQL version 11, 12, 13, 14, 15, 16, 17 & 18.
<br>
    
```bash
pg_config
```
Run the following command in the source code directory of Apache AGE to build and install the extension.  
     
```bash
make install
```
     
If the path to your Postgres installation is not in the PATH variable, add the path in the arguments:
```bash
make PG_CONFIG=/path/to/postgres/bin/pg_config install
```


<h4></a><img width="30" src="/img/docker.svg"></a>
&nbsp;Run using Docker
</h4>

<h5> Get the docker image </h5>

```bash
docker pull apache/age

```
<h5> Create AGE docker container </h5>

```bash
docker run \
    --name age  \
    -p 5455:5432 \
    -e POSTGRES_USER=postgresUser \
    -e POSTGRES_PASSWORD=postgresPW \
    -e POSTGRES_DB=postgresDB \
    -d \
    apache/age
```

<h5> Enter PostgreSQL's psql: </h5>

```bash
docker exec -it age psql -d postgresDB -U postgresUser
```



<h2><img height="20" src="/img/contents.svg">&nbsp;&nbsp;Post Installation</h2>

For every connection of AGE you start, you will need to load the AGE extension.

```bash
CREATE EXTENSION age;
```
```bash
LOAD 'age';
```
```bash
SET search_path = ag_catalog, "$user", public;
```

<h2><img height="20" src="/img/contents.svg">&nbsp;&nbsp;Using AGE with Non-Autocommit Clients (psycopg, JDBC, etc.)</h2>

If you are using AGE from a database client that does **not** default to autocommit — most commonly `psycopg` v3 or JDBC — you must understand how PostgreSQL's transaction semantics apply to AGE's setup and DDL-like functions. Otherwise, you may see graphs or labels that appear to be created successfully, but are not visible from new connections.

This is **not** a bug in AGE — it is standard PostgreSQL behavior. AGE's DDL-like functions write to the catalog, and catalog writes only become visible to other sessions after the enclosing transaction is committed.

### What is and isn't transactional

| Statement | Scope | Needs commit to be visible elsewhere? |
|---|---|---|
| `LOAD 'age'` | Session-local (loads the .so into the current backend) | No |
| `SET search_path = ag_catalog, "$user", public` | Session-local | No |
| `SELECT create_graph('g')` | **Writes** to `ag_graph` and creates a schema | **Yes** |
| `SELECT create_vlabel('g', 'L')` / `create_elabel(...)` | **Writes** to `ag_label` and creates a table | **Yes** |
| `SELECT drop_graph('g', true)` / `drop_label(...)` | **Writes** to catalog | **Yes** |
| `SELECT load_labels_from_file(...)` / `load_edges_from_file(...)` | **Writes** to catalog + data | **Yes** |
| `cypher('g', $$ CREATE (:L {...}) $$)` | **Writes** data | **Yes** |

In a client that defaults to autocommit (e.g. `psql`), every statement commits automatically, so this is never noticed. In a non-autocommit client, the first statement you run implicitly opens a transaction that stays open until you call `commit()`, `rollback()`, or close the connection.

### psycopg v3 — the "savepoint gotcha"

The common pitfall is that `with connection.transaction():` in psycopg does **not** start a new top-level transaction when one is already open — it creates a **savepoint** inside the existing outer transaction. Releasing a savepoint is not a commit, so your `create_graph` write stays invisible to other sessions until the outer transaction is explicitly committed.

#### ❌ Broken: graph is not visible from a new connection

```python
import psycopg

params = {"host": "localhost", "port": 5432, "user": "postgres",
          "password": "pw", "dbname": "mydb"}

# --- First connection ---
conn = psycopg.connect(**params)
conn.execute("LOAD 'age'")                                      # implicitly opens a txn
conn.execute("SET search_path = ag_catalog, '$user', public")

with conn.transaction(), conn.cursor() as cur:                  # <-- SAVEPOINT, not a real txn
    cur.execute("SELECT * FROM create_graph('my_graph')")
# outer transaction is STILL OPEN here

conn.close()  # outer transaction is rolled back on close → my_graph is gone

# --- New connection ---
conn = psycopg.connect(**params)
conn.execute("LOAD 'age'")
conn.execute("SET search_path = ag_catalog, '$user', public")
with conn.cursor() as cur:
    cur.execute("SELECT name FROM ag_graph;")
    # 'my_graph' is NOT in the results
```

#### ✅ Fix 1: explicit `commit()` after setup

```python
conn = psycopg.connect(**params)
conn.execute("LOAD 'age'")
conn.execute("SET search_path = ag_catalog, '$user', public")
conn.commit()   # <-- closes the implicit outer txn

with conn.transaction(), conn.cursor() as cur:
    cur.execute("SELECT * FROM create_graph('my_graph')")
# this transaction block is now top-level and commits on exit
conn.close()
```

#### ✅ Fix 2: enable autocommit on the connection

```python
conn = psycopg.connect(**params, autocommit=True)
conn.execute("LOAD 'age'")
conn.execute("SET search_path = ag_catalog, '$user', public")
conn.execute("SELECT * FROM create_graph('my_graph')")          # commits immediately
conn.close()
```

You can also toggle autocommit at runtime with `conn.set_autocommit(True)`.

### JDBC

JDBC connections also default to autocommit **true** per the JDBC spec, but many frameworks (Spring, etc.) flip it off. If you are running AGE DDL-like calls from JDBC, either:

```java
connection.setAutoCommit(true);
// ... LOAD 'age'; SET search_path ...; SELECT create_graph(...);
```

or keep autocommit off and explicitly commit after DDL-like calls:

```java
stmt.execute("LOAD 'age'");
stmt.execute("SET search_path = ag_catalog, \"$user\", public;");
stmt.execute("SELECT create_graph('my_graph');");
connection.commit();   // make the graph visible to other sessions
```

### Rule of thumb

> If an AGE call creates, drops, or modifies a graph, label, vertex, edge, or property, it is a **transactional write**. In a non-autocommit client, it will not be visible to other sessions until you explicitly `commit()`.

<h2><img height="20" src="/img/contents.svg">&nbsp;&nbsp;Quick Start</h2>

To create a graph, use the create_graph function located in the ag_catalog namespace.

```bash
SELECT create_graph('graph_name');
```

To create a single vertex with label and properties, use the CREATE clause.

```bash
SELECT * 
FROM cypher('graph_name', $$
    CREATE (:label {property:"Node A"})
$$) as (v agtype);
```

```bash
SELECT * 
FROM cypher('graph_name', $$
    CREATE (:label {property:"Node B"})
$$) as (v agtype);
```

To create an edge between two nodes and set its properties:

```bash
SELECT * 
FROM cypher('graph_name', $$
    MATCH (a:label), (b:label)
    WHERE a.property = 'Node A' AND b.property = 'Node B'
    CREATE (a)-[e:RELTYPE {property:a.property + '<->' + b.property}]->(b)
    RETURN e
$$) as (e agtype);
```

And to query the connected nodes:

```
SELECT * from cypher('graph_name', $$
        MATCH (V)-[R]-(V2)
        RETURN V,R,V2
$$) as (V agtype, R agtype, V2 agtype);
```

<h2><img height="20" src="/img/gettingstarted.svg">&nbsp;&nbsp;Language Specific Drivers</h2>

Starting with Apache AGE is very simple. You can easily select your platform and incorporate the relevant SDK into your code.
</br>
</br>

<p align="center">
<img src="/img/age-02.png" width="80%" height="80%">
</p>


<h4>Built-in</h4>

- [Go driver](./drivers/golang)
- [Java driver](./drivers/jdbc)
- [NodeJs driver](./drivers/nodejs)
- [Python driver](./drivers/python)

<h4>Community-driven Driver</h4>

- [Apache AGE Rust Driver](https://github.com/Dzordzu/rust-apache-age.git)
- [Apache AGE .NET Driver](https://github.com/Allison-E/pg-age)

<h2><img height="20" src="/img/visualization.svg">&nbsp;&nbsp;Graph Visualization Tool for AGE</h2>


Apache AGE Viewer is a user interface for Apache AGE that provides visualization and exploration of data.
This web visualization tool allows users to enter complex graph queries and explore the results in graph and table forms.
Apache AGE Viewer is enhanced to proceed with extensive graph data and discover insights through various graph algorithms.
Apache AGE Viewer will become a graph data administration and development platform for Apache AGE to support multiple relational databases: <https://github.com/apache/age-viewer>.

**This is a visualization tool.**
After installing AGE Extension, you may use this tool to get access to the visualization features.


![Viewer gdb, and graph](/img/agce.gif)


<h2><img height="20" src="/img/videos.png">&nbsp;&nbsp;Video Links</h2>

You can also get help from these videos. 

- Install on [Windows](https://www.youtube.com/watch?v=ddk8VX8Hm-I&list=PLGp3huJbWNDjgwP7s99Q-9_w1vxpjNHXG)
- Install on [MacOS](https://www.youtube.com/watch?v=0-qMwpDh0CA)



<h2><img height="20" src="/img/community.svg">&nbsp;&nbsp;Contributing</h2>

You can improve ongoing efforts or initiate new ones by sending pull requests to [this repository](https://github.com/apache/age).
Also, you can learn from the code review process, how to merge pull requests, and from code style compliance to documentation by visiting the [Apache AGE official site - Developer Guidelines](https://age.apache.org/contribution/guide).
Send all your comments and inquiries to the user mailing list, users@age.apache.org.
