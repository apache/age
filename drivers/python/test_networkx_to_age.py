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
from age.models import *
import unittest
from age.networkx import networkx_to_age
import networkx as nx

DSN = "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=agens"
TEST_GRAPH_NAME = "test"

class TestNetworkxtoAge(unittest.TestCase):
    ag = None
    def setUp(self):
        print("")
        print("Connecting to Test Graph.....")
        # Setting up connection to work with Graph
        self.ag = age.Age().connect(TEST_GRAPH_NAME, dsn=DSN)
        self.graph = nx.DiGraph()

    def tearDown(self):
        # Clear test data
        print("Deleting Test Graph.....")
        age.deleteGraph(self.ag.connection, self.ag.graphName)
        self.ag.close()

    def testnetworkx_to_age(self):
        print("[Test NetworkX to AGE]")
        ag = self.ag

        # Create NetworkX graph
        self.graph.add_node(1, label='Person', properties={'name': 'Tito', 'age': '26', 'id': 1})
        self.graph.add_node(2, label='Person', properties={'name': 'Austen', 'age': '26', 'id': 2})
        self.graph.add_edge(1, 2, label='KNOWS', properties={'since': '1997', 'start_id': 1, 'end_id': 2})
        self.graph.add_node(3, label='Person', properties={'name': 'Eric', 'age': '28', 'id': 3})

        networkx_to_age(self.ag.connection, self.graph, TEST_GRAPH_NAME)

        # Check that node(s) were created
        cursor = ag.execCypher('MATCH (n) RETURN n')
        result = cursor.fetchall()
        # Check number of vertices created
        self.assertEqual(len(result), 3)
        # Checks if type of property in query output is a Vertex
        self.assertEqual(Vertex, type(result[0][0]))
        self.assertEqual(Vertex, type(result[1][0]))

        # Check that edge(s) was created
        cursor = ag.execCypher('MATCH ()-[e]->() RETURN e')
        result = cursor.fetchall()
        # Check number of edge(s) created
        self.assertEqual(len(result), 1)
        # Checks if type of property in query output is an Edge
        self.assertEqual(Edge, type(result[0][0]))

    def testinvalid_node_label(self):
        print("Test for invalid node label")
        self.graph.add_node(4, label=123, properties={'name': 'Mason', 'age': '24', 'id': 4})

        # Check that the function raises an exception for the invalid node label
        with self.assertRaises(Exception) as context:
            networkx_to_age(self.ag.connection, G=self.graph, graphName=TEST_GRAPH_NAME)
        # Check the raised exception has the expected error message
        self.assertEqual(str(context.exception), "label of node : 4 must be a string")

    def testinvalid_node_properties(self):
        print("Test for invalid node properties")
        self.graph.add_node(4, label='Person', properties="invalid")

        # Check that the function raises an exception for the invalid node properties
        with self.assertRaises(Exception) as context:
            networkx_to_age(self.ag.connection, G=self.graph, graphName=TEST_GRAPH_NAME)
        # Check the raised exception has the expected error message
        self.assertEqual(str(context.exception), "properties of node : 4 must be a dict")

    def testinvalid_edge_label(self):
        print("Test for invalid edge label")
        self.graph.add_edge(1, 2, label=123, properties={'since': '1997', 'start_id': 1, 'end_id': 2})

        # Check that the function raises an exception for the invalid edge label
        with self.assertRaises(Exception) as context:
            networkx_to_age(self.ag.connection, G=self.graph, graphName=TEST_GRAPH_NAME)
        # Check the raised exception has the expected error message
        self.assertEqual(str(context.exception), "label of edge : 1->2 must be a string")

    def testinvalid_edge_properties(self):
        print("Test for invalid edge properties")
        self.graph.add_edge(1, 2, label='KNOWS', properties="invalid")

        # Check that the function raises an exception for the invalid edge properties
        with self.assertRaises(Exception) as context:
            networkx_to_age(self.ag.connection, G=self.graph, graphName=TEST_GRAPH_NAME)
        # Check the raised exception has the expected error message
        self.assertEqual(str(context.exception), "properties of edge : 1->2 must be a dict")
    
if __name__ == '__main__':
    unittest.main()