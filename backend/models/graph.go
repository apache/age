package models

import (
	"age-viewer-go/db"
	"database/sql"
	"errors"
)

type Graph struct {
	Name string
}

func (g *Graph) GetMetaData(conn *sql.DB, v int, dataChan chan<- *sql.Rows, errorChan chan<- error) {

	defer conn.Close()
	var data *sql.Rows
	var err error

	switch v {
	case 11:
		data, err = conn.Query(db.META_DATA_11, g.Name)
	case 12:
		data, err = conn.Query(db.META_DATA_12, g.Name)
	default:
		err = errors.New("unsupported version")
	}

	errorChan <- err
	dataChan <- data
}
