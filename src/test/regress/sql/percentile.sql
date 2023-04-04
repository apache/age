--
-- Testing Percentile functions
--

-- Creating sample graph
CREATE GRAPH percentile_graph;
SET graph_path = percentile_graph;
CREATE
  (keanu:Person {name: 'Keanu Reeves', age: 58}),
  (liam:Person {name: 'Liam Neeson', age: 70}),
  (carrie:Person {name: 'Carrie Anne Moss', age: 55}),
  (guy:Person {name: 'Guy Pearce', age: 55}),
  (kathryn:Person {name: 'Kathryn Bigelow', age: 71}),
  (speed:Movie {title: 'Speed'}),
  (keanu)-[:ACTED_IN]->(speed),
  (keanu)-[:KNOWS]->(carrie),
  (keanu)-[:KNOWS]->(liam),
  (keanu)-[:KNOWS]->(kathryn),
  (carrie)-[:KNOWS]->(guy),
  (liam)-[:KNOWS]->(guy);

-- Testing the percentilecont() function
MATCH (p:Person) WITH collect(p.age) AS ages RETURN percentilecont(ages, 0.4);
MATCH (p:Person) WITH collect(p.age) AS ages RETURN percentilecont(null, 0.4);


-- Testing the percentiledisc() function
MATCH (p:Person) WITH collect(p.age) AS ages RETURN percentiledisc(ages, 0.5);
MATCH (p:Person) WITH collect(p.age) AS ages RETURN percentiledisc(null, 0.5);
