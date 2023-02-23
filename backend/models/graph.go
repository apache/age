package models

import (
	"age-viewer-go/db"

	"github.com/labstack/echo"
)

type Graph struct {
	Name string
}

func (g *Graph) GetMetaData(user Connection) error {
	conn, err := user.GetConnection("false")
	if err != nil {
		return echo.NewHTTPError(400, "unable to get metadata")
	}
	err = conn.Ping()
	if err != nil {
		return echo.NewHTTPError(400, "unable to connect to db")
	}
	defer conn.Close()

	conn.Query(db.INIT_EXTENSION)
}
