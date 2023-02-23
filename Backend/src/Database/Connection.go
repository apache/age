package Connectionpackage

import (
	"database/sql"
	"fmt"

	_ "github.com/lib/pq"
)

func ConnectToDB() (*sql.DB, error) {
	db, err := sql.Open("postgres", ToURL())
	if err != nil {
		return nil, fmt.Errorf("failed to connect to database: %v", err)
	}
	if err := db.Ping(); err != nil {
		db.Close()
		return nil, fmt.Errorf("failed to ping database: %v", err)
	}
	return db, nil
}
