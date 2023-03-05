package session

import (
	"github.com/labstack/echo"
)

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
