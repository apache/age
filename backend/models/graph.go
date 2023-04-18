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
func (g *Graph) GetMetaData(conn *sql.DB, v int) (*sql.Rows, error) {

	defer conn.Close()
	switch v {
	case 11:
		return conn.Query(db.META_DATA_11, g.Name)
	case 12:
		return conn.Query(db.META_DATA_12, g.Name)
	default:
		return nil, errors.New("unsupported version")
	}
}
