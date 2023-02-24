package main

import (
	"log"
	"database/sql"
	_ "github.com/lib/pq"
	"github.com/gin-gonic/gin"
	"net/http"
	// "os"
	// "github.com/gin-contrib/cors"
	// "github.com/apache/age/drivers/golang/age"
)

type dbInfo struct {
	dbDriver string
	dbSource string
	conn *sql.DB
	err error
}

type GraphRepo struct {
	Host string
	Port string
	User string
	Password string
	Database string
	Graphs []string
}

type connectRequest struct {
	Host string `json:"host" binding: "required"`
	Port string `json:"port" binding: "required"`
	Username string `json:"user" binding: "required"`
	Password string `json:"password" binding: "required"`
	Database string `json: "database" binding: "required"`
}

var graphRepo = &GraphRepo{}
var dbinfo = &dbInfo{}

func (a *App) connect(ctx *gin.Context) {
	var req connectRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
	}
	dbinfo.dbDriver = "postgres"
	dbinfo.dbSource = "postgresql://" + req.Username + ":" + req.Password + "@" + req.Host + ":" + req.Port + "/" + req.Database + "?sslmode=disable"
	dbinfo.conn, dbinfo.err = sql.Open(dbinfo.dbDriver, dbinfo.dbSource)

	if dbinfo.err != nil {
		log.Println("Cannot connect to DB")
		ctx.JSON(http.StatusBadRequest, gin.H{"error": dbinfo.err.Error()})
	} else {
		dbinfo.err = dbinfo.conn.Ping()
		if dbinfo.err != nil {
			log.Println("Cannot connect to DB")
			ctx.JSON(http.StatusBadRequest, gin.H{"error": dbinfo.err.Error()})
		}  else {
			graphRepo = &GraphRepo{Host: "localhost", Port: req.Port, User: req.Username, Password: req.Password, Database: req.Database}
		}
	}

	connag_row, connag_err := dbinfo.conn.Query(`CREATE EXTENSION IF NOT EXISTS age; LOAD 'age'; SET search_path = ag_catalog, "$user", public;`)
	if connag_err != nil {
		ctx.JSON(http.StatusBadRequest, gin.H{"error": connag_err.Error()})
	}
	graph_row, graph_err := dbinfo.conn.Query(`SELECT * FROM ag_catalog.ag_graph;`)
	defer connag_row.Close()
	defer graph_row.Close()

	for graph_row.Next() {
		var (
			name string
			namespace string
		)
		if graph_err = graph_row.Scan(&name, &namespace); graph_err != nil {
			log.Println(graph_err)
		}
		graphRepo.Graphs = append(graphRepo.Graphs, name)
	}

	ctx.JSON(http.StatusOK, graphRepo)
} 