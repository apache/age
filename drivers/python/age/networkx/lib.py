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

from age import *
import json
import psycopg
import networkx as nx
from psycopg import sql
from typing import Dict, Any, List, Set
from age.models import Vertex, Edge, Path
from age.age import validate_graph_name, validate_identifier


def checkIfGraphNameExistInAGE(connection: psycopg.connect,
                               graphName: str):
    """Check if the age graph exists"""
    validate_graph_name(graphName)
    with connection.cursor() as cursor:
        cursor.execute(
            sql.SQL("SELECT count(*) FROM ag_catalog.ag_graph WHERE name={gn}")
            .format(gn=sql.Literal(graphName))
        )
        if cursor.fetchone()[0] == 0:
            raise GraphNotFound(graphName)


def getOidOfGraph(connection: psycopg.connect,
                  graphName: str) -> int:
    """Returns oid of a graph"""
    validate_graph_name(graphName)
    try:
        with connection.cursor() as cursor:
            cursor.execute(
                sql.SQL("SELECT graphid FROM ag_catalog.ag_graph WHERE name={gn}")
                .format(gn=sql.Literal(graphName))
            )
            oid = cursor.fetchone()[0]
            return oid
    except Exception as e:
        print(e)


def get_vlabel(connection: psycopg.connect,
               graphName: str):
    node_label_list = []
    oid = getOidOfGraph(connection, graphName)
    try:
        with connection.cursor() as cursor:
            cursor.execute(
                sql.SQL("SELECT name FROM ag_catalog.ag_label WHERE kind='v' AND graph={oid}")
                .format(oid=sql.Literal(oid))
            )
            for row in cursor:
                node_label_list.append(row[0])

    except Exception as ex:
        print(type(ex), ex)
    return node_label_list


def create_vlabel(connection: psycopg.connect,
                  graphName: str,
                  node_label_list: List):
    """create_vlabels from list if not exist"""
    validate_graph_name(graphName)
    try:
        node_label_set = set(get_vlabel(connection, graphName))
        for label in node_label_list:
            if label in node_label_set:
                continue
            validate_identifier(label, "Vertex label")
            with connection.cursor() as cursor:
                cursor.execute(
                    sql.SQL("SELECT create_vlabel({gn},{lbl})")
                    .format(gn=sql.Literal(graphName), lbl=sql.Literal(label))
                )
        connection.commit()
    except Exception as e:
        raise Exception(e)


def get_elabel(connection: psycopg.connect,
               graphName: str):
    edge_label_list = []
    oid = getOidOfGraph(connection, graphName)
    try:
        with connection.cursor() as cursor:
            cursor.execute(
                sql.SQL("SELECT name FROM ag_catalog.ag_label WHERE kind='e' AND graph={oid}")
                .format(oid=sql.Literal(oid))
            )
            for row in cursor:
                edge_label_list.append(row[0])
    except Exception as ex:
        print(type(ex), ex)
    return edge_label_list


def create_elabel(connection: psycopg.connect,
                  graphName: str,
                  edge_label_list: List):
    """create_elabels from list if not exist"""
    validate_graph_name(graphName)
    try:
        edge_label_set = set(get_elabel(connection, graphName))
        for label in edge_label_list:
            if label in edge_label_set:
                continue
            validate_identifier(label, "Edge label")
            with connection.cursor() as cursor:
                cursor.execute(
                    sql.SQL("SELECT create_elabel({gn},{lbl})")
                    .format(gn=sql.Literal(graphName), lbl=sql.Literal(label))
                )
        connection.commit()
    except Exception as e:
        raise Exception(e)


def getNodeLabelListAfterPreprocessing(G: nx.DiGraph):
    """
        - Add default label if label is missing 
        - Add properties if not exist 
        - return all distinct node label
    """
    node_label_list = set()
    try:
        for node, data in G.nodes(data=True):
            if 'label' not in data:
                data['label'] = '_ag_label_vertex'
            if 'properties' not in data:
                data['properties'] = {}
            if not isinstance(data['label'], str):
                raise Exception(f"label of node : {node} must be a string")
            if not isinstance(data['properties'], Dict):
                raise Exception(f"properties of node : {node} must be a dict")
            if '__id__' not in data['properties'].keys():
                data['properties']['__id__'] = node
            node_label_list.add(data['label'])
    except Exception as e:
        raise Exception(e)
    return node_label_list


def getEdgeLabelListAfterPreprocessing(G: nx.DiGraph):
    """
        - Add default label if label is missing 
        - Add properties if not exist 
        - return all distinct edge label
    """
    edge_label_list = set()
    try:
        for u, v, data in G.edges(data=True):
            if 'label' not in data:
                data['label'] = '_ag_label_edge'
            if 'properties' not in data:
                data['properties'] = {}
            if not isinstance(data['label'], str):
                raise Exception(f"label of edge : {u}->{v} must be a string")
            if not isinstance(data['properties'], Dict):
                raise Exception(
                    f"properties of edge : {u}->{v} must be a dict")
            edge_label_list.add(data['label'])
    except Exception as e:
        raise Exception(e)
    return edge_label_list


def addAllNodesIntoAGE(connection: psycopg.connect, graphName: str, G: nx.DiGraph, node_label_list: Set):
    """Add all node to AGE"""
    validate_graph_name(graphName)
    try:
        queue_data = {label: [] for label in node_label_list}
        id_data = {}

        for node, data in G.nodes(data=True):
            json_string = json.dumps(data['properties'])
            queue_data[data['label']].append((json_string,))

        for label, rows in queue_data.items():
            validate_identifier(label, "Node label")
            insert_query = sql.SQL("INSERT INTO {schema}.{table} (properties) VALUES (%s) RETURNING id").format(
                schema=sql.Identifier(graphName),
                table=sql.Identifier(label)
            )
            cursor = connection.cursor()
            cursor.executemany(insert_query, rows, returning=True)
            ids = []
            while True:
                ids.append(cursor.fetchone()[0])
                if not cursor.nextset():
                    break

            id_data[label] = ids
            connection.commit()
            cursor.close()
            id_data[label].reverse()

        for node, data in G.nodes(data=True):
            data['properties']['__gid__'] = id_data[data['label']][-1]
            id_data[data['label']].pop()

    except Exception as e:
        raise Exception(e)


def addAllEdgesIntoAGE(connection: psycopg.connect, graphName: str, G: nx.DiGraph, edge_label_list: Set):
    """Add all edge to AGE"""
    validate_graph_name(graphName)
    try:
        queue_data = {label: [] for label in edge_label_list}
        for u, v, data in G.edges(data=True):
            json_string = json.dumps(data['properties'])
            queue_data[data['label']].append(
                (G.nodes[u]['properties']['__gid__'], G.nodes[v]['properties']['__gid__'], json_string,))

        for label, rows in queue_data.items():
            validate_identifier(label, "Edge label")
            insert_query = sql.SQL("INSERT INTO {schema}.{table} (start_id,end_id,properties) VALUES (%s, %s, %s)").format(
                schema=sql.Identifier(graphName),
                table=sql.Identifier(label)
            )
            cursor = connection.cursor()
            cursor.executemany(insert_query, rows)
            connection.commit()
            cursor.close()
    except Exception as e:
        raise Exception(e)


def addAllNodesIntoNetworkx(connection: psycopg.connect, graphName: str, G: nx.DiGraph):
    """Add all nodes to Networkx"""
    validate_graph_name(graphName)
    node_label_list = get_vlabel(connection, graphName)
    try:
        for label in node_label_list:
            validate_identifier(label, "Node label")
            with connection.cursor() as cursor:
                cursor.execute(
                    sql.SQL("SELECT id, CAST(properties AS VARCHAR) FROM {schema}.{table}")
                    .format(
                        schema=sql.Identifier(graphName),
                        table=sql.Identifier(label)
                    )
                )
                rows = cursor.fetchall()
                for row in rows:
                    G.add_node(int(row[0]), label=label,
                               properties=json.loads(row[1]))
    except Exception as e:
        print(e)


def addAllEdgesIntoNetworkx(connection: psycopg.connect, graphName: str, G: nx.DiGraph):
    """Add All edges to Networkx"""
    validate_graph_name(graphName)
    try:
        edge_label_list = get_elabel(connection, graphName)
        for label in edge_label_list:
            validate_identifier(label, "Edge label")
            with connection.cursor() as cursor:
                cursor.execute(
                    sql.SQL("SELECT start_id, end_id, CAST(properties AS VARCHAR) FROM {schema}.{table}")
                    .format(
                        schema=sql.Identifier(graphName),
                        table=sql.Identifier(label)
                    )
                )
                rows = cursor.fetchall()
                for row in rows:
                    G.add_edge(int(row[0]), int(
                        row[1]), label=label, properties=json.loads(row[2]))
    except Exception as e:
        print(e)
