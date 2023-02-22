package session

import (
	m "age-viewer-go/miscellaneous"

	"github.com/labstack/echo"
)

func UserSessions() echo.MiddlewareFunc {

	return func(next echo.HandlerFunc) echo.HandlerFunc {
		return func(c echo.Context) error {
			sess, err := Store.Get(c.Request(), "database")
			if err != nil {
				return m.HandleError(err, "could not hanlde session", c)
			}

			c.Set("database", sess)
			return next(c)
		}
	}

}
