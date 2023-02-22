package miscellaneous

import (
	"github.com/labstack/echo"
)

func HandleError(e error, msg string, r echo.Context, code ...int) error {
	var rcode int
	errorMSG := map[string]string{
		"error":  e.Error(),
		"status": msg,
	}

	if len(code) == 0 {
		rcode = 400
	} else {
		rcode = code[0]
	}
	return r.JSON(rcode, errorMSG)
}
