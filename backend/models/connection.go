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
	"database/sql"
	"fmt"

	"github.com/labstack/echo"
	_ "github.com/lib/pq"
)

// Connection struct represents the connection parameters for a database
type Connection struct {
	Port      int
	Host      string
	Password  string
	User      string
	DbName    string
	SSL       string `default:"require"`
	GraphInit bool
	Version   int
	Graphs    []string
	Graph     string
}

/*
function takes an HTTP request context as input, extracts a Connection object object from
the request body, and returns it as a Connection struct instance along with an error.
*/
func FromRequestBody(context echo.Context) (*Connection, error) {
	user := new(Connection)
	e := context.Bind(&user)
	return user, e
}

/*
function generates and returns a string representing the SQL connection parameters for the
Connection struct instance, if "secure" parameter contains one or more values, the first value is used as
the SSL mode for the generated SQL connection string
*/
func (c *Connection) ToSQLString(secure ...string) string {
	if len(secure) > 0 {
		c.SSL = secure[0]
	}

	str := fmt.Sprintf(
		"user=%s port=%d host=%s password=%s sslmode=%s dbname=%s",
		c.User,
		c.Port,
		c.Host,
		c.Password,
		c.SSL,
		c.DbName,
	)

	return str
}

/*
Getconnection returns a pointer to a sql.DB instance by calling the sql.Open
function with the database connection string obtained by calling the ToSQLString method on a Connection struct.
*/
func (c Connection) GetConnection(secure ...string) (*sql.DB, error) {
	return sql.Open("postgres", c.ToSQLString(secure...))
}
