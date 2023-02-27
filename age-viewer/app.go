package main

import (
	"context"
	"fmt"
	_ "github.com/lib/pq"
	"github.com/gin-gonic/gin"
	// "github.com/gorilla/sessions"
	// "net/http"
	// "os"
	"github.com/gin-contrib/cors"
	// "github.com/apache/age/drivers/golang/age"
)

// App struct
type App struct {
	ctx context.Context
}

// NewApp creates a new App application struct
func NewApp() *App {
	return &App{}
}

// startup is called when the app starts. The context is saved
// so we can call the runtime methods
func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
}

// Greet returns a greeting for the given name
func (a *App) Greet(name string) string {
	return fmt.Sprintf("Hello %s, It's show time!", name)
}

func (a *App) router(ctx context.Context) {
	a.ctx = ctx
	router := gin.Default()
	router.Use(cors.Default())

	router.POST("/api/v1/db/connect", func(ctx *gin.Context) {
		a.connect(ctx)
	})

	router.GET("/api/v1/db/disconnect", func(ctx *gin.Context) {
		a.disconnect(ctx)
	})

	router.GET("/api/v1/db", func(ctx *gin.Context) {
		a.status(ctx)
	})

	router.POST("/api/v1/db/meta", func(ctx *gin.Context) {
		a.getMetadata(ctx)
	})

	router.Run(":3001")
}
