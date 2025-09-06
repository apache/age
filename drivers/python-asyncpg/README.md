# Apache AGE `asyncpg` Driver

Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.

[Apache AGE](https://age.apache.org/) is a PostgreSQL extension that provides graph database functionality. This repository hosts the development of an **asynchronous** Python driver for Apache AGE, built on top of the high-performance `asyncpg` library.

A graph consists of a set of vertices (also called nodes) and edges, where each individual vertex and edge possesses a map of properties. A vertex is the basic object of a graph, that can exist independently of everything else in the graph. An edge creates a directed connection between two vertices. A graph database stores relationship information as a first-class entity, which is useful when the meaning is in the relationships between the data. Apache AGE gives you the best of both worlds, relational and graph databases, simultaneously.

## Features
*   **Asynchronous:** Fully asynchronous, based on `asyncio` and `asyncpg`.
*   **AGType Parsing:** Unmarshals AGE result data (AGType) to Python objects like `Vertex`, `Edge`, and `Path`.
*   **Cypher Queries:** Execute openCypher queries against your AGE database.
*   **NetworkX Integration:** Convert between AGE graphs and `networkx` graphs.

## Prerequisites
*   Python 3.9+
*   An running instance of PostgreSQL with the Apache AGE extension installed.

## Installation

### From PyPI (once published)
```bash
pip install apache-age-asyncpg
```

### From source
```bash
git clone https://github.com/apache/age.git
cd age/drivers/python-asyncpg
pip install .
```

### Install for development
```bash
git clone https://github.com/apache/age.git
cd age/drivers/python-asyncpg
pip install -r requirements.txt
pip install -e .
```

## Testing
To run the test suite, you will need a running PostgreSQL instance with the AGE extension. The connection parameters are defined at the top of the test files (`test_age_py.py`, `test_networkx.py`).

```bash
# Make sure you have the test dependencies installed
pip install -e .[testing]

# Run the tests
pytest
```

## Usage
Here is a basic example of how to use the driver:

```python
import asyncio
import age

async def main():
    # Connect to the database
    conn = await age.connect(dsn="postgres://user:password@host:port/dbname", graph="my_graph")

    try:
        # Create a vertex
        await conn.execCypher("CREATE (n:Person {name: 'John Doe'})")

        # Query for the vertex
        result = await conn.execCypher("MATCH (n:Person) RETURN n")
        for row in result:
            vertex = row[0]
            print(f"Found vertex: {vertex.label} with properties {vertex.properties}")

    finally:
        # Close the connection
        await conn.close()

if __name__ == "__main__":
    asyncio.run(main())
```
For more examples, please see the sample Jupyter notebooks in the `samples/` directory. Note that you will need to adapt the synchronous code in the notebooks to be asynchronous.

## NetworkX Integration

### AGE to NetworkX
Convert an AGE graph to a `networkx.DiGraph`.

```python
import asyncio
import age
from age.networkx import age_to_networkx

async def main():
    conn = await age.connect(dsn="...", graph="my_graph")
    try:
        # Convert the entire graph
        G = await age_to_networkx(conn.connection, "my_graph")
        print(f"Graph has {G.number_of_nodes()} nodes and {G.number_of_edges()} edges.")
    finally:
        await conn.close()

asyncio.run(main())
```

### NetworkX to AGE
Insert a `networkx.DiGraph` into an AGE graph.

```python
import asyncio
import age
import networkx as nx
from age.networkx import networkx_to_age

async def main():
    # Create a NetworkX graph
    G = nx.DiGraph()
    G.add_node(1, label='Person', properties={'name': 'Alice'})
    G.add_node(2, label='Person', properties={'name': 'Bob'})
    G.add_edge(1, 2, label='KNOWS', properties={'since': 2020})

    conn = await age.connect(dsn="...", graph="my_graph")
    try:
        # Insert the graph into AGE
        await networkx_to_age(conn.connection, G, "my_graph")
        print("Graph inserted into AGE.")
    finally:
        await conn.close()

asyncio.run(main())
```

## License
Apache-2.0 License
