package main

import (
	"github.com/gin-gonic/gin"
	"net/http"
	"github.com/apache/age/drivers/golang/age"
)

type GraphData struct {
	Nodes []Node
	Edges []Edge
	Graph string
	Database string
}

type Node struct {
	Label string
	Cnt int64
}

type Edge struct {
	Label string
	Cnt int64
}


func (a *App) getMetadata(ctx *gin.Context) {

	metadata := make(map[string]GraphData)
	graph := graphRepo.Graphs[0]

	_, err := age.GetReady(dbinfo.conn, graph)
	if err != nil {
		panic(err)
	}
	tx, err := dbinfo.conn.Begin()
	if err != nil {
		panic(err)
	}

	nodes_row, err := age.ExecCypher(tx, graph, 1, "MATCH (V) RETURN V")
	if err != nil {
		panic(err)
	}
	nodes := []Node{}
	nodeLabelCnt := make(map[string]int64)
	nodeNewVertex := make(map[string]Node)
	for nodes_row.Next() {
		row, err := nodes_row.GetRow()
		if err != nil {
			panic(err)
		}
		vertex := row[0].(*age.Vertex)
		nodeLabelCnt[vertex.Label()] += 1
		nodeNewVertex[vertex.Label()] = Node{Label: vertex.Label(), Cnt: nodeLabelCnt[vertex.Label()] } 
	}

	for key := range nodeNewVertex {
		nodes = append(nodes, nodeNewVertex[key])
	}

	edges_row, err := age.ExecCypher(tx, graph, 3, "MATCH (V)-[R]-(V2) RETURN V, R, V2")
	if err != nil {
		panic(err)
	}

	edges := []Edge{}
	edgeLabelCnt := make(map[string]int64)
	edgeNewVertex := make(map[string]Edge)
	for edges_row.Next() {
		row, err := edges_row.GetRow()
		if err != nil {
			panic(err)
		}
		vertex := row[1].(*age.Edge)
		edgeLabelCnt[vertex.Label()] += 1
		edgeNewVertex[vertex.Label()] = Edge{Label: vertex.Label(), Cnt: edgeLabelCnt[vertex.Label()]}
	}

	for key := range edgeNewVertex {
		edgeCnt := edgeNewVertex[key]
		edgeCnt.Cnt /= 2
		edgeNewVertex[key] = edgeCnt
		edges = append(edges, edgeNewVertex[key])
	}

	tx.Commit()

	node_meta := GraphData {
		Nodes: nodes,
		Edges: edges,
		Graph: graph,
		Database: graphRepo.Database,
	}
	metadata[graph] = node_meta
	ctx.JSON(http.StatusOK, metadata)
} 