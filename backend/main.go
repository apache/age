/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 
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
