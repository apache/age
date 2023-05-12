#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for load balancing.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

# sleep time after reload in seconds
st=10

for mode in r
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

	$PSQL -p 11002 template1 <<EOF
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';
SELECT create_graph('test_graph');
EOF

	$PSQL -p 11003 template1 <<EOF
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';
EOF


	$PSQL template1 <<EOF


SELECT * FROM cypher('test_graph', \$\$ CREATE (:vertex1 {i: 123}) \$\$) as (v agtype);
SELECT * FROM cypher('test_graph', \$\$ CREATE (:vertex2 {i: 124}) \$\$) as (v agtype);

EOF


# check if simple load balance worked
	fgrep "SELECT * FROM cypher('test_graph', \$\$ MATCH (v) RETURN v \$\$) as (v agtype);" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: select is sent to zero-weight node.
		./shutdownall
		exit 1
	fi
	echo ok: simple load balance works.


# check a set of cypher queries:
	
	$PSQL template1 <<EOF
-- Create node for Alice and Bob
SELECT * FROM cypher('test_graph', \$\$ CREATE (p:Person {name: 'Alice', age: 30}) RETURN p.name \$\$) AS (a agtype);
SELECT * FROM cypher('test_graph', \$\$ CREATE (p:Person {name: 'Bob', age: 35}) RETURN p.name \$\$) AS (a agtype);

-- Create a relationship between Alice and Bob
SELECT * FROM cypher('test_graph', \$\$ MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'}) CREATE (a)-[:FRIENDS_WITH]->(b) RETURN a.name \$\$) AS (a agtype);

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
	echo ok: Wrtie queries work.


	
# Read and Update query
	$PSQL template1 <<EOF

-- Get all Person nodes
SELECT * FROM cypher('test_graph', \$\$ MATCH (p:Person) RETURN p.name \$\$) AS (a agtype);


-- Get all relationships between Person nodes
SELECT * FROM cypher('test_graph', \$\$ MATCH (a:Person)-[r:FRIENDS_WITH]->(b:Person) RETURN a.name \$\$) AS (a agtype);

-- Update Alice's age 
SELECT * FROM cypher('test_graph', \$\$ MATCH (p:Person {name: 'Alice'}) SET p.age = 31 RETURN p.age \$\$) AS (a agtype);

-- Merge a new node or update if exists with the same name
SELECT * FROM cypher('test_graph', \$\$ MERGE (p:Person {name: 'Charlie', age: 25}) RETURN p.name \$\$) AS (a agtype);

-- Delete the relationship between Alice and Bob
SELECT * FROM cypher('test_graph', \$\$ MATCH (a:Person {name: 'Alice'})-[r:FRIENDS_WITH]->(b:Person {name: 'Bob'}) DELETE r RETURN a.age \$\$) AS (a agtype);

-- Delete the node for Charlie
SELECT * FROM cypher('test_graph', \$\$ MATCH (p:Person {name: 'Charlie'}) DETACH DELETE p RETURN p.name \$\$) AS (a agtype);

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

	echo ok: Wrtie queries work.


	./shutdownall

	cd ..

done
exit 0
