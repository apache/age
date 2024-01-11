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
package age

import (
	"bytes"
	"fmt"
	"math/big"
	"reflect"
)

// GTYPE representing entity types for AGE result data : Vertex, Edge, Path and SimpleEntity
type GTYPE uint8

const (
	G_OTHER GTYPE = 1 + iota
	G_VERTEX
	G_EDGE
	G_PATH
	G_MAP_PATH
	G_STR
	G_INT
	G_INTBIG
	G_FLOAT
	G_FLOATBIG
	G_BOOL
	G_NULL
	G_MAP
	G_ARR
)

var (
	_TpV        = reflect.TypeOf(&Vertex{})
	_TpE        = reflect.TypeOf(&Edge{})
	_TpP        = reflect.TypeOf(&Path{})
	_TpMP       = reflect.TypeOf(&MapPath{})
	_TpStr      = reflect.TypeOf(string(""))
	_TpInt      = reflect.TypeOf(int64(0))
	_TpIntBig   = reflect.TypeOf(big.NewInt(0))
	_TpFloat    = reflect.TypeOf(float64(0))
	_TpFloatBig = reflect.TypeOf(big.NewFloat(0))
	_TpBool     = reflect.TypeOf(bool(false))
	_TpMap      = reflect.TypeOf(map[string]interface{}{})
	_TpArr      = reflect.TypeOf([]interface{}{})
)

// Entity object interface for parsed AGE result data : Vertex, Edge, Path and SimpleEntity
type Entity interface {
	GType() GTYPE
	String() string
}

// Check if the given value is Entity object.
// An entity is assigned a set of properties,
// each of which are uniquely identified in the set by the irrespective property keys.
// See https://age.apache.org/age-manual/master/intro/types.html#simple-entities for more details.
func IsEntity(v interface{}) bool {
	_, ok := v.(Entity)
	return ok
}

// SimpleEntity is a simple entity object that has a single value.
// It is used to represent a simple value such as string, integer, float, boolean, null, map and array.
type SimpleEntity struct {
	Entity
	typ   GTYPE
	value interface{}
}

func NewSimpleEntity(value interface{}) *SimpleEntity {
	if value == nil {
		return &SimpleEntity{typ: G_NULL, value: nil}
	}

	switch value.(type) {
	case string:
		return &SimpleEntity{typ: G_STR, value: value}
	case int64:
		return &SimpleEntity{typ: G_INT, value: value}
	case *big.Int:
		return &SimpleEntity{typ: G_INTBIG, value: value}
	case float64:
		return &SimpleEntity{typ: G_FLOAT, value: value}
	case *big.Float:
		return &SimpleEntity{typ: G_FLOATBIG, value: value}
	case bool:
		return &SimpleEntity{typ: G_BOOL, value: value}
	case map[string]interface{}:
		return &SimpleEntity{typ: G_MAP, value: value}
	case []interface{}:
		return &SimpleEntity{typ: G_ARR, value: value}
	default:
		return &SimpleEntity{typ: G_OTHER, value: value}
	}
}

func (e *SimpleEntity) GType() GTYPE {
	return e.typ
}

func (e *SimpleEntity) IsNull() bool {
	return e.value == nil
}

func (e *SimpleEntity) Value() interface{} {
	return e.value
}

func (e *SimpleEntity) String() string {
	return fmt.Sprintf("%v", e.value)
}

func (e *SimpleEntity) AsStr() string {
	return e.value.(string)
}

func (e *SimpleEntity) AsInt() int {
	return e.value.(int)
}

func (e *SimpleEntity) AsInt64() int64 {
	return e.value.(int64)
}

func (e *SimpleEntity) AsBigInt() *big.Int {
	return e.value.(*big.Int)
}

func (e *SimpleEntity) AsFloat() float64 {
	return e.value.(float64)
}

func (e *SimpleEntity) AsBigFloat() *big.Float {
	return e.value.(*big.Float)
}

func (e *SimpleEntity) AsBool() bool {
	return e.value.(bool)
}

func (e *SimpleEntity) AsMap() map[string]interface{} {
	return e.value.(map[string]interface{})
}

func (e *SimpleEntity) AsArr() []interface{} {
	return e.value.([]interface{})
}

type LabeledEntity struct {
	Entity
	id    int64
	label string
	props map[string]interface{}
}

func newLabeledEntity(id int64, label string, props map[string]interface{}) *LabeledEntity {
	return &LabeledEntity{id: id, label: label, props: props}
}

func (n *LabeledEntity) Id() int64 {
	return n.id
}

func (n *LabeledEntity) Label() string {
	return n.label
}

func (n *LabeledEntity) Prop(key string) interface{} {
	return n.props[key]
}

// return properties
func (n *LabeledEntity) Props() map[string]interface{} {
	return n.props
}

type Vertex struct {
	*LabeledEntity
}

// NewVertex creates a new Vertex object with the given id, label and properties.
// See https://age.apache.org/age-manual/master/intro/types.html#vertex for more details.
func NewVertex(id int64, label string, props map[string]interface{}) *Vertex {
	return &Vertex{newLabeledEntity(id, label, props)}
}

func (v *Vertex) GType() GTYPE {
	return G_VERTEX
}

// String returns a formatted string containing the vertex id, label and properties.
func (v *Vertex) String() string {
	return fmt.Sprintf("V{id:%d, label:%s, props:%v}", v.id, v.label, v.props)
}

// An edge is an entity that encodes a directed connection between exactly two nodes,
// the source node and the target node. An outgoing edge is a directed relationship
// from the point of view of its source node. An incoming edge is a directed relationship
// from the point of view of its target node. An edge is assigned exactly one edge type.
type Edge struct {
	*LabeledEntity
	start_id int64
	end_id   int64
}

// NewEdge creates a new Edge object with the given id, label, start vertex id, end vertex id and properties.
// See https://age.apache.org/age-manual/master/intro/types.html#edge for more details.
func NewEdge(id int64, label string, start int64, end int64, props map[string]interface{}) *Edge {
	return &Edge{LabeledEntity: newLabeledEntity(id, label, props), start_id: start, end_id: end}
}

func (e *Edge) GType() GTYPE {
	return G_EDGE
}

func (e *Edge) StartId() int64 {
	return e.start_id
}

func (e *Edge) EndId() int64 {
	return e.end_id
}

// String returns a formatted string containing the edge id, label, start vertex id, end vertex id and properties.
func (e *Edge) String() string {
	return fmt.Sprintf("E{id:%d, label:%s, start:%d, end:%d, props:%v}",
		e.id, e.label, e.start_id, e.end_id, e.props)
}

// A path is a series of alternating vertices and edges.
// A path must start with a vertex, and have at least one edge.
type Path struct {
	Entity
	entities []Entity
}

func NewPath(entities []Entity) *Path {
	return &Path{entities: entities}
}

func (e *Path) GType() GTYPE {
	return G_PATH
}

func (e *Path) Size() int {
	return len(e.entities)
}

// Get returns the entity at the specified position in the specified path.
func (e *Path) Get(index int) Entity {
	if index < 0 && index >= len(e.entities) {
		panic(fmt.Errorf("Entity index[%d] is out of range (%d) ", index, len(e.entities)))
	}
	return e.entities[index]
}

// GetAsVertex casts the entity at the specified position in the specified path to Vertex.
func (e *Path) GetAsVertex(index int) *Vertex {
	v := e.Get(index)

	if v.GType() != G_VERTEX {
		panic(fmt.Errorf("Entity[%d] is not Vertex", index))
	}
	return v.(*Vertex)
}

func (e *Path) GetAsEdge(index int) *Edge {
	v := e.Get(index)
	if v.GType() != G_EDGE {
		panic(fmt.Errorf("Entity[%d] is not Edge", index))
	}
	return v.(*Edge)
}

func (p *Path) String() string {
	var buf bytes.Buffer
	buf.WriteString("P[")
	for _, e := range p.entities {
		buf.WriteString(e.String())
		buf.WriteString(",")
	}
	buf.WriteString("]")
	return buf.String()
}

type MapPath struct {
	Entity
	entities []interface{}
}

func NewMapPath(entities []interface{}) *MapPath {
	return &MapPath{entities: entities}
}

func (e *MapPath) GType() GTYPE {
	return G_MAP_PATH
}

func (e *MapPath) Size() int {
	return len(e.entities)
}

func (e *MapPath) Get(index int) interface{} {
	if index < 0 && index >= len(e.entities) {
		panic(fmt.Errorf("Entity index[%d] is out of range (%d) ", index, len(e.entities)))
	}
	return e.entities[index]
}

func (p *MapPath) String() string {
	var buf bytes.Buffer
	buf.WriteString("P[")
	for _, e := range p.entities {
		buf.WriteString(fmt.Sprintf("%v", e))
		buf.WriteString(",")
	}
	buf.WriteString("]")
	return buf.String()
}
