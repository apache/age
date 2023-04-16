package miscellaneous

import (
	"errors"
	"fmt"
	"net/http"

	"github.com/labstack/echo"
)

/*
We are using application/json for all http request,
The function ValidateContentTypeMiddleWare is a middleware that checks the content type of the request.
If the Content-Type header is not present or does not match the application/json type, an HTTP error is returned.
*/
func ValidateContentTypeMiddleWare(next echo.HandlerFunc) echo.HandlerFunc {
	return func(ctx echo.Context) error {
		val, ok := ctx.Request().Header["Content-Type"]
		msg := fmt.Sprintf("expected %s, received %s", "application/json", val)
		if ok {
			if fmt.Sprintf("%s", val) != "[application/json]" {
				return echo.NewHTTPError(http.StatusUnsupportedMediaType, errors.New(msg))
			}
		} else {
			return echo.NewHTTPError(http.StatusUnsupportedMediaType, errors.New(msg))
		}
		return next(ctx)
	}
}
