
<h3 align="center">
    <a href="https://age.apache.org/age-manual/master/index.html" target="_blank">
        <img src="https://age.apache.org/age-manual/master/_static/logo.png" height="30%" alt="Apache AGE" style="margin: 0 0 -3px 0">
    </a>
   <h3 align="center"> Apache AGE is a leading multi-model graph database </h3> 
</h3>

<h3 align="center">Graph Processing & Analytics for Relational Databases</h3>

<p align="center">                                                                                                    
  <a href="https://github.com/apache/age/blob/master/LICENSE">
    <img src="https://img.shields.io/github/license/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/releases">
    <img src="https://img.shields.io/badge/Release-v1.5.0-FFA500?labelColor=gray&style=flat&link=https://github.com/apache/age/releases"/>
  </a>
  &nbsp;
  <a href="https://www.postgresql.org/docs/16/index.html">
    <img src="https://img.shields.io/badge/Version-Postgresql 16-00008B?labelColor=gray&style=flat&link=https://www.postgresql.org/docs/16/index.html"/>
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

<h2><img height="30" src="/img/AGE.png">&nbsp;&nbsp;What is Apache AGE?</h2>

[Apache AGE](https://age.apache.org/age-manual/master/index.html) is an extension for PostgreSQL that enables users to leverage a graph database on top of existing relational databases. AGE stands for A Graph Extension and is inspired by Bitnine's AgensGraph, a multi-model database fork of PostgreSQL. The project's basic principle is to create a single storage solution that handles both relational and graph data models, allowing users to use standard ANSI SQL alongside openCypher, one of the most popular graph query languages today. There is a strong need for cohesive, easy-to-implement multi-model databases. As an extension of PostgreSQL, AGE supports all the functionalities and features of PostgreSQL while also offering a graph model.

<p align="center">
    <img src="/img/age-01.png" width="80%" height="80%">
</p>

<h2><img height="30" src="/img/tick.svg">&nbsp;&nbsp;Overview</h2>

Apache AGE is:

- **Powerful**: adds graph database support to the already popular PostgreSQL database used by organizations including Apple, Spotify, and NASA.
- **Flexible**: allows you to perform openCypher queries, making complex queries much easier to write. It also enables querying multiple graphs simultaneously.
- **Intelligent**: enables graph queries that serve as the basis for many next-level web services such as fraud detection, master data management, product recommendations, identity and relationship management, experience personalization, knowledge management, and more.

<h2><img height="30" src="/img/features.svg">&nbsp;&nbsp;Features</h2>

<p align="center">
    <img src="/img/age-03.png" width="80%" height="80%">
</p>

- **Cypher Query**: supports the graph query language.
- **Hybrid Querying**: enables SQL and/or Cypher.
- **Querying**: enables multiple graphs.
- **Hierarchical**: graph label organization.
- **Property Indexes**: on both vertices (nodes) and edges.
- **Full PostgreSQL**: supports PG features.

<h2><img height="30" src="/img/documentation.svg">&nbsp;&nbsp;Documentation</h2>

Refer to our latest [Apache AGE documentation](https://age.apache.org/age-manual/master/index.html) to learn about installation, features, built-in functions, and Cypher queries.

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

<h4><a><img width="20" src="/img/pg.svg"></a>&nbsp;Install PostgreSQL</h4>

You will need to install an AGE compatible version of PostgreSQL. For now, AGE supports PostgreSQL versions 11, 12, 13, 14, 15, & 16. Supporting the latest versions is on AGE roadmap.

<h4>Installation via Package Manager</h4>

You can use a [package management](https://www.postgresql.org/download/) tool that your OS provides to download AGE.

```bash
sudo apt install postgresql
```

<h4>Installation From Source Code</h4>

You can [download the Postgres](https://www.postgresql.org/ftp/source/) source code and install your instance of Postgres. Read instructions on how to install from source code for different versions on the [official Postgres Website](https://www.postgresql.org/docs/16/installation.html).

<h4><img width="20" src="/img/tux.svg"><img width="20" src="/img/apple.svg">&nbsp;Install AGE on Linux and MacOS</h4>

Clone the [GitHub repository](https://github.com/apache/age) or download an [official release](https://github.com/apache/age/releases).
Run the pg_config utility and check the version of PostgreSQL. Currently, only PostgreSQL versions 11, 12, 13, 14, 15, & 16 are supported. If you have any other version of Postgres, you will need to install PostgreSQL version 11, 12, 13, 14, 15, or 16.

```bash
pg_config
```

Run the following command in the source code directory of Apache AGE to build and install the extension.

```bash
make install
```

If the path to your Postgres installation is not

 in the PATH variable, add the path in the arguments:

```bash
make PG_CONFIG=/path/to/postgres/bin/pg_config install
```

<h4><img width="30" src="/img/docker.svg">&nbsp;Run using Docker</h4>

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

Starting with Apache AGE is straightforward. You can easily select your platform and incorporate the relevant SDK into your code.

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

<h2><img height="20" src="/img/visualization.svg">&nbsp;&nbsp;Graph Visualization Tool for AGE</h2>

Apache AGE Viewer is a user interface for Apache AGE that provides visualization and exploration of data.
This web visualization tool allows users to enter complex graph queries and explore the results in graph and table forms.
Apache AGE Viewer is enhanced to proceed with extensive graph data and discover insights through various graph algorithms.
Apache AGE Viewer will become a graph data administration and development platform for Apache AGE to support multiple relational databases: [Apache AGE Viewer Repository](https://github.com/apache/age-viewer).

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
```
