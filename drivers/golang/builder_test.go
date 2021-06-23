package main

import (
	"fmt"
	"math/big"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestPathParsing(t *testing.T) {
	rstStr1 := `[{"id": 2251799813685425, "label": "Person", "properties": {"name": "Smith"}}::vertex, 
	{"id": 2533274790396576, "label": "workWith", "end_id": 2251799813685425, "start_id": 2251799813685424, "properties": {"weight": 3}}::edge, 
	{"id": 2251799813685424, "label": "Person", "properties": {"name": "Joe"}}::vertex]::path`

	rstStr2 := `[{"id": 2251799813685424, "label": "Person", "properties": {"name": "Joe"}}::vertex, 
	{"id": 2533274790396576, "label": "workWith", "end_id": 2251799813685425, "start_id": 2251799813685424, "properties": {"weight": 3}}::edge, 
	{"id": 2251799813685425, "label": "Person", "properties": {"name": "Smith"}}::vertex]::path`

	rstStr3 := `[{"id": 2251799813685424, "label": "Person", "properties": {"name": "Joe"}}::vertex, 
	{"id": 2533274790396579, "label": "workWith", "end_id": 2251799813685426, "start_id": 2251799813685424, "properties": {"weight": 5}}::edge, 
	{"id": 2251799813685426, "label": "Person", "properties": {"name": "Jack"}}::vertex]::path`

	unmarshaler := NewAGUnmarshaler()
	entity1, _ := unmarshaler.unmarshal(rstStr1)
	entity2, _ := unmarshaler.unmarshal(rstStr2)
	entity3, _ := unmarshaler.unmarshal(rstStr3)

	assert.Equal(t, entity1.GType(), entity2.GType(), "Type Check")
	p1 := entity1.(*Path)
	p2 := entity2.(*Path)
	p3 := entity3.(*Path)

	assert.Equal(t, p1.GetAsVertex(0).props["name"], p2.GetAsVertex(2).props["name"])
	assert.Equal(t, p2.GetAsVertex(0).props["name"], p3.GetAsVertex(0).props["name"])

	fmt.Println(entity1)
	fmt.Println(entity2)
	fmt.Println(entity3)
}

func TestVertexParsing(t *testing.T) {
	rstStr := `{"id": 2251799813685425, "label": "Person", 
		"properties": {"name": "Smith", "numInt":123, "numIntBig":12345678901235555555555555555, "numFloat": 384.23424, 
		"yn":true, "nullVal": null}}::vertex`

	unmarshaler := NewAGUnmarshaler()
	entity, _ := unmarshaler.unmarshal(rstStr)

	// fmt.Println(entity)
	assert.Equal(t, G_VERTEX, entity.GType())

	v := entity.(*Vertex)
	assert.Equal(t, "Smith", v.props["name"])
	assert.True(t, (123 == v.props["numInt"]))
	assert.Equal(t, 123, v.props["numInt"])

	bi := new(big.Int)
	bi, ok := bi.SetString("12345678901235555555555555555", 10)
	if !ok {
		fmt.Println("Cannot reach this line. ")
	}
	assert.Equal(t, bi, v.props["numIntBig"])

	assert.True(t, (384.23424 == v.props["numFloat"]))
	assert.Equal(t, float64(384.23424), v.props["numFloat"])
	assert.Equal(t, true, v.props["yn"])
	assert.Nil(t, v.props["nullVal"])
}

func TestNormalValueParsing(t *testing.T) {
	mapStr := `{"name": "Smith", "num":123, "yn":true}`
	arrStr := `["name", "Smith", "num", 123, "yn", true]`
	// strStr1 := `abcd`
	strStr2 := `"abcd"`
	intStr := `1234`
	floatStr := `1234.56789`
	boolStr := `true`

	unmarshaler := NewAGUnmarshaler()
	mapv, _ := unmarshaler.unmarshal(mapStr)
	arrv, _ := unmarshaler.unmarshal(arrStr)
	// str1 := unmarshaler.unmarshal(strStr1)
	str2, _ := unmarshaler.unmarshal(strStr2)
	intv, _ := unmarshaler.unmarshal(intStr)
	fl, _ := unmarshaler.unmarshal(floatStr)
	b, _ := unmarshaler.unmarshal(boolStr)

	assert.Equal(t, G_MAP, mapv.GType())
	assert.Equal(t, G_ARR, arrv.GType())
	// assert.Equal(t, G_STR, str1.GType())
	assert.Equal(t, G_STR, str2.GType())
	assert.Equal(t, G_INT, intv.GType())
	assert.Equal(t, G_FLOAT, fl.GType())
	assert.Equal(t, G_BOOL, b.GType())

}
