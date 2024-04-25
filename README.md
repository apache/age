pg_graph: Powerful Graph Database based on postgresql
===================================================

pg_graph is a new generation multi-model graph database for the modern complex data environment. pg_graph is a multi-model database, which supports the relational and graph data model at the same time that enables developers to integrate the legacy relational data model and the flexible graph data model in one database. pg_graph supports ANSI-SQL and openCypher (http://www.opencypher.org). SQL queries and Cypher queries can be integrated into a single query in pg_graph.

pg_graph is based on the powerful PostgreSQL RDBMS, and is very robust, fully-featured and ready for enterprise use. pg_graph is optimized for handling complex connected graph data and provides plenty of powerful database features essential to the enterprise database environment including ACID transactions, multi-version concurrency control, stored procedure, triggers, constraints, sophisticated monitoring and a flexible data model (JSON). Moreover, pg_graph leverages the rich eco-systems of PostgreSQL and can be extended with many outstanding external modules, like PostGIS.

Building from the Source Code
-------------------
1. Clone pg_graph onto your local machine

2. Install the necessary libraries and dependencies:
    * CENTOS:
        ```sh
        $ yum install gcc glibc glib-common readline readline-devel zlib zlib-devel
        ```

    * Fedora:
        ```sh
        $ dnf install gcc glibc bison flex readline readline-devel zlib zlib-devel
        ```

    * RHEL:
        ```sh
        $ yum install gcc glibc glib-common readline readline-devel zlib zlib-devel flex bison
        ```

    * Ubuntu:
        ```sh
        $ sudo apt-get install build-essential libreadline-dev zlib1g-dev flex bison
        ```

    * macOS (install Xcode):
        ```bash
        $ xcode-select --install
        ```


3. Configure the source tree in /path/to/pg_graph:
    ```sh
    $ ./configure --prefix=$(pwd)
    ```
   >By default, `make install` will install all the files in `/usr/local/pgsql/bin`, `/usr/local/pgsql/lib` etc.  You want to specify an installation prefix to the current library.
   > If `configure` doesn't find any header with an error message, you can use `--with-includes=/path/to/headers` option.

4. Build & install pg_graph:
    * Build and install pg_graph engine
        ```sh
        $ make install
        ```
  
    * OPTIONAL: Build and install pg_graph along with other contrib and external modules (If you want to build pg_graph alone, run make install. This command builds pg_graph along with additional extensions):
        ```sh
        $ make install-world
        ```

    * OPTIONAL: Set the `PGGDATA` environment variable to easily configure pg_graph settings when necessary:
        ```sh
        $ echo "export PGGDATA=/path/to/pg_graph/data" >> ~/.bashrc
        ```

License
-------
* [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0)