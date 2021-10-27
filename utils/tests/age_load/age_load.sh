#! /bin/bash

# make sure we have all 3 input parameters, postgresql version, build name and port number
if [ -z $1 ] || [ -z $2 ] || [ -z $3 ]
   then
      echo "Usage: "$0" <postgresql version> <port number> <build name>"
      echo "Ex:    "$0" 11.4 5432 test_build"
      exit 1
fi

# postgresql version
pver=$1

# port number
port=$2

#build name
build="psql"-$pver-$port-$3
# parent directory

# form build directory
builddir=$HOME/postgresql/postgresql-$pver/$build/

$builddir/bin/pg_ctl stop -D $builddir/data

# starting database
$builddir/bin/pg_ctl -D $builddir/data -l $builddir/logfile start

$builddir/bin/psql -p $port $build -c "DROP SCHEMA IF EXISTS age_load_graph CASCADE;"

$builddir/bin/psql -p $port $build -c "DROP EXTENSION IF EXISTS age CASCADE;"
$builddir/bin/psql -p $port $build -c "CREATE EXTENSION age;"
$builddir/bin/psql -p $port $build -c "LOAD 'age';"
$builddir/bin/psql -p $port $build -c "SET search_path TO ag_catalog; "

cipher_cmd="SELECT ag_catalog.create_graph('age_load_graph');"
$builddir/bin/psql -p $port $build -c "$cipher_cmd"

graph_name="age_load_graph"
file_path="`pwd`/tests/age_load/data/countries.csv"
label="Country"
bin/ag_load -g $graph_name -f $file_path -n $label -p $port -v -d $build

file_path="`pwd`/tests/age_load/data/cities.csv"
label="City"
bin/ag_load -g $graph_name -f $file_path -n $label -p $port -v -d $build


file_path="`pwd`/tests/age_load/data/edges.csv"
label="has_country"
bin/ag_load -g $graph_name -f $file_path -n $label -p $port -e -d $build

# SELECT agtype_access_operator('properties', '"id"') FROM age_load_graph."Country"

$builddir/bin/psql -p $port $build -c "select schema_name FROM information_schema.schemata;"

$builddir/bin/psql -p $port $build -c "\dt ag_catalog.*;"

$builddir/bin/psql -p $port $build -c "\dt+ age_load_graph.*;"

$builddir/bin/psql -p $port $build

cipher_cmd="SELECT count(*) from age_load_graph.\"Country\";"
$builddir/bin/psql -p $port $build -c "$cipher_cmd"

cipher_cmd="SELECT count(*) from age_load_graph.\"City\";"
$builddir/bin/psql -p $port $build -c "$cipher_cmd"

cipher_cmd="SELECT ag_catalog.drop_graph('age_load_graph', true);"
$builddir/bin/psql -p $port $build -c "$cipher_cmd"

$builddir/bin/psql -p $port $build -c "DROP EXTENSION age CASCADE;"

$builddir/bin/pg_ctl stop -D $builddir/data
