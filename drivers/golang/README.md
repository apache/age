# apache-age-go

Go driver support for [Apache AGE](https://age.apache.org/), graph extention for PostgreSQL.

### Prerequisites
* over Go 1.16
* This module runs on golang standard api [database/sql](https://golang.org/pkg/database/sql/) and [antlr4-python3](https://github.com/antlr/antlr4/tree/master/runtime/Go/antlr)


### Go get  
``` 
go get github.com/apache/incubator-age/drivers/golang
```
### gomod
``` 
require  github.com/apache/incubator-age/drivers/golang {version}
```


Check [latest version](https://github.com/apache/incubator-age/releases)

### For more information about [Apache AGE](https://age.apache.org/)
* Apache Incubator Age : https://age.apache.org/
* Github : https://github.com/apache/incubator-age
* Document : https://age.incubator.apache.org/docs/
* apache-age-go GitHub : https://github.com/rhizome-ai/apache-age-go

### Check AGE loaded on your PostgreSQL
Connect to your containerized Postgres instance and then run the following commands:
```(sql)
# psql 
CREATE EXTENSION age;
LOAD 'age';
SET search_path = ag_catalog, "$user", public;
```

### Test
Check out and rewrite DSN in incubator-age/drivers/golang/age_test.go
```
cd incubator-age/drivers/golang
go test -v

```

### Usage
```
import (
    "fmt"
    "database/sql"
	age "github.com/apache/incubator-age/drivers/golang"
)

func main() {
    var dsn string = "host={host} port={port} dbname={dbname} user={username} password={password} sslmode=disable"
    var graphName string = "{graph_path}"

    // Connect to PostgreSQL
	db, err := sql.Open("postgres", dsn)
	if err != nil {
		t.Fatal(err)
	}

    // Confirm graph_path created
	_, err = age.GetReady(db, graphName)
	if err != nil {
		t.Fatal(err)
	}

    // Tx begin for execute create vertex
	cursor, err := db.Begin()

    // Create vertices with Cypher
	err = age.ExecCypher(cursor, graphName, "CREATE (n:Person {name: '%s', weight:67.0})", "Joe")
	if err != nil {
		t.Fatal(err)
	}

	err = age.ExecCypher(cursor, graphName, "CREATE (n:Person {name: '%s', weight:77.3, roles:['Dev','marketing']})", "Jack")
	if err != nil {
		t.Fatal(err)
	}

	err = age.ExecCypher(cursor, graphName, "CREATE (n:Person {name: '%s', weight:59})", "Andy")
	if err != nil {
		t.Fatal(err)
	}
    
    // Commit Tx
	cursor.Commit()

    // Tx begin for queries
	cursor, err = db.Begin()

    // Query cypher
	cypherCursor, err := age.QueryCypher(cursor, graphName, "MATCH (n:Person) RETURN n")

    // Unmarsal result data to Vertex row by row
	for cypherCursor.Next() {
		entity, err := cypherCursor.GetRow()
		if err != nil {
			t.Fatal(err)
		}
		vertex := entity.(*age.Vertex)

		fmt.Println(vertex.id, vertex.label, vertex.props["name"], vertex.props)
	}

    // Create Paths (Edges)
	err = age.ExecCypher(cursor, graphName, "MATCH (a:Person), (b:Person) WHERE a.name='%s' AND b.name='%s' CREATE (a)-[r:workWith {weight: %d}]->(b)", "Jack", "Joe", 3)
	if err != nil {
		t.Fatal(err)
	}

	err = age.ExecCypher(cursor, graphName, "MATCH (a:Person {name: '%s'}), (b:Person {name: '%s'}) CREATE (a)-[r:workWith {weight: %d}]->(b)", "Joe", "Andy", 7)
	if err != nil {
		t.Fatal(err)
	}

	cursor.Commit()

	cursor, err = db.Begin()

    // Query Paths with Cypher
	cypherCursor, err = age.QueryCypher(cursor, graphName, "MATCH p=()-[:workWith]-() RETURN p")
	if err != nil {
		t.Fatal(err)
	}

	for cypherCursor.Next() {
		entity, err := cypherCursor.GetRow()
		if err != nil {
			t.Fatal(err)
		}

		path := entity.(*Path)
        startVertex = path.start
        edge = path.rel
        endVertex = path.end

		fmt.Println(startVertex, edge, endVertex)

		fmt.Println(startVertex.label, startVertex.props["name"], startVertex.props)
		fmt.Println(edge.label, edge.props["weight"], edge.props)
		fmt.Println(endVertex.label, endVertex.props["name"], endVertex.props)
	}

    // Delete Vertices 
	err = age.ExecCypher(cursor, graphName, "MATCH (n:Person) DETACH DELETE n RETURN *")
	if err != nil {
		t.Fatal(err)
	}
	cursor.Commit()
}
```
### License
Apache-2.0 License