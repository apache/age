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

import PostgresClientKit


func setUpAge(connection:Connection, graphName: String){
    do{
        querySQL(statement: "Load 'age';", connection: connection)
        querySQL(statement: "SET search_path = ag_catalog, '$user', public;", connection: connection)
        let cursor = try connection.prepareStatement(text: "SELECT typelem FROM pg_type WHERE typname='_agtype'").execute();
        defer { cursor.close() }
        var oid:Int? = nil;
        for row in cursor {
            let columns = try row.get().columns
            oid = try columns[0].int()
            break;
        }
                               
        if oid == nil{
            // Will raise exception over here
        }     
    }catch{
        print(error)
    }
    
    checkGraphCreated(connection: connection, graphName: graphName)
}


func connectDatabase(connectionParam:[String:Any]) -> Connection?{
    do {
        var configuration = PostgresClientKit.ConnectionConfiguration()
        configuration.host = connectionParam["host"] as? String ?? "localhost"
        configuration.port = connectionParam["port"] as? Int ?? 5432
        configuration.user = connectionParam["user"] as? String ?? "postgres"
        configuration.ssl = false
        
        let key_exist = connectionParam["password"] != nil
        
        if key_exist{
            configuration.credential = .scramSHA256(password: connectionParam["password"] as? String ?? "postgres")
        }

    configuration.database = connectionParam["dbname"] as? String ?? "postgres"
    
    let connection =  try PostgresClientKit.Connection(configuration: configuration)
    
    return connection
    } catch {
        print(error) // better error handling goes here
    }
    return nil;
}


func checkGraphCreated(connection:Connection, graphName:String){
    do{
        let rows = fetchSQL(statement: "SELECT count(*) FROM ag_graph WHERE name='" + graphName + "'", connection: connection);
        print(rows)
        let row_counts = rows[0] as! [PostgresValue];
        let count = try row_counts[0].int();
        if count < 1{
            querySQL(statement: "SELECT create_graph('" + graphName + "');", connection: connection);
        }
    }catch{
        print(error)
    }
}


func querySQL(statement:String, connection:Connection){
    do{
        try connection.prepareStatement(text: statement).execute()
    }catch{
        print(error)
    }
}


func fetchSQL(statement:String, connection:Connection)-> [Any]{
    var rows:[Any] = [];
    do{
        let statement = try connection.prepareStatement(text: statement)
        defer { statement.close() }
        
        let cursor = try statement.execute()
        defer { cursor.close() }

        for row in cursor {
            rows.append(try row.get().columns);
        }
    }catch{
        print(error)
    }
    
    return rows;
}


class Age{
    var connection:Connection?;
    var graphName:String;
    
    init(){
        self.connection = nil;
        self.graphName = "";
    }
    
    
    func connect(connectionParam:[String:Any], graph:String){
        self.connection = connectDatabase(connectionParam: connectionParam)
        setUpAge(connection:self.connection!, graphName: graph);
    }
    
    func execSQL(statement:String){
        if self.connection == nil{
            return print("The connection is not yet established")
        }
        let rows = fetchSQL(statement: statement, connection: self.connection!)
        
        for column in rows{
            print(column)
        }
    } 
}
