# Creating Graph from CSV files

AGLOAD Version 0.1

AGLOAD aka ag_load is a utility built to create graph from csv files. 

The current version allows users to create nodes and edges (vertices and edges) from csv files formatted in a specific way. 

A CSV file for nodes shall be formatted aw following; 

| field name | Field description                                            |
| ---------- | ------------------------------------------------------------ |
| id         | it shall be the first column of the file and all values shall be positive integer. |
| Properties | all other columns contains the properties for the nodes. Header row shall contain the name of property |

Similarly a CSV file for edges shall be formatted as following 

| field name        | Field description                                            |
| ----------------- | ------------------------------------------------------------ |
| start_id          | node id of the node from where the edge is stated. This id shall be present in nodes.csv file. |
| start_vertex_type | class of the node                                            |
| end_id            | end id of the node at which the edge shall be termintated    |
| end_vertex_type   | Class of the node                                            |
| properties        | properties of the edge. header shall contain the property name |

example files can be viewed at `utils/test/age_load/data`

## Compiling the code 

To compile the code in the util folder run 
```
make clean && make
```

This shall create a bin directory in the utill folder and an executeable with the name `ag_load` 

## Adding vertices to the existing graph 

Now you can run the the script as following 

For vertices 

```
bin/ag_load -v -h <host> -p <port> -g <graph_name> -n <vertix label> -d <database name> -f <file path> 
```

For edges 

```
bin/ag_load -e -h <host> -p <port> -g <graph_name> -n <vertix label> -d <database name> -f <file path> 
```

## Details abut arguments 

| Argument | Details                                                     | Default     |
| -------- | ----------------------------------------------------------- | ----------- |
| -h       | Host name or IP address                                     | `localhost` |
| -p       | Port number                                                 | `5432`      |
| -u       | Postgressql username                                        | `postgres`  |
| -w       | Postgressql password leave blank for now password           |             |
| -v       | Flag that tell program to create nodes/vertices             |             |
| -e       | Flag that tells program to create edges                     |             |
| -n       | label for the vertices or edges                             |             |
| -f       | path of the file that will be used to create nodes or edges |             |
|          |                                                             |             |

## Test scripts

We have provided the test scripts and test data to test this utility you can run the script as following 

```shell
./test/age_load.sh <postgresql version> <port number> <build name>
```

Test script usage 

```
echo "Usage: "$0" <postgresql version> <port number> <build name>"
      echo "Ex:    "$0" 11.4 5432 test_build"
      exit 1
```



## Version History 

0.1 version 
