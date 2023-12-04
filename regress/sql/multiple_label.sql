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

LOAD 'age';
SET search_path TO ag_catalog;


/*
 * MATCH queries with multiple labels
 */
SElECT create_graph('mlabels1');
-- create
SELECT * FROM cypher('mlabels1', $$ CREATE (x:a     {name:'a'})   $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE (x:b     {name:'b'})   $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE (x:c     {name:'c'})   $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE (x:a:b   {name:'ab'})  $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE (x:a:b:c {name:'abc'}) $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE ()-[:p {name:'p'}]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE ()-[:q {name:'q'}]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels1', $$ CREATE ()-[:r {name:'r'}]->() $$) as (a agtype);
-- match OR
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a)            RETURN x.name $$) as (":a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a|b)          RETURN x.name $$) as (":a|b" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a|b|c)        RETURN x.name $$) as (":a|b|c" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:p]->()     RETURN e.name $$) as (":p" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:p|q]->()   RETURN e.name $$) as (":p|q" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:p|q|r]->() RETURN e.name $$) as (":p|q|r" agtype);
-- match AND
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a)            RETURN x.name $$) as (":a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a:b)          RETURN x.name $$) as (":a:b" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a:b:c)        RETURN x.name $$) as (":a:b:c" agtype);
-- mutual inclusion\exclusion
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a)            RETURN x.name $$) as (":a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:b)            RETURN x.name $$) as (":b" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a:b)          RETURN x.name $$) as (":a:b" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a|b)          RETURN x.name $$) as (":a|b" agtype);
-- duplicate: (a, b, a) = (a, b)
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a:b:a)        RETURN x.name $$) as (":a:b:a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a|b|a)        RETURN x.name $$) as (":a|b|a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:p|q|p]->() RETURN e.name $$) as (":p|q|p" agtype);
-- order: (a, b) = (b, a)
SELECT * FROM cypher('mlabels1', $$ MATCH (x:b:a)          RETURN x.name $$) as (":b:a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:b|a)          RETURN x.name $$) as (":b|a" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:q|p]->()   RETURN e.name $$) as (":q|p" agtype);
-- some label does not exist: m and n
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a:m:b)        RETURN x.name $$) as (":a:m:b" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:a|m|b)        RETURN x.name $$) as (":a|m|b" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:p|n|q]->() RETURN e.name $$) as (":p|n|q" agtype);
-- no label exists
SELECT * FROM cypher('mlabels1', $$ MATCH (x:i:j:k)        RETURN x.name $$) as (":i:j:k" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH (x:i|j|k)        RETURN x.name $$) as (":i|j|j" agtype);
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:l|m|n]->() RETURN e.name $$) as (":l|m|n" agtype);
-- unsupported label expression in match
SELECT * FROM cypher('mlabels1', $$ MATCH ()-[e:l:m:n]->() RETURN e.name $$) as (":l|m|n" agtype);
-- cleanup
SElECT drop_graph('mlabels1', true);



/*
 * Unsupported label expressions for create\merge
 */
SElECT create_graph('mlabels4');
-- for vertices
SELECT * FROM cypher('mlabels4', $$ CREATE (:a|b) $$) as (a agtype);
SELECT * FROM cypher('mlabels4', $$ MERGE  (:a|b) $$) as (a agtype);
-- for edges
SELECT * FROM cypher('mlabels4', $$ CREATE ()-[]->()     $$) as (a agtype);
SELECT * FROM cypher('mlabels4', $$ CREATE ()-[:a|b]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels4', $$ CREATE ()-[:a:b]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels4', $$ MERGE  ()-[]->()     $$) as (a agtype);
SELECT * FROM cypher('mlabels4', $$ MERGE  ()-[:a|b]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels4', $$ MERGE  ()-[:a:b]->() $$) as (a agtype);
-- cleanup
SElECT drop_graph('mlabels4', true);



/*
 * Modelling inheritance
 */
SElECT create_graph('mlabels5');
SELECT * FROM cypher('mlabels5', $$ CREATE (:employee:engineer                  {title:'engineer'})   $$) as (a agtype);
SELECT * FROM cypher('mlabels5', $$ CREATE (:employee:manager                   {title:'manager'})    $$) as (a agtype);
SELECT * FROM cypher('mlabels5', $$ CREATE (:employee:manager:engineer:techlead {title:'techlead'})   $$) as (a agtype);
SELECT * FROM cypher('mlabels5', $$ CREATE (:employee:accountant                {title:'accountant'}) $$) as (a agtype);
-- match titles
SELECT * FROM cypher('mlabels5', $$ MATCH (x:employee)   RETURN x.title $$) as ("all"         agtype);
SELECT * FROM cypher('mlabels5', $$ MATCH (x:engineer)   RETURN x.title $$) as ("engineers"   agtype);
SELECT * FROM cypher('mlabels5', $$ MATCH (x:manager)    RETURN x.title $$) as ("managers"    agtype);
SELECT * FROM cypher('mlabels5', $$ MATCH (x:techlead)   RETURN x.title $$) as ("techleads"   agtype);
SELECT * FROM cypher('mlabels5', $$ MATCH (x:accountant) RETURN x.title $$) as ("accountants" agtype);
-- cleanup
SElECT drop_graph('mlabels5', true);



/*
 * Invalid label
 */
SElECT create_graph('mlabels6');
SELECT * FROM cypher('mlabels6', $$ CREATE (:a)-[:x]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels6', $$ CREATE (:b)-[:y]->() $$) as (a agtype);
SELECT * FROM cypher('mlabels6', $$ CREATE (:c)-[:z]->() $$) as (a agtype);
-- following fails
SELECT * FROM cypher('mlabels6', $$ CREATE (:a:y:c) $$) as (a agtype);
SELECT * FROM cypher('mlabels6', $$ CREATE ()-[:b]->() $$) as (a agtype);
-- cleanup
SElECT drop_graph('mlabels6', true);



/*
 * Mixing different label expression types
 */
SElECT create_graph('mlabels7');
SELECT * FROM cypher('mlabels7', $$ CREATE (:a) $$) as (a agtype);
SELECT * FROM cypher('mlabels7', $$ CREATE (:b) $$) as (a agtype);
SELECT * FROM cypher('mlabels7', $$ CREATE (:c) $$) as (a agtype);
-- following fails
SELECT * FROM cypher('mlabels7', $$ MATCH (x:a|b:c) RETURN x $$) as (a agtype);
-- cleanup
SElECT drop_graph('mlabels7', true);



/*
 * Catalog ag_label.allrelations
 */
SElECT create_graph('mlabels8');
CREATE VIEW mlabels8.catalog AS
    SELECT name, relation, allrelations, rel_kind
    FROM ag_catalog.ag_label
    WHERE graph IN
        (SELECT graphid
         FROM ag_catalog.ag_graph
         WHERE name = 'mlabels8')
    ORDER BY name ASC;
-- creates :a, :b and :a:b
SELECT * FROM cypher('mlabels8', $$ CREATE (:a:b)  $$) as (":a:b" agtype);
SELECT * FROM mlabels8.catalog;
-- creates :c and :bc
SELECT * FROM cypher('mlabels8', $$ CREATE (:b:c)  $$) as (":b:c" agtype);
SELECT * FROM mlabels8.catalog;
-- :a:b:c inserted in other labels' allrelations column
SELECT * FROM cypher('mlabels8', $$ CREATE (:a:b:c) $$) as (":a:b:c" agtype);
SELECT * FROM mlabels8.catalog;
-- cleanup
SElECT drop_graph('mlabels8', true);



/*
 * Alias of union subquery in join and filter nodes
 */
SElECT create_graph('mlabels9');
SELECT * FROM cypher('mlabels9', $$ CREATE (:a {age:22, title:'a'}) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ CREATE (:b {age:25, title:'b'}) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ CREATE (:c {age:27, title:'c'}) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ CREATE (:d {age:32, title:'d'}) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (x:a) CREATE (x)-[:rel {start:'a'}]->(:m) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (x:b) CREATE (x)-[:rel {start:'b'}]->(:m) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (x:c) CREATE (x)-[:rel {start:'c'}]->(:m) $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (x:d) CREATE (x)-[:rel {start:'d'}]->(:m) $$) as (a agtype);
-- join
SELECT * FROM cypher('mlabels9', $$ MATCH (x:a|b|c)-[e:rel]->(:m)            RETURN e.start $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (:a|b|c)-[e:rel]->(:m)             RETURN e.start $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (x:a|b|c) WITH x MATCH (x)-[e]->() RETURN e.start $$) as (a agtype);
-- filters
SELECT * FROM cypher('mlabels9', $$ MATCH (x:a|b|c) WHERE x.age > 24 RETURN x.title $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ MATCH (x:a|b|c {age:27})         RETURN x.title $$) as (a agtype);
-- plans: to check alias
SELECT * FROM cypher('mlabels9', $$ EXPLAIN (COSTS off) MATCH (x:a)-[e:rel]->(:m)                RETURN e.start $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ EXPLAIN (COSTS off) MATCH (x:a|b|c)-[e:rel]->(:m)            RETURN e.start $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ EXPLAIN (COSTS off) MATCH (:a|b|c)-[e:rel]->(:m)             RETURN e.start $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ EXPLAIN (COSTS off) MATCH (x:a|b|c) WHERE x.age > 24         RETURN x.title $$) as (a agtype);
SELECT * FROM cypher('mlabels9', $$ EXPLAIN (COSTS off) MATCH (x:a|b|c {age:27})                 RETURN x.title $$) as (a agtype);
-- cleanup
SElECT drop_graph('mlabels9', true);



/*
 * Match without variables
 */
SElECT create_graph('mlabels10');
SELECT * FROM cypher('mlabels10', $$ CREATE (:a:b)-[:rel{title:'ab->pq'}]->(:p:q) $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ CREATE (:c)-[:rel{title:'c->m'}]->(:m)       $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ CREATE (:d)-[:rel{title:'d->n'}]->(:n)       $$) as (a agtype);
-- AND
SELECT * FROM cypher('mlabels10', $$ MATCH (:a:b)-[e:rel]->()     RETURN e.title $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ MATCH ()-[e:rel]->(:p:q)     RETURN e.title $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ MATCH (:a:b)-[e:rel]->(:p:q) RETURN e.title $$) as (a agtype);
-- OR
SELECT * FROM cypher('mlabels10', $$ MATCH (:c|d)-[e:rel]->()     RETURN e.title $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ MATCH ()-[e:rel]->(:m|n)     RETURN e.title $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ MATCH (:c|d)-[e:rel]->(:m|n) RETURN e.title $$) as (a agtype);
-- Single
SELECT * FROM cypher('mlabels10', $$ MATCH (:c)-[e:rel]->()   RETURN e.title $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ MATCH ()-[e:rel]->(:m)   RETURN e.title $$) as (a agtype);
SELECT * FROM cypher('mlabels10', $$ MATCH (:c)-[e:rel]->(:m) RETURN e.title $$) as (a agtype);
-- cleanup
SElECT drop_graph('mlabels10', true);



/*
 * Prevent label names with reserved prefix
 */
SElECT create_graph('mlabels11');
SELECT * FROM cypher('mlabels11', $$ CREATE (:a:b {title:'ab'}) $$) as (a agtype);
SELECT * FROM cypher('mlabels11', $$ CREATE (:_agr_ab {title:'_agr_ab'})      $$) as (a agtype);
SELECT create_vlabel('mlabels11', '_agr_xy');
SELECT create_elabel('mlabels11', '_agr_pq');
-- check
SELECT * FROM cypher('mlabels11', $$ MATCH (x:a:b) RETURN x.title      $$) as (a agtype);
-- cleanup
SElECT drop_graph('mlabels11', true);



/*
 * Internal functions _label_names() and _agtype_build_vertex
 */
SElECT create_graph('mlabels12');
-- check create
SELECT * FROM cypher('mlabels12', $$ CREATE (x:Person:Student {title: 'Person and Student'}) RETURN x $$) as (a agtype);
SELECT * FROM cypher('mlabels12', $$ CREATE (x:Person         {title: 'Person only'}) RETURN x $$) as (a agtype);
SELECT * FROM cypher('mlabels12', $$ CREATE (x {title: 'No label'})  RETURN x $$) as (a agtype);
-- check _label_names
SELECT
    v.properties -> '"title"' AS "title",
    _label_names(g.id, v.id)
FROM
    mlabels12._ag_label_vertex AS v,
    (SELECT graphid AS id
     FROM ag_catalog.ag_graph
     WHERE name = 'mlabels12'
     LIMIT 1) AS g;
-- check match
SELECT * FROM cypher('mlabels12', $$ MATCH (x) RETURN x $$) as ("MATCH" agtype);
-- check set
SELECT * FROM cypher('mlabels12', $$ MATCH (x) SET x.age = 32 RETURN x $$) as ("SET" agtype);
-- check merge
SELECT * FROM cypher('mlabels12', $$ MERGE (x:Teacher:Person) RETURN x $$) as ("MERGE" agtype);
-- check vle
SELECT * FROM cypher('mlabels12', $$ CREATE (:a:b)-[:r]->(:k)-[:r]->(:m:n) $$) as (a agtype);
SELECT * FROM cypher('mlabels12', $$ MATCH p=()-[*2]->() RETURN p $$) as ("VLE" agtype);
-- check vertex typecast
SELECT agtype_typecast_vertex('{"id": 281474976710657, "label": ["a", "b"], "properties": {"age": 32}}'::agtype);
SELECT agtype_typecast_vertex('{"id": 281474976710657, "label": ["a"], "properties": {"age": 32}}'::agtype);
SELECT agtype_typecast_vertex('{"id": 281474976710657, "label": [], "properties": {"age": 32}}'::agtype);
SELECT agtype_typecast_vertex('{"id": 281474976710657, "label": "hello", "properties": {"age": 32}}'::agtype);
SELECT agtype_typecast_vertex('{"id": 281474976710657, "label": "", "properties": {"age": 32}}'::agtype);
-- check agtype_in
SELECT agtype_in('{"id": 281474976710657, "label": ["a", "b"], "properties": {"age": 32}}::vertex');
SELECT agtype_in('{"id": 281474976710657, "label": ["a"], "properties": {"age": 32}}::vertex');
SELECT agtype_in('{"id": 281474976710657, "label": [], "properties": {"age": 32}}::vertex');
SELECT agtype_in('{"id": 281474976710657, "label": "hello", "properties": {"age": 32}}::vertex');
SELECT agtype_in('{"id": 281474976710657, "label": "", "properties": {"age": 32}}::vertex');
-- check age_labels
SELECT * FROM cypher('mlabels12', $$ MATCH (x) RETURN labels(x) $$) as ("labels" agtype);
-- check startnode and endnode
SELECT * FROM cypher('mlabels12', $$ MATCH ()-[e]->() RETURN startNode(e) $$) as ("startNode" agtype);
SELECT * FROM cypher('mlabels12', $$ MATCH ()-[e]->() RETURN endNode(e)   $$) as ("endNode" agtype);
-- cleanup
SElECT drop_graph('mlabels12', true);
