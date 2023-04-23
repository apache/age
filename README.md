# AgeSQL
Graph Database CLI based on PostgreSQL psql.

# Installation
To use this CLI, it is necessary to install PostgreSQL 15 as it is currently the only version compatible with the program's requirements.

To install PostgreSQL, use the following command in your terminal:

```
sudo apt install postgresql-15 postgresql-server-dev-15
```

The next step is export the following directories for file compilation.

- **Export Directories**

```
export PG_CONFIG=/path/to/postgresql/15/bin/pg_config
export PATH=/path/to/postgresql/15/bin:$PATH
export PGDATA=/path/to/postgresql/15/bin/data
export top_builddir=/path/to/postgresql-15
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/backend
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/backend/catalog
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/backend/utils
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/interfaces/libpq
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/port
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/common
C_INCLUDE_PATH=$C_INCLUDE_PATH:/path/to/postgresql-15/src/fe_utils
```
After exporting the directories, you need to run the ```make``` command to compile the file.
