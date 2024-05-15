/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 
package models

// struct that holds the graph metadata and user role information.
type MetaDataContainer struct {
	Graphs map[string]GraphData
}

// struct that contains separate slices for storing nodes and edges
type GraphData struct {
	Nodes []MetaData
	Edges []MetaData
}

// struct that represents a single row of metadata from the database.
// It contains information about the label, count, kind, namespace, namespace_id,
// graph, and relation.
type MetaData struct {
	Label        string
	Cnt          int
	Kind         string
	NameSpace    string
	Namespace_id int
	Graph        string
	Relation     string
}
