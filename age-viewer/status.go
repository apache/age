package main

import (
	"github.com/gin-gonic/gin"
	"net/http"
)

func (a *App) status(ctx *gin.Context) {
	host := graphRepo.Host
	user := graphRepo.User
	if len(host) == 0 || len(user) == 0 {
		ctx.JSON(http.StatusBadRequest, "Not Connected")
		return
	}
	ctx.JSON(http.StatusOK, graphRepo)
} 