package routes

import (
	"age-viewer-go/models"
	"fmt"
	"net/http"

	"github.com/gorilla/sessions"
	"github.com/labstack/echo"
)

/*
This function takes user data from the request body, establishes a database connection, saves the
connection information in the session, and it returns a JSON response with fields
containing the host, postgres version, port, database, user, password, list of graphs 
and current graph. It handles errors related to invalid data, connection establishment, and session saving.

*/
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
	udata.Graphs = []string{}
	sess.Values["db"] = udata
	err = sess.Save(c.Request(), c.Response().Writer)
	if err != nil {
		return echo.NewHTTPError(400, "could not save session")
	}

	return c.JSON(200, udata)
}

/*
DisconnectFromDb is used to disconnect from a database by removing the database
connection object from the user's session  and clearing the cookies. It returns a
JSON response with a status message indicating that the disconnection was successful.
*/
func DisconnectFromDb(c echo.Context) error {
	sess := c.Get("database").(*sessions.Session)
	dbObj := sess.Values["db"]
	if dbObj == nil {
		return echo.NewHTTPError(400, "no database connection found")
	}

	// Clear cookies
	for _, cookie := range c.Request().Cookies() {
		cookie := &http.Cookie{
			Name:   cookie.Name,
			Value:  "",
			Path:   "/",
			MaxAge: -1,
		}
		c.SetCookie(cookie)
	}

	sess.Values["db"] = nil
	err := sess.Save(c.Request(), c.Response().Writer)
	if err != nil {
		return echo.NewHTTPError(500, "error saving session")
	}
	return c.JSON(200, map[string]string{"status": "disconnected"})
}
