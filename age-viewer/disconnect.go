package main

import (
	"github.com/gin-gonic/gin"
	"net/http"
)

func (a *App) disconnect(ctx *gin.Context) {
	graphRepo = &GraphRepo{}
	dbinfo = &dbInfo{}

	ctx.JSON(http.StatusOK, graphRepo)
} 