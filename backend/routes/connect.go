package routes

import (
	m "age-viewer-go/miscellaneous"
	"age-viewer-go/models"
	"fmt"

	"github.com/gorilla/sessions"
	"github.com/labstack/echo"
)

func ConnectToDb(c echo.Context) error {
	udata, err := models.FromRequestBody(c)
	m.HandleError(err, "unable to bind user data", c)
	fmt.Print("here")
	db, err := udata.GetConnection()
	m.HandleError(err, "could not establish connection", c)
	err = db.Ping()
	m.HandleError(err, "could not connect to db", c, 400)
	defer db.Close()
	sess := c.Get("database").(*sessions.Session)
	sess.Values["db"] = udata
	err = sess.Save(c.Request(), c.Response().Writer)
	m.HandleError(err, "could not save session", c, 400)

	return c.JSON(200, map[string]string{"status": "connected"})
}
