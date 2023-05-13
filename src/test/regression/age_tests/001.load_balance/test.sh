#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for load balancing.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

# sleep time after reload in seconds
st=10

for mode in s
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	source ./bashrc.ports

	echo "backend_weight0 = 0" >> etc/pgpool.conf
	echo "backend_weight1 = 1" >> etc/pgpool.conf
	echo "statement_level_load_balance = on" >> etc/pgpool.conf
	

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup



	$PSQL test <<EOF
-- Setup
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';	
SET search_path = ag_catalog, public;
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (dummy) RETURN dummy \$\$) as (v ag_catalog.agtype); -- Forcing secondary node to Self-Load AGE with a dummy MATCH

-- Database population
SELECT create_graph('test_graph');

SELECT * FROM ag_catalog.cypher('test_graph', \$\$ CREATE (:vertex1 {i: 123}) \$\$) as (v ag_catalog.agtype);
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ CREATE (:vertex2 {i: 124}) \$\$) as (v ag_catalog.agtype);


-- Read Query
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (v) RETURN v \$\$) as (v ag_catalog.agtype);

EOF


# check if simple load balance worked
	fgrep "SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (v) RETURN v \$\$) as (v ag_catalog.agtype);" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: select is sent to zero-weight node.
		./shutdownall
		exit 1
	fi
	echo ok: load balance works.



# CREATE cypher queries:
	
	$PSQL test <<EOF
-- Setup
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';	
SET search_path = ag_catalog, public;
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (dummy) RETURN dummy \$\$) as (v ag_catalog.agtype); -- Forcing Secondary node to Self-Load AGE with a dummy MATCH

-- Create node for Alice and Bob
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ CREATE (p:Person {name: 'Alice', age: 30}) RETURN p.name \$\$) AS (a ag_catalog.agtype);
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ CREATE (p:Person {name: 'Bob', age: 35}) RETURN p.name \$\$) AS (a ag_catalog.agtype);

-- Create a relationship between Alice and Bob
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'}) CREATE (a)-[:FRIENDS_WITH]->(b) RETURN a.name \$\$) AS (a ag_catalog.agtype);

EOF

# check if Create queries are sent to node 0
	fgrep "CREATE (p:Person {name: 'Alice', age: 30}) RETURN p.name" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Write queries sent to secondary node.
		./shutdownall
		exit 1
	fi
	fgrep "CREATE (p:Person {name: 'Bob', age: 35}) RETURN p.name" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Write queries sent to secondary node.
		./shutdownall
		exit 1
	fi
	fgrep "MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'}) CREATE (a)-[:FRIENDS_WITH]->(b) RETURN a.name" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Write queries sent to secondary node.
		./shutdownall
		exit 1
	fi
	echo ok: Write queries work.


	
# Read and Update queries
	$PSQL test <<EOF
-- Setup
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';	
SET search_path = ag_catalog, public;
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (dummy) RETURN dummy \$\$) as (v ag_catalog.agtype); -- Forcing secondary node to Self-Load AGE with a dummy MATCH


-- Get all Person nodes
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (p:Person) RETURN p.name \$\$) AS (a ag_catalog.agtype);

-- Get all relationships between Person nodes
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (a:Person)-[r:FRIENDS_WITH]->(b:Person) RETURN a.name \$\$) AS (a ag_catalog.agtype);

-- Update Alice's age 
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (p:Person {name: 'Alice'}) SET p.age = 31 RETURN p.age \$\$) AS (a ag_catalog.agtype);

-- Merge a new node or update if exists with the same name
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MERGE (p:Person {name: 'Charlie', age: 25}) RETURN p.name \$\$) AS (a ag_catalog.agtype);

-- Delete the relationship between Alice and Bob
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (a:Person {name: 'Alice'})-[r:FRIENDS_WITH]->(b:Person {name: 'Bob'}) DELETE r RETURN a.age \$\$) AS (a ag_catalog.agtype);

-- Delete the node for Charlie
SELECT * FROM ag_catalog.cypher('test_graph', \$\$ MATCH (p:Person {name: 'Charlie'}) DETACH DELETE p RETURN p.name \$\$) AS (a ag_catalog.agtype);

EOF

# check if Read queries are sent to node 1

	fgrep "MATCH (p:Person) RETURN p.name" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Read query sent to primary node.
		./shutdownall
		exit 1
	fi

	fgrep "MATCH (a:Person)-[r:FRIENDS_WITH]->(b:Person) RETURN a.name" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Read query sent to primary node.
		./shutdownall
		exit 1
	fi

		fgrep "MATCH (p:Person {name: 'Alice'}) SET p.age = 31 RETURN p.age" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Update query sent to secondary node.
		./shutdownall
		exit 1
	fi

		fgrep "MERGE (p:Person {name: 'Charlie', age: 25}) RETURN p.name" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Merge query sent to secondary node.
		./shutdownall
		exit 1
	fi

		fgrep "MATCH (a:Person {name: 'Alice'})-[r:FRIENDS_WITH]->(b:Person {name: 'Bob'}) DELETE r RETURN a.age" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Delete relationship query sent to secondary node.
		./shutdownall
		exit 1
	fi

		fgrep "MATCH (p:Person {name: 'Charlie'}) DETACH DELETE p RETURN p.name" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: Delete node query sent to secondary node.
		./shutdownall
		exit 1
	fi

	echo ok: Write queries work.


	./shutdownall

	cd ..

done
exit 0
