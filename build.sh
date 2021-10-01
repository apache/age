#! /usr/bin/bash

pver=$1
port=$2
build="psql"-$pver-$port-$3

path="$HOME/postgresql/postgresql-$pver/$build/bin"

#REGRESS_OPTS=--encoding=UTF-8 

make PG_CONFIG=$path/pg_config clean

make PG_CONFIG=$path/pg_config 

make PG_CONFIG=$path/pg_config install

make PG_CONFIG=$path/pg_config installcheck

find src -type f -name '*\.o' -exec rm {} \;
