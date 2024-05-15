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

import (
	"age-viewer-go/db"
	"database/sql"
	"errors"
)

// Graph is a struct that contains the name of a graph.
type Graph struct {
	Name string
}

/*
GetMetaData returns the metadata for a given graph instance g, based on the version of the database.
and returns a set of rows and an error (if any)
*/
func GetMetaData(conn *sql.DB, v int, dataChan chan<- *sql.Rows, errorChan chan<- error) {

	defer conn.Close()
	var data *sql.Rows
	var err error

	switch v {
	case 11:
		data, err = conn.Query(db.META_DATA_11)
	case 12:
		data, err = conn.Query(db.META_DATA_12)
	default:
		err = errors.New("unsupported version")
	}

	errorChan <- err
	dataChan <- data
}

// GetGraphNamesFromDB retrieves all unique graph names from the database
// and returns a slice of graph names, the first graph name, and an error (if any).
// The first graph name will be an empty string if there are no graph names.
func GetGraphNamesFromDB(conn *sql.DB) ([]string, string, error) {
	data, err := conn.Query(db.GET_ALL_GRAPHS)
	if err != nil {
		return nil, "", err
	}
	defer data.Close()
	graphNames := make([]string, 0)
	for data.Next() {
		var graphName string
		err := data.Scan(&graphName)
		if err != nil {
			return nil, "", err
		}
		graphNames = append(graphNames, graphName)
	}
	firstGraphName := ""
	if len(graphNames) > 0 {
		firstGraphName = graphNames[0]
	}
	return graphNames, firstGraphName, nil
}
