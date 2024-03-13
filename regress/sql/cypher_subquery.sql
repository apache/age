LOAD 'age';
SET search_path TO ag_catalog;

SELECT * FROM create_graph('subquery');

SELECT * FROM cypher('subquery', $$
								   CREATE (:person {name: "Briggite", age: 32})-[:knows]->(:person {name: "Takeshi", age: 28}),
										  (:person {name: "Faye", age: 25})-[:knows]->(:person {name: "Tony", age: 34})-[:loved]->(:person {name : "Valerie", age: 33}),
										  (:person {name: "Calvin", age: 6})-[:knows]->(:pet {name: "Hobbes"}),
										  (:person {name: "Charlie", age: 8})-[:knows]->(:pet {name : "Snoopy"})
								 $$) AS (result agtype);

SELECT * FROM cypher('subquery', $$ MATCH (a) RETURN (a) $$) AS (result agtype);

SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {(a:person)-[]->(:pet)}
									RETURN (a) $$) AS (result agtype);
--trying to use b when not defined, should fail
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
										   WHERE EXISTS {(a:person)-[]->(b:pet)}
										   RETURN (a) $$) AS (result agtype);
--query inside
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {MATCH (a:person)-[]->(b:pet) RETURN b}
									RETURN (a) $$) AS (result agtype);

--repeat variable in match
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (a:person)
												  WHERE a.name = 'Takeshi'
												  RETURN a
												 }
									RETURN (a) $$) AS (result agtype);
--query inside, with WHERE
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {MATCH (a:person)-[]->(b:pet)
												  WHERE b.name = 'Briggite'
												  RETURN b}
									RETURN (a) $$) AS (result agtype);


--no return
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {MATCH (a:person)-[]->(b:pet)
												  WHERE a.name = 'Calvin'}
									RETURN (a) $$) AS (result agtype);

--union
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (a:person)-[]->(b:pet)
												  WHERE b.name = 'Hobbes'
												  RETURN b
												  UNION
												  MATCH (c:person)-[]->(d:person)
												  RETURN c
												 }
									RETURN (a) $$) AS (result agtype);

-- union, mismatched var, should fail
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (a:person)-[]->(b:pet)
												  WHERE b.name = 'Snoopy'
												  RETURN c
												  UNION
												  MATCH (c:person)-[]->(d:person)
												  RETURN c
												 }
									RETURN (a) $$) AS (result agtype);

--union, no returns
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (a:person)-[]->(b:pet)
												  WHERE a.name = 'Charlie'
												  UNION
												  MATCH (c:person)-[]->(d:person)
												 }
									RETURN (a) $$) AS (result agtype);

--union, mismatched returns, should fail
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (a:person)-[]->(b:pet)
												  WHERE a.name = 'Faye'
												  RETURN a
												  UNION
												  MATCH (c:person)-[]->(d:person)
												 }
									RETURN (a) $$) AS (result agtype);

--nesting
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (b:person)
												  WHERE EXISTS {
																MATCH (c:person)
																WHERE c.name = 'Takeshi'
																RETURN c
															   }
												 }
									RETURN (a) $$) AS (result agtype);

--nesting, accessing var in outer scope
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (b:person)
												  WHERE EXISTS {
																MATCH (c:person)
																WHERE b = c
																RETURN c
															   }
												 }
									RETURN (a) $$) AS (result agtype);

--nesting, accessing indirection in outer scope
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
										   WHERE EXISTS {
														 MATCH (b:person)
														 WHERE EXISTS {
																	   MATCH (c:person)
																	   WHERE b.name = 'Takeshi'
																	   RETURN c
																	  }
														}
										   RETURN (a) $$) AS (result agtype);

--nesting, accessing var 2+ levels up
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
										   WHERE EXISTS {
														 MATCH (b:person)
														 WHERE EXISTS {
																	   MATCH (c:person)
																	   WHERE a.name = 'Takeshi'
																	   RETURN c
																	  }
														}
										   RETURN (a) $$) AS (result agtype);

--nesting, accessing indirection 2+ levels up
--EXISTS subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE EXISTS {
												  MATCH (b:person)
												  WHERE EXISTS {
																MATCH (c:person)
																WHERE a = b
																RETURN c
															  }
												}
									RETURN (a) $$) AS (result agtype);

--EXISTS outside of WHERE
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN a, EXISTS {(a:person)-[]->(:pet)}
									$$) AS (a agtype, exists agtype);

--Var doesnt exist in outside scope, should fail
SELECT * FROM cypher('subquery', $$ RETURN 1,
										   EXISTS {
												   MATCH (b:person)-[]->(:pet)
												   RETURN a
												  }
									$$) AS (a agtype, exists agtype);

--- COUNT

--count pattern subquery in where
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE COUNT {(a:person)} > 1
									RETURN (a) $$) AS (result agtype);

--count pattern in return
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN COUNT{(a)-[]->(:pet)} $$) AS (result agtype);

--count pattern with WHERE
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN COUNT{(a)-[]->(b:pet) WHERE b.name = 'Hobbes'} $$) AS (result agtype);

--solo match in where
--COUNT subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE COUNT{MATCH (a:person)-[]-(:pet)} > 1 RETURN a $$) AS (result agtype);

--solo match in return
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN COUNT{MATCH (a)} $$) AS (result agtype);

--match return in where
--COUNT subquery is currently implemented with postgres subqueries, which do not
--constrain the outer query results. the results of this regression test may change
--upon implementation
--TODO: implement inner subquery constraints
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									WHERE COUNT{MATCH (a) RETURN a} > 1 RETURN a $$) AS (result agtype);

--match return in return
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN COUNT{MATCH (a) RETURN a} $$) AS (result agtype);

--match where return
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN COUNT{MATCH (a)
												 WHERE a.age > 25
												 RETURN a} $$) AS (result agtype);

--counting number of relationships per node
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN a.name, COUNT{(a)-[]-()} $$) AS (name agtype, count agtype);

--nested counts
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN a.name, COUNT{MATCH (a) WHERE COUNT {MATCH (a) WHERE a.age < 23 RETURN a } > 0 } $$) AS (name agtype, count agtype);

--incorrect variable reference
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN a.name, COUNT{MATCH (a) RETURN b} $$) AS (name agtype, count agtype);

--incorrect nested variable reference
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN a.name, COUNT{MATCH (a) WHERE COUNT {MATCH (b) RETURN b } > 1 RETURN b} $$) AS (name agtype count agtype);


--count nested with exists
SELECT * FROM cypher('subquery', $$ MATCH (a:person)
									RETURN a.name, COUNT{MATCH (a) WHERE EXISTS {MATCH (a) RETURN a }} $$) AS (name agtype, count agtype);

--
-- Cleanup
--
SELECT * FROM drop_graph('subquery', true);

--
-- End of tests
--
