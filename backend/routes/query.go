package routes

import (
	"age-viewer-go/db"
	"age-viewer-go/models"
	"fmt"

	"github.com/gorilla/sessions"
	"github.com/labstack/echo"
)

func GraphQuery(c echo.Context) error {
	graph := models.Graph{}
	user := c.Get("database").(*sessions.Session).Values["db"].(*models.Connection)
	c.Bind(&graph)
	if user == nil {
		return echo.NewHTTPError(403, "no session")
	}

	conn, err := user.GetConnection("false")

	if err != nil {
		return echo.NewHTTPError(400, err.Error())
	}
	if err = conn.Ping(); err != nil {
		return echo.NewHTTPError(400, "could not establish connection with database")
	}
	defer conn.Close()
	if !user.GraphInit {
		_, err = conn.Exec(db.INIT_EXTENSION)
		if err != nil {
			msg := fmt.Sprintf("could not communicate with db\nerror: %s", err.Error())
			return echo.NewHTTPError(400, msg)
		}
		user.GraphInit = true
	}
	if user.Version == "0" {
		rows, err := conn.Query(db.PG_VERSION)
		if err != nil {
			fmt.Print(err.Error())
		}
		var version float32
		for rows.Next() {
			rows.Scan(&version)
		}
		user.Version = fmt.Sprintf("%f", version)
	}
	return nil
}
