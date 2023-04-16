package session

import (
	"github.com/labstack/echo"
)

/*
Store is a global variable that holds the session store, it is initialized in the
main function. UserSessions is a middleware that checks if the session is valid, and
if it is, it adds the session to the context, so that it can be used in the handlers, and the routes.
*/
func UserSessions() echo.MiddlewareFunc {

	return func(next echo.HandlerFunc) echo.HandlerFunc {
		return func(c echo.Context) error {
			sess, err := Store.Get(c.Request(), "database")
			if err != nil {
				return echo.NewHTTPError(400, err.Error())
			}
			c.Set("database", sess)
			return next(c)
		}
	}

}
