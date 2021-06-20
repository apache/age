package main

import (
	"fmt"
	"testing"

	"database/sql"

	_ "github.com/lib/pq"
)

var dsn string = "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=agens sslmode=disable"
var graphName string = "testGraph"

func TestAdditional(t *testing.T) {
	db, err := sql.Open("postgres", dsn)
	if err != nil {
		t.Fatal(err)
	}

	_, err = GetReady(db, graphName)
	if err != nil {
		t.Fatal(err)
	}

	cursor, err := db.Begin()
	err = ExecCypher(cursor, graphName, "CREATE (n:Person {name: '%s', weight:67.0})", "Joe")
	if err != nil {
		t.Fatal(err)
	}

	err = ExecCypher(cursor, graphName, "CREATE (n:Person {name: '%s', weight:77.3, roles:['Dev','marketing']})", "Jack")
	if err != nil {
		t.Fatal(err)
	}

	err = ExecCypher(cursor, graphName, "CREATE (n:Person {name: '%s', weight:59})", "Andy")
	if err != nil {
		t.Fatal(err)
	}
	cursor.Commit()

	cursor, err = db.Begin()

	cypherCursor, err := QueryCypher(cursor, graphName, "MATCH (n:Person) RETURN n")

	for cypherCursor.Next() {
		entity, err := cypherCursor.GetRow()
		if err != nil {
			t.Fatal(err)
		}
		vertex := entity.(*Vertex)
		fmt.Println(vertex.id, vertex.label, vertex.props)
	}

	err = ExecCypher(cursor, graphName, "MATCH (a:Person), (b:Person) WHERE a.name='%s' AND b.name='%s' CREATE (a)-[r:workWith {weight: %d}]->(b)",
		"Jack", "Joe", 3)
	if err != nil {
		t.Fatal(err)
	}

	err = ExecCypher(cursor, graphName, "MATCH (a:Person {name: '%s'}), (b:Person {name: '%s'}) CREATE (a)-[r:workWith {weight: %d}]->(b)",
		"Joe", "Andy", 7)
	if err != nil {
		t.Fatal(err)
	}

	cursor.Commit()

	cursor, err = db.Begin()

	cypherCursor, err = QueryCypher(cursor, graphName, "MATCH p=()-[:workWith]-() RETURN p")
	if err != nil {
		t.Fatal(err)
	}

	for cypherCursor.Next() {
		entity, err := cypherCursor.GetRow()
		if err != nil {
			t.Fatal(err)
		}

		path := entity.(*Path)
		fmt.Println(path.start, path.rel.props, path.start)
	}

	err = ExecCypher(cursor, graphName, "MATCH (n:Person) DETACH DELETE n RETURN *")
	if err != nil {
		t.Fatal(err)
	}
	cursor.Commit()
}

func TestQuery(t *testing.T) {
	ag, err := ConnectAge(graphName, dsn)

	if err != nil {
		t.Fatal(err)
	}

	tx, err := ag.Begin()
	if err != nil {
		t.Fatal(err)
	}

	err = tx.ExecCypher("CREATE (n:Person {name: '%s'})", "Joe")
	if err != nil {
		t.Fatal(err)
	}

	err = tx.ExecCypher("CREATE (n:Person {name: '%s', age: %d})", "Smith", 10)
	if err != nil {
		t.Fatal(err)
	}

	err = tx.ExecCypher("CREATE (n:Person {name: '%s', weight:%f})", "Jack", 70.3)
	if err != nil {
		t.Fatal(err)
	}

	tx.Commit()

	tx, err = ag.Begin()
	if err != nil {
		t.Fatal(err)
	}

	cursor, err := tx.QueryCypher("MATCH (n:Person) RETURN n")
	if err != nil {
		t.Fatal(err)
	}

	count := 0
	for cursor.Next() {
		entity, err := cursor.GetRow()
		if err != nil {
			t.Fatal(err)
		}
		count++
		vertex := entity.(*Vertex)
		fmt.Println(count, "]", vertex.id, vertex.label, vertex.props)
	}

	fmt.Println("Vertex Count:", count)

	err = tx.ExecCypher("MATCH (a:Person), (b:Person) WHERE a.name='%s' AND b.name='%s' CREATE (a)-[r:workWith {weight: %d}]->(b)",
		"Jack", "Joe", 3)
	if err != nil {
		t.Fatal(err)
	}

	err = tx.ExecCypher("MATCH (a:Person {name: '%s'}), (b:Person {name: '%s'}) CREATE (a)-[r:workWith {weight: %d}]->(b)",
		"Joe", "Smith", 7)
	if err != nil {
		t.Fatal(err)
	}

	tx.Commit()

	tx, err = ag.Begin()
	if err != nil {
		t.Fatal(err)
	}

	cursor, err = tx.QueryCypher("MATCH p=()-[:workWith]-() RETURN p")
	if err != nil {
		t.Fatal(err)
	}

	count = 0
	for cursor.Next() {
		entity, err := cursor.GetRow()
		if err != nil {
			t.Fatal(err)
		}
		count++
		path := entity.(*Path)
		fmt.Println(count, "]", path.start, path.rel.props, path.start)
	}

	err = tx.ExecCypher("MATCH (n:Person) DETACH DELETE n RETURN *")
	if err != nil {
		t.Fatal(err)
	}
	tx.Commit()
}

func TestQueryGraph(t *testing.T) {
	ag, err := ConnectAge(graphName, dsn)

	if err != nil {
		t.Fatal(err)
	}

	tx, err := ag.Begin()
	if err != nil {
		t.Fatal(err)
	}

	// Create Vertex
	err = tx.ExecCypher("CREATE (n:Person {name: '%s'})", "Joe")
	err = tx.ExecCypher("CREATE (n:Person {name: '%s', age: %d})", "Smith", 10)
	err = tx.ExecCypher("CREATE (n:Person {name: '%s', weight:%f})", "Jack", 70.3)

	tx.Commit()

	tx, err = ag.Begin()
	if err != nil {
		t.Fatal(err)
	}

	// Match
	cursor, err := tx.QueryCypher("MATCH (n:Person) RETURN n")

	enList, err := cursor.All()
	if err != nil {
		t.Fatal(err)
	}

	fmt.Println("Created Vertext count is ", len(enList))

	for idx, entity := range enList {
		vertex := entity.(*Vertex)
		fmt.Println(idx, ">", vertex.id, vertex.label, vertex.props)
	}

	// Create Path
	err = tx.ExecCypher("MATCH (a:Person), (b:Person) WHERE a.name='%s' AND b.name='%s' CREATE (a)-[r:workWith {weight: %d}]->(b)",
		"Jack", "Joe", 3)

	err = tx.ExecCypher("MATCH (a:Person {name: '%s'}), (b:Person {name: '%s'}) CREATE (a)-[r:workWith {weight: %d}]->(b)",
		"Joe", "Smith", 7)

	tx.Commit()

	tx, err = ag.Begin()
	if err != nil {
		t.Fatal(err)
	}

	// Query Path
	cursor, err = tx.QueryCypher("MATCH p=()-[:workWith]-() RETURN p")
	if err != nil {
		t.Fatal(err)
	}

	pathList, err := cursor.All()
	if err != nil {
		t.Fatal(err)
	}

	fmt.Println("Created Path count is ", len(pathList))

	for idx, entity := range pathList {
		path := entity.(*Path)
		fmt.Println(idx, ">", path.start, path.rel.props, path.start)
	}

	// Clear Data
	err = tx.ExecCypher("MATCH (n:Person) DETACH DELETE n RETURN *")
	if err != nil {
		t.Fatal(err)
	}
	tx.Commit()
}
