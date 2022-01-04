Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

   [here](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.

# Docker Apache Incubator AGE Extension
** Unofficial - to visit official github repo go [here](https://github.com/apache/incubator-age) **

This is an image to build the AGE Extension on the official PostgreSQL 11 Docker image. 

## To run
#### If planning to use psql inside container

```docker run -it -e POSTGRES_PASSWORD=mypassword apache/incuabtor-age```
#### Connecting to the database server

```docker run -it -e POSTGRES_PASSWORD=mypassword -5432:5432 apache/incuabtor-age```

Notice the docker will always use 5432 within the docker. If you wish to access incubator-age using a different host port (because perhaps 5432 is busy) say 5455 you will enter

```docker run -it -e POSTGRES_PASSWORD=mypassword -5455:5432 apache/incuabtor-age```

Loading AGE

For psql, connect to your containerized Postgres instance at the default port 5432, then run:

```psql --username=postgres```

To connect psql your containerized Postgres instance at say 5455 as described above

```psql --username=postgres --port=5455```

Once psql is running or once you have connected to the database server you can load the extension as follows

```CREATE EXTENSION age;
LOAD 'age';
SET search_path = ag_catalog, "$user", public;
```

You can confirm the extension is loaded with the psql command
```\dx``` or confirm the AGE extension version with

```sql
SELECT extname, extversion FROM pg_extension WHERE extname='age';
 extname | extversion
---------+------------
 age     | 0.6.0
(1 row)
```


## Using AGE
First you will need to create a graph:

```
SELECT create_graph('my_graph_name');
```

To execute Cypher queries, you will need to wrap them in the following syntax:
```
SELECT * FROM cypher('my_graph_name', $$
  CypherQuery
$$) AS (a agtype);
```
For example, if we wanted to create a graph with 4 nodes, we could do something as shown below:
```
SELECT * FROM cypher('my_graph_name', $$
  CREATE (a:Part {part_num: '123'}), 
         (b:Part {part_num: '345'}), 
         (c:Part {part_num: '456'}), 
         (d:Part {part_num: '789'})
$$) AS (a agtype);
```

Then we could query the graph with the following:
```sql
SELECT * FROM cypher('my_graph_name', $$
  MATCH (a)
  RETURN a
$$) AS (a agtype);

                                          a
-------------------------------------------------------------------------------------
 {"id": 844424930131969, "label": "Part", "properties": {"part_num": "123"}}::vertex
 {"id": 844424930131970, "label": "Part", "properties": {"part_num": "345"}}::vertex
 {"id": 844424930131971, "label": "Part", "properties": {"part_num": "456"}}::vertex
 {"id": 844424930131972, "label": "Part", "properties": {"part_num": "789"}}::vertex
(4 rows)
```
Next, we could create a relationship between a couple of nodes:
```sql
SELECT * FROM cypher('my_graph_name', $$
  MATCH (a:Part {part_num: '123'}), (b:Part {part_num: '345'})
  CREATE (a)-[u:used_by { quantity: 1 }]->(b)
$$) AS (a agtype);
```

Next we can return the path we just created (results have been formatted for readability):
```
SELECT * FROM cypher('age', $$
  MATCH p=(a)-[]->(b)
  RETURN p
$$) AS (a agtype);

// RESULTS
[
   {
      "id":844424930131969,
      "label":"Part",
      "properties":{
         "part_num":"123"
      }
   }::"vertex",
   {
      "id":1125899906842625,
      "label":"used_by",
      "end_id":844424930131970,
      "start_id":844424930131969,
      "properties":{
         "quantity":1
      }
   }::"edge",
   {
      "id":844424930131970,
      "label":"Part",
      "properties":{
         "part_num":"345"
      }
   }::"vertex"
]::"path"

(1 row)
```
Then we could show all nodes in the graph with the following:

```sql
SELECT * FROM cypher('my_graph_name', $$
  MATCH (a)
  RETURN a
$$) AS (a agtype);

--- RESULTS
                                          a
-------------------------------------------------------------------------------------
 {"id": 844424930131969, "label": "Part", "properties": {"part_num": "123"}}::vertex
 {"id": 844424930131970, "label": "Part", "properties": {"part_num": "345"}}::vertex
 {"id": 844424930131971, "label": "Part", "properties": {"part_num": "456"}}::vertex
 {"id": 844424930131972, "label": "Part", "properties": {"part_num": "789"}}::vertex
(4 rows)
```
