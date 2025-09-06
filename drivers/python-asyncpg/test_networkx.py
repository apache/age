# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import age
import pytest
import pytest_asyncio
import networkx as nx
from age.models import *
from age.networkx import *
from age.exceptions import *


TEST_GRAPH_NAME = "test_graph"
ORIGINAL_GRAPH = "original_graph"
EXPECTED_GRAPH = "expected_graph"

TEST_HOST = "127.0.0.1"
TEST_PORT = 5432
TEST_DB = "postgres"
TEST_USER = "postgres"
TEST_PASSWORD = "agens"

@pytest_asyncio.fixture
async def ag_for_age_to_nx():
    dsn = f"postgres://{TEST_USER}:{TEST_PASSWORD}@{TEST_HOST}:{TEST_PORT}/{TEST_DB}"
    age_conn = age.Age()
    try:
        await age_conn.connect(dsn=dsn, graph=TEST_GRAPH_NAME)
    except Exception as e:
        print(f"Database connection failed: {e}")
        pytest.skip(f"Could not connect to database: {e}")

    yield age_conn

    try:
        await age.deleteGraph(age_conn.connection, TEST_GRAPH_NAME)
    except Exception:
        pass # Graph might not exist
    await age_conn.close()

@pytest_asyncio.fixture
async def ag_for_nx_to_age():
    dsn = f"postgres://{TEST_USER}:{TEST_PASSWORD}@{TEST_HOST}:{TEST_PORT}/{TEST_DB}"
    ag_conn = age.Age()
    ag1_conn = age.Age()
    ag2_conn = age.Age()

    try:
        await ag_conn.connect(dsn=dsn, graph=TEST_GRAPH_NAME)
        await ag1_conn.connect(dsn=dsn, graph=ORIGINAL_GRAPH)
        await ag2_conn.connect(dsn=dsn, graph=EXPECTED_GRAPH)
    except Exception as e:
        print(f"Database connection failed: {e}")
        pytest.skip(f"Could not connect to database: {e}")


    yield ag_conn, ag1_conn, ag2_conn

    for c in [ag_conn, ag1_conn, ag2_conn]:
        try:
            await age.deleteGraph(c.connection, c.graphName)
        except Exception:
            pass
        await c.close()


@pytest.mark.asyncio
class TestAgeToNetworkx:

    def compare_networkX(self, G, H):
        return nx.is_isomorphic(G, H, node_match=lambda g,h: g==h, edge_match=lambda g,h: g==h)

    async def test_empty_graph(self, ag_for_age_to_nx):
        print('Testing AGE to Networkx for empty graph')
        G = nx.DiGraph()
        H = await age_to_networkx(ag_for_age_to_nx.connection, TEST_GRAPH_NAME)
        assert self.compare_networkX(G, H)

    async def test_existing_graph_without_query(self, ag_for_age_to_nx):
        print('Testing AGE to Networkx for non empty graph without query')
        ag = ag_for_age_to_nx
        async with ag.connection.transaction():
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name":"Jack"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name":"Andy"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name":"Smith"})
            await ag.execCypher("MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE (a)-[r:workWith {weight: $weight}]->(b)",
                                params={"a_name": "Andy", "b_name": "Jack", "weight": 3})
            await ag.execCypher("MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE p=((a)-[r:workWith {weight: $weight}]->(b))",
                                params={"a_name": "Jack", "b_name": "Smith", "weight": 10})

        G = await age_to_networkx(ag.connection, TEST_GRAPH_NAME)

        assert isinstance(G, nx.DiGraph)
        assert len(G.nodes) == 3
        assert len(G.edges) == 2

    async def test_existing_graph_with_query(self, ag_for_age_to_nx):
        print('Testing AGE to Networkx for non empty graph with query')
        ag = ag_for_age_to_nx
        async with ag.connection.transaction():
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name":"Jack"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name":"Andy"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name":"Smith"})
            await ag.execCypher("MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE (a)-[r:workWith {weight: $weight}]->(b)",
                                params={"a_name": "Andy", "b_name": "Jack", "weight": 3})
            await ag.execCypher("MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE p=((a)-[r:workWith {weight: $weight}]->(b))",
                                params={"a_name": "Jack", "b_name": "Smith", "weight": 10})

        query = f"SELECT * FROM cypher('{TEST_GRAPH_NAME}', $$ MATCH (a:Person)-[r:workWith]->(b:Person) WHERE a.name = 'Andy' RETURN a, r, b $$) AS (a agtype, r agtype, b agtype);"
        G = await age_to_networkx(ag.connection, graphName=TEST_GRAPH_NAME, query=query)

        assert isinstance(G, nx.DiGraph)
        assert len(G.nodes) == 2
        assert len(G.edges) == 1

    async def test_non_existing_graph(self, ag_for_age_to_nx):
        print("Testing AGE to NetworkX for non-existing graph")
        ag = ag_for_age_to_nx
        non_existing_graph = "non_existing_graph"
        with pytest.raises(GraphNotFound) as context:
            await age_to_networkx(ag.connection, graphName=non_existing_graph)
        assert str(context.value) == non_existing_graph


@pytest.mark.asyncio
class TestNetworkxToAGE:
    async def compare_age(self, age1, age2):
        g_nodes = await age1.execCypher("MATCH (v) RETURN count(v)")
        g_edges = await age1.execCypher("MATCH ()-[r]->() RETURN count(r)")
        h_nodes = await age2.execCypher("MATCH (v) RETURN count(v)")
        h_edges = await age2.execCypher("MATCH ()-[r]->() RETURN count(r)")

        return g_nodes[0][0] == h_nodes[0][0] and g_edges[0][0] == h_edges[0][0]

    async def test_number_of_nodes_and_edges(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for number of nodes and edges")
        ag, _, _ = ag_for_nx_to_age
        graph = nx.DiGraph()
        graph.add_node(1, label='Person', properties={'name': 'Moontasir', 'age': '26', 'id': 1})
        graph.add_node(2, label='Person', properties={'name': 'Austen', 'age': '26', 'id': 2})
        graph.add_edge(1, 2, label='KNOWS', properties={'since': '1997', 'start_id': 1, 'end_id': 2})
        graph.add_node(3, label='Person', properties={'name': 'Eric', 'age': '28', 'id': 3})

        await networkx_to_age(ag.connection, graph, TEST_GRAPH_NAME)

        result = await ag.execCypher('MATCH (n) RETURN n')
        assert len(result) == 3

        result = await ag.execCypher('MATCH ()-[e]->() RETURN e')
        assert len(result) == 1

    async def test_empty_graph(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for empty Graph")
        _, ag1, ag2 = ag_for_nx_to_age
        G = nx.DiGraph()
        await networkx_to_age(ag1.connection, G, ORIGINAL_GRAPH)
        assert await self.compare_age(ag1, ag2)

    async def test_non_empty_graph(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for non-empty Graph")
        _, ag1, ag2 = ag_for_nx_to_age

        async with ag2.connection.transaction():
            await ag2.execCypher("CREATE (:l1 {name: $name, weight: $weight})", params={"name": "n1", "weight": "5"})
            await ag2.execCypher("CREATE (:l1 {name: $name, weight: $weight})", params={"name": "n2", "weight": "4"})
            await ag2.execCypher("CREATE (:l1 {name: $name, weight: $weight})", params={"name": "n3", "weight": "9"})
            await ag2.execCypher("MATCH (a:l1 {name: $a_name}), (b:l1 {name: $b_name}) CREATE (a)-[e:e1 {property:$prop}]->(b)",
                                params={"a_name": "n1", "b_name": "n2", "prop": "graph"})
            await ag2.execCypher("MATCH (a:l1 {name: $a_name}), (b:l1 {name: $b_name}) CREATE (a)-[e:e2 {property:$prop}]->(b)",
                                params={"a_name": "n2", "b_name": "n3", "prop": "node"})

        G = nx.DiGraph()
        G.add_node('1', label='l1', properties={'name': 'n1', 'weight': '5'})
        G.add_node('2', label='l1', properties={'name': 'n2', 'weight': '4'})
        G.add_node('3', label='l1', properties={'name': 'n3', 'weight': '9'})
        G.add_edge('1', '2', label='e1', properties={'property': 'graph'})
        G.add_edge('2', '3', label='e2', properties={'property': 'node'})

        await networkx_to_age(ag1.connection, G, ORIGINAL_GRAPH)
        assert await self.compare_age(ag1, ag2)

    async def test_invalid_node_label(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for invalid node label")
        ag, _, _ = ag_for_nx_to_age
        graph = nx.DiGraph()
        graph.add_node(4, label=123, properties={'name': 'Mason', 'age': '24', 'id': 4})
        with pytest.raises(TypeError) as context:
            await networkx_to_age(ag.connection, G=graph, graphName=TEST_GRAPH_NAME)
        assert str(context.value) == "label of node : 4 must be a string"

    async def test_invalid_node_properties(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for invalid node properties")
        ag, _, _ = ag_for_nx_to_age
        graph = nx.DiGraph()
        graph.add_node(4, label='Person', properties="invalid")
        with pytest.raises(TypeError) as context:
            await networkx_to_age(ag.connection, G=graph, graphName=TEST_GRAPH_NAME)
        assert str(context.value) == "properties of node : 4 must be a dict"

    async def test_invalid_edge_label(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for invalid edge label")
        ag, _, _ = ag_for_nx_to_age
        graph = nx.DiGraph()
        graph.add_node(1)
        graph.add_node(2)
        graph.add_edge(1, 2, label=123, properties={'since': '1997', 'start_id': 1, 'end_id': 2})
        with pytest.raises(TypeError) as context:
            await networkx_to_age(ag.connection, G=graph, graphName=TEST_GRAPH_NAME)
        assert str(context.value) == "label of edge : 1->2 must be a string"

    async def test_invalid_edge_properties(self, ag_for_nx_to_age):
        print("Testing Networkx To AGE for invalid edge properties")
        ag, _, _ = ag_for_nx_to_age
        graph = nx.DiGraph()
        graph.add_node(1)
        graph.add_node(2)
        graph.add_edge(1, 2, label='KNOWS', properties="invalid")
        with pytest.raises(TypeError) as context:
            await networkx_to_age(ag.connection, G=graph, graphName=TEST_GRAPH_NAME)
        assert str(context.value) == "properties of edge : 1->2 must be a dict"
