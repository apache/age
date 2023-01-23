//
//  age.swift
//  
//
//  Created by Fahad Zaheer on 16/01/2023.
//

import PostgresClientKit


func setUpAge(connection:Connection){
    do{
        try connection.prepareStatement(text: "Load 'age';").execute()
        try connection.prepareStatement(text: "SET search_path = ag_catalog, '$user', public;").execute()
        
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


func querySQL(statement:String, connection:Connection){
    do{
        let statement = try connection.prepareStatement(text: statement)
        defer { statement.close() }
        
        let cursor = try statement.execute()
        defer { cursor.close() }

        for row in cursor {
                let columns = try row.get().columns
                columns.forEach{column in
                    print(column)
                }
            }
    }catch{
        print(error)
    }
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
        setUpAge(connection:self.connection!)
    }
    
    func execSQL(statement:String){
        if self.connection == nil{
            return print("The connection is not yet established")
        }
        return querySQL(statement: statement, connection: self.connection!)
    }
    
    
}
