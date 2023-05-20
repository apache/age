package session

import (
	"net/http"

	"github.com/labstack/echo"
)

const (
	DatabaseName = "database"
)

func UserSessions() echo.MiddlewareFunc {
	return func(next echo.HandlerFunc) echo.HandlerFunc {
		return func(c echo.Context) error {
			sess, err := Store.Get(c.Request(), DatabaseName)
			if err != nil {
				return echo.NewHTTPError(http.StatusBadRequest, err.Error())
			}
			c.Set(DatabaseName, sess)
			return next(c)
		}
	}
}
