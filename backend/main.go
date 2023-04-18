package main

import (
	m "age-viewer-go/miscellaneous"
	"age-viewer-go/models"
	"age-viewer-go/routes"
	"age-viewer-go/session"
	"encoding/gob"

	"github.com/labstack/echo"
)

// main is the entry point for the Backend, it starts the server, and sets up the routes.
func main() {
	app := echo.New()
	gob.Register(models.Connection{})
	app.Use(session.UserSessions())

	app.POST("/connect", routes.ConnectToDb)

	cypher := app.Group("/query", routes.CypherMiddleWare) // "/query" is group of routes under which "/metadata" route is defined
	cypher.Use(m.ValidateContentTypeMiddleWare)            // there we use metadata ends point using localhost:8080/query/metadata
	cypher.POST("/metadata", routes.GraphMetaData)
	cypher.POST("", routes.Cypher)
	app.Start(":8080")
}
