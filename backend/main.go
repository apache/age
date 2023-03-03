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
	cypher := app.Group("/query", routes.CypherMiddleWare)
	cypher.POST("/metadata", routes.GraphMetaData)
	cypher.POST("", routes.Cypher)
	app.Start(":8080")
}
