package main

import (
	m "age-viewer-go/miscellaneous"
	"age-viewer-go/models"
	"age-viewer-go/routes"
	"age-viewer-go/session"
	"encoding/gob"

	"github.com/labstack/echo"
	"github.com/labstack/echo/middleware"
)

// main is the entry point for the Backend, it starts the server, and sets up the routes.
func main() {
	app := echo.New()

	app.Use(middleware.CORSWithConfig(middleware.CORSConfig{
		AllowOrigins: []string{"*"},
		AllowHeaders: []string{echo.HeaderOrigin, echo.HeaderContentType, echo.HeaderAccept},
	  }))
	gob.Register(models.Connection{})
	app.Use(session.UserSessions())

	app.POST("/connect", routes.ConnectToDb)
	app.POST("/disconnect", routes.DisconnectFromDb)
	cypher := app.Group("/query", routes.CypherMiddleWare)
	cypher.Use(m.ValidateContentTypeMiddleWare)
	cypher.POST("/metadata", routes.GraphMetaData)
	cypher.POST("", routes.Cypher)
	app.Start(":8080")
}
