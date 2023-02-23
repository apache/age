package main

import (
	"fmt"
	"log"

	Connectionpackage "github.com/apache/age/tree/ageviewer_go/Backend/src/Database"
	queryPackage "github.com/apache/age/tree/ageviewer_go/Backend/src/handleQuery"
)

func main() {

	db, err := Connectionpackage.ConnectToDB()
	if err != nil {
		log.Fatalf("failed to connect to database: %v", err)
	}
	defer db.Close()

	query := queryPackage.RetrieveQuery()
	if query == "" {
		log.Fatal("Empty Query")
	}

	result, err := queryPackage.QueryDB(db, query)
	if err != nil {
		log.Fatalf("failed to execute query: %v", err)
	}

	// Data for the given query is now present in result var
	// Can be used to print in the frontend.

	// Use following piece of code to print the result
	for i, row := range result {
		fmt.Printf("row %d:\n", i+1)
		for col, val := range row {
			fmt.Printf("  %s: %v\n", col, val)
		}
	}
}
