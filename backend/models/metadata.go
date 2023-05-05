package models

// struct that holds the graph metadata and user role information.
type MetaDataContainer struct {
	Graphs map[string]GraphData
}

// struct that contains separate slices for storing nodes and edges
type GraphData struct {
	Nodes []MetaData
	Edges []MetaData
}

// struct that represents a single row of metadata from the database.
// It contains information about the label, count, kind, namespace, namespace_id,
// graph, and relation.
type MetaData struct {
	Label        string
	Cnt          int
	Kind         string
	NameSpace    string
	Namespace_id int
	Graph        string
	Relation     string
}
