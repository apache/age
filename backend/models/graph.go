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
