package models

// MetaDataContainer is a struct that contains the metadata for a graph. It contains a list of nodes and a list of edges.
type MetaDataContainer struct {
	Nodes []MetaData
	Edges []MetaData
}

// MetaData is a struct that contains the metadata for a node or an edge.
type MetaData struct {
	Label string
	Cnt   int
	Kind  string
}
