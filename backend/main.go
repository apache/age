package main

import (
	"age-viewer-go/models"
	"age-viewer-go/routes"
	"age-viewer-go/session"
	"encoding/gob"
	"fmt"
	"net/http"
	"os"

	"github.com/labstack/echo"
	"github.com/labstack/echo/middleware"
	log "github.com/sirupsen/logrus"
)

func main() {
	app := echo.New()

	app.Use(middleware.CORSWithConfig(middleware.CORSConfig{
		AllowOrigins: []string{"*"},
		AllowHeaders: []string{echo.HeaderOrigin, echo.HeaderContentType, echo.HeaderAccept},
	}))
	gob.Register(models.Connection{})
	app.Use(session.UserSessions())

	app.POST("/connect", routes.ConnectToDb)
	app.GET("/status", routes.StatusDB)
	app.POST("/disconnect", routes.DisconnectFromDb)
	cypher := app.Group("/query", routes.CypherMiddleWare)
	cypher.Use(validateContentTypeMiddleware)
	cypher.POST("/metadata", routes.GraphMetaData)
	cypher.POST("", routes.Cypher)
	app.Use(loggingMiddleware)

	app.Start(":8080")
}

func validateContentTypeMiddleware(next echo.HandlerFunc) echo.HandlerFunc {
	return func(c echo.Context) error {
		contentType := c.Request().Header.Get(echo.HeaderContentType)
		if contentType != "application/json" {
			return echo.NewHTTPError(http.StatusBadRequest, "Invalid content type")
		}
		return next(c)
	}
}

// loggingMiddleware is a custom middleware function for logging requests.
func loggingMiddleware(next echo.HandlerFunc) echo.HandlerFunc {
	return func(c echo.Context) error {
		log.WithFields(log.Fields{
			"method": c.Request().Method,
			"path":   c.Request().URL.Path,
		}).Print("Request")

		// Call the next handler
		err := next(c)

		statusText := getStatusText(err)
		logMethod := log.Print
		if err != nil {
			logMethod = log.Error
		}

		logMethod(fmt.Sprintf("Response: %s", statusText))

		return err
	}
}
func getStatusText(err error) string {
	if err != nil {
		return fmt.Sprintf("Error: %v", err)
	}
	return "Success"
}

func init() {
	log.SetFormatter(&log.TextFormatter{
		ForceColors: true,
	})

	log.SetOutput(os.Stdout)
}
