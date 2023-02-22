package routes

import (
	"age-viewer-go/models"
	"fmt"

	"github.com/gorilla/sessions"
	"github.com/labstack/echo"
)

func ConnectToDb(c echo.Context) error {
	udata, err := models.FromRequestBody(c)
	if err != nil {
		return echo.NewHTTPError(400, "invalid data")
	}
	db, err := udata.GetConnection()
	if err != nil {
		return echo.NewHTTPError(400, fmt.Sprintf("could not establish connection due to %s", err.Error()))
	}
	err = db.Ping()
	if err != nil {
		return echo.NewHTTPError(400, fmt.Sprintf("could not establish connection due to %s", err.Error()))
	}
	defer db.Close()
	sess := c.Get("database").(*sessions.Session)
	sess.Values["db"] = udata
	err = sess.Save(c.Request(), c.Response().Writer)
	if err != nil {
		return echo.NewHTTPError(400, "could not save session")
	}

	return c.JSON(200, map[string]string{"status": "connected"})
}
