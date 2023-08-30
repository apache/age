// Code generated from /home/harry/projects/age/drivers/golang/parser/Age.g4 by ANTLR 4.13.0. DO NOT EDIT.

package parser // Age

import "github.com/antlr4-go/antlr/v4"

type BaseAgeVisitor struct {
	*antlr.BaseParseTreeVisitor
}

func (v *BaseAgeVisitor) VisitAgeout(ctx *AgeoutContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitVertex(ctx *VertexContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitEdge(ctx *EdgeContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitPath(ctx *PathContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitValue(ctx *ValueContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitProperties(ctx *PropertiesContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitPair(ctx *PairContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *BaseAgeVisitor) VisitArr(ctx *ArrContext) interface{} {
	return v.VisitChildren(ctx)
}
