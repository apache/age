package main

import (
	"age-viewer-go/models"
	"age-viewer-go/routes"
	"age-viewer-go/session"
	"encoding/gob"

	"github.com/labstack/echo"
)

func main() {
	app := echo.New()
	gob.Register(models.Connection{})
	app.Use(session.UserSessions())

	app.POST("/connect", routes.ConnectToDb)
	app.POST("/query", routes.GraphQuery)
	app.Start(":8080")
}
