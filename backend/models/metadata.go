package models

type MetaDataContainer struct {
	Nodes []MetaData
	Edges []MetaData
}
type MetaData struct {
	Label string
	Cnt   int
	Kind  string
}
