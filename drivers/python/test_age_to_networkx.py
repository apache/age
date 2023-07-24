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
from age.exceptions import *
import unittest
from age.networkx import age_to_networkx
import networkx as nx

DSN = "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=agens"
TEST_GRAPH_NAME = "test"

class TestAgetoNetworkx(unittest.TestCase):
    ag = None
    def setUp(self):
        print("")
        print("Connecting to Test Graph.....")
        # Setting up connection to work with Graph
        self.ag = age.Age().connect(TEST_GRAPH_NAME, dsn=DSN)

    def tearDown(self):
        # Clear test data
        print("Deleting Test Graph.....")
        age.deleteGraph(self.ag.connection, self.ag.graphName)
        self.ag.close()

    def testexisting_graph_without_query(self):
        print("[Test AGE to NetworkX with Existing Graph]")
        ag = self.ag
        # Create nodes
        ag.execCypher("CREATE (n:Person {name: %s}) ", params=('Jack',))
        ag.execCypher("CREATE (n:Person {name: %s}) ", params=('Andy',))
        ag.execCypher("CREATE (n:Person {name: %s}) ", params=('Smith',))
        ag.commit()

        # Create Edges
        ag.execCypher("""MATCH (a:Person), (b:Person)
                      WHERE a.name = 'Andy' AND b.name = 'Jack'
                      CREATE (a)-[r:workWith {weight: 3}]->(b)""")
        ag.execCypher("""MATCH (a:Person), (b:Person) 
                    WHERE  a.name = %s AND b.name = %s 
                    CREATE p=((a)-[r:workWith {weight: 10}]->(b)) """, params=('Jack', 'Smith',))
        ag.commit()

        G = age_to_networkx(self.ag.connection, TEST_GRAPH_NAME)

        # Check that the G has the expected properties
        self.assertIsInstance(G, nx.DiGraph)

        # Check that the G has the correct number of nodes and edges
        self.assertEqual(len(G.nodes), 3)
        self.assertEqual(len(G.edges), 2)

        # Check that the node properties are correct
        for node in G.nodes:
            self.assertEqual(int, type(node))
            self.assertEqual(G.nodes[node]['label'], 'Person')
            self.assertIn('name', G.nodes[node]['properties'])
            self.assertIn('properties', G.nodes[node])
            self.assertEqual(str, type(G.nodes[node]['label']))

        # Check that the edge properties are correct
        for edge in G.edges:
            self.assertEqual(tuple, type(edge))
            self.assertEqual(int, type(edge[0]) and type(edge[1]))
            self.assertEqual(G.edges[edge]['label'], 'workWith')
            self.assertIn('weight', G.edges[edge]['properties'])
            self.assertEqual(int, type(G.edges[edge]['properties']['weight']))

    def testexisting_graph_with_query(self):
        print("[Test AGE to NetworkX with Existing Graph with Query]")
        ag = self.ag
        # Create nodes
        ag.execCypher("CREATE (n:Person {name: %s}) ", params=('Jack',))
        ag.execCypher("CREATE (n:Person {name: %s}) ", params=('Andy',))
        ag.execCypher("CREATE (n:Person {name: %s}) ", params=('Smith',))
        ag.commit()

        # Create Edges
        ag.execCypher("""MATCH (a:Person), (b:Person)
                      WHERE a.name = 'Andy' AND b.name = 'Jack'
                      CREATE (a)-[r:workWith {weight: 3}]->(b)""")
        ag.execCypher("""MATCH (a:Person), (b:Person) 
                    WHERE  a.name = %s AND b.name = %s 
                    CREATE p=((a)-[r:workWith {weight: 10}]->(b)) """, params=('Jack', 'Smith',))
        ag.commit()

        query = """SELECT * FROM cypher('%s', $$ MATCH (a:Person)-[r:workWith]->(b:Person)
        WHERE a.name = 'Andy'
        RETURN a, r, b $$) AS (a agtype, r agtype, b agtype);
        """ % (TEST_GRAPH_NAME)

        G = age_to_networkx(self.ag.connection, graphName=TEST_GRAPH_NAME, query=query)

        # Check that the G has the expected properties
        self.assertIsInstance(G, nx.DiGraph)

        # Check that the G has the correct number of nodes and edges
        self.assertEqual(len(G.nodes), 2)
        self.assertEqual(len(G.edges), 1)

        # Check that the node properties are correct
        for node in G.nodes:
            self.assertEqual(int, type(node))
            self.assertEqual(G.nodes[node]['label'], 'Person')
            self.assertIn('name', G.nodes[node]['properties'])
            self.assertIn('properties', G.nodes[node])
            self.assertEqual(str, type(G.nodes[node]['label']))

        # Check that the edge properties are correct
        for edge in G.edges:
            self.assertEqual(tuple, type(edge))
            self.assertEqual(int, type(edge[0]) and type(edge[1]))
            self.assertEqual(G.edges[edge]['label'], 'workWith')
            self.assertIn('weight', G.edges[edge]['properties'])
            self.assertEqual(int, type(G.edges[edge]['properties']['weight']))

    def testnon_existing_graph(self):
        print("[Test AGE to NetworkX with Non-Existing Graph]")
        ag = self.ag

        non_existing_graph = "non_existing_graph"
        # Check that the function raises an exception for non existing graph
        with self.assertRaises(GraphNotFound) as context:
            age_to_networkx(ag.connection, graphName=non_existing_graph)
        # Check the raised exception has the expected error message
        self.assertEqual(str(context.exception), non_existing_graph)
    
if __name__ == '__main__':
    unittest.main()