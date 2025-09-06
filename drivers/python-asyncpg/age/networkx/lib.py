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

import json
import asyncpg
import networkx as nx
from typing import Dict, Any, List, Set
from age.models import Vertex, Edge, Path
from age.exceptions import *


async def checkIfGraphNameExistInAGE(connection: asyncpg.Connection, graphName: str):
    """Check if the age graph exists"""
    count = await connection.fetchval("SELECT count(*) FROM ag_catalog.ag_graph WHERE name = $1", graphName)
    if count == 0:
        raise GraphNotFound(graphName)


async def getOidOfGraph(connection: asyncpg.Connection, graphName: str) -> int:
    """Returns oid of a graph"""
    return await connection.fetchval("SELECT graphid FROM ag_catalog.ag_graph WHERE name = $1", graphName)


async def get_vlabel(connection: asyncpg.Connection, graphName: str):
    node_label_list = []
    oid = await getOidOfGraph(connection, graphName)
    rows = await connection.fetch("SELECT name FROM ag_catalog.ag_label WHERE kind='v' AND graph= $1", oid)
    for row in rows:
        node_label_list.append(row['name'])
    return node_label_list


async def create_vlabel(connection: asyncpg.Connection, graphName: str, node_label_list: List):
    """create_vlabels from list if not exist"""
    existing_labels = await get_vlabel(connection, graphName)
    async with connection.transaction():
        for label in node_label_list:
            if label not in existing_labels:
                # f-string is safe here because graphName and label are not from user input in this context
                await connection.execute(f"SELECT create_vlabel('{graphName}','{label}');")


async def get_elabel(connection: asyncpg.Connection, graphName: str):
    edge_label_list = []
    oid = await getOidOfGraph(connection, graphName)
    rows = await connection.fetch("SELECT name FROM ag_catalog.ag_label WHERE kind='e' AND graph=$1", oid)
    for row in rows:
        edge_label_list.append(row['name'])
    return edge_label_list


async def create_elabel(connection: asyncpg.Connection, graphName: str, edge_label_list: List):
    """create_elabels from list if not exist"""
    existing_labels = await get_elabel(connection, graphName)
    async with connection.transaction():
        for label in edge_label_list:
            if label not in existing_labels:
                await connection.execute(f"SELECT create_elabel('{graphName}','{label}');")


def getNodeLabelListAfterPreprocessing(G: nx.DiGraph):
    """
        - Add default label if label is missing
        - Add properties if not exist
        - return all distinct node label
    """
    node_label_list = set()
    for node, data in G.nodes(data=True):
        if 'label' not in data:
            data['label'] = '_ag_label_vertex'
        if 'properties' not in data:
            data['properties'] = {}
        if not isinstance(data['label'], str):
            raise TypeError(f"label of node : {node} must be a string")
        if not isinstance(data['properties'], Dict):
            raise TypeError(f"properties of node : {node} must be a dict")
        if '__id__' not in data['properties'].keys():
            data['properties']['__id__'] = node
        node_label_list.add(data['label'])
    return node_label_list


def getEdgeLabelListAfterPreprocessing(G: nx.DiGraph):
    """
        - Add default label if label is missing
        - Add properties if not exist
        - return all distinct edge label
    """
    edge_label_list = set()
    for u, v, data in G.edges(data=True):
        if 'label' not in data:
            data['label'] = '_ag_label_edge'
        if 'properties' not in data:
            data['properties'] = {}
        if not isinstance(data['label'], str):
            raise TypeError(f"label of edge : {u}->{v} must be a string")
        if not isinstance(data['properties'], Dict):
            raise TypeError(f"properties of edge : {u}->{v} must be a dict")
        edge_label_list.add(data['label'])
    return edge_label_list


async def addAllNodesIntoAGE(connection: asyncpg.Connection, graphName: str, G: nx.DiGraph, node_label_list: Set):
    """Add all node to AGE"""
    queue_data = {label: [] for label in node_label_list}
    id_data = {}

    for node, data in G.nodes(data=True):
        json_string = json.dumps(data['properties'])
        queue_data[data['label']].append((json_string,))

    async with connection.transaction():
        for label, rows in queue_data.items():
            table_name = f'"{graphName}"."{label}"'
            insert_query = f"INSERT INTO {table_name} (properties) VALUES ($1) RETURNING id"

            ids = []
            for row in rows:
                new_id = await connection.fetchval(insert_query, row[0])
                ids.append(new_id)

            id_data[label] = ids
            id_data[label].reverse()

    for node, data in G.nodes(data=True):
        data['properties']['__gid__'] = id_data[data['label']][-1]
        id_data[data['label']].pop()


async def addAllEdgesIntoAGE(connection: asyncpg.Connection, graphName: str, G: nx.DiGraph, edge_label_list: Set):
    """Add all edge to AGE"""
    queue_data = {label: [] for label in edge_label_list}
    for u, v, data in G.edges(data=True):
        json_string = json.dumps(data['properties'])
        queue_data[data['label']].append(
            (G.nodes[u]['properties']['__gid__'], G.nodes[v]['properties']['__gid__'], json_string,))

    async with connection.transaction():
        for label, rows in queue_data.items():
            table_name = f'"{graphName}"."{label}"'
            insert_query = f"INSERT INTO {table_name} (start_id,end_id,properties) VALUES ($1, $2, $3)"
            await connection.executemany(insert_query, rows)


async def addAllNodesIntoNetworkx(connection: asyncpg.Connection, graphName: str, G: nx.DiGraph):
    """Add all nodes to Networkx"""
    node_label_list = await get_vlabel(connection, graphName)
    for label in node_label_list:
        rows = await connection.fetch(f'SELECT id, CAST(properties AS VARCHAR) FROM "{graphName}"."{label}"')
        for row in rows:
            G.add_node(int(row['id']), label=label, properties=json.loads(row['properties']))


async def addAllEdgesIntoNetworkx(connection: asyncpg.Connection, graphName: str, G: nx.DiGraph):
    """Add All edges to Networkx"""
    edge_label_list = await get_elabel(connection, graphName)
    for label in edge_label_list:
        rows = await connection.fetch(f'SELECT start_id, end_id, CAST(properties AS VARCHAR) FROM "{graphName}"."{label}"')
        for row in rows:
            G.add_edge(int(row['start_id']), int(row['end_id']), label=label, properties=json.loads(row['properties']))
