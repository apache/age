package models

import (
	"age-viewer-go/db"
	"database/sql"
	"errors"
)

type Graph struct {
	Name string
}

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
