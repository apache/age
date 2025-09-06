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
import asyncpg
import networkx as nx
from age.models import Vertex, Edge, Path
from .lib import *


async def age_to_networkx(connection: asyncpg.Connection,
                    graphName: str,
                    G: None | nx.DiGraph = None,
                    query: str | None = None
                    ) -> nx.DiGraph:
    """
    @params
    ---------------------
    connection - (asyncpg.Connection) Connection object
    graphName - (str) Name of the graph
    G - (networkx.DiGraph) Networkx directed Graph [optional]
    query - (str) Cypher query [optional]

        @returns
    ------------
    Networkx directed Graph

    """

    # Check if the age graph exists
    await checkIfGraphNameExistInAGE(connection, graphName)

    # Create an empty directed graph
    if G == None:
        G = nx.DiGraph()

    def addNodeToNetworkx(node):
        """Add Nodes in Networkx"""
        G.add_node(node.id,
                   label=node.label,
                   properties=node.properties)

    def addEdgeToNetworkx(edge):
        """Add Edge in Networkx"""
        G.add_edge(edge.start_id,
                   edge.end_id,
                   label=edge.label,
                   properties=edge.properties)

    def addPath(path):
        """Add Path in Networkx"""
        for x in path:
            if (isinstance(x, Path)):
                addPath(x)
        for x in path:
            if (isinstance(x, Vertex)):
                addNodeToNetworkx(x)
        for x in path:
            if (isinstance(x, Edge)):
                addEdgeToNetworkx(x)

    # Setting up connection to work with Graph
    await age.setUpAge(connection, graphName)

    if (query == None):
        await addAllNodesIntoNetworkx(connection, graphName, G)
        await addAllEdgesIntoNetworkx(connection, graphName, G)
    else:
        # The query here is a full SQL query, not just a cypher statement.
        rows = await connection.fetch(query)
        for row in rows:
            for x in row:
                if isinstance(x, Path):
                    addPath(x)
                elif isinstance(x, Edge):
                    addEdgeToNetworkx(x)
                elif isinstance(x, Vertex):
                    addNodeToNetworkx(x)
    return G
