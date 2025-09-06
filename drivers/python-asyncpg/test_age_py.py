# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
import json
import pytest
import pytest_asyncio
import decimal
import age
import argparse

TEST_HOST = "127.0.0.1"
TEST_PORT = 5432
TEST_DB = "postgres"
TEST_USER = "postgres"
TEST_PASSWORD = "agens"
TEST_GRAPH_NAME = "test_graph"

# Using pytest fixture for setup and teardown
@pytest_asyncio.fixture
async def ag():
    print("Connecting to Test Graph.....")
    dsn = f"postgres://{TEST_USER}:{TEST_PASSWORD}@{TEST_HOST}:{TEST_PORT}/{TEST_DB}"
    age_conn = age.Age()
    try:
        await age_conn.connect(dsn=dsn, graph=TEST_GRAPH_NAME)
    except Exception as e:
        print(f"Database connection failed: {e}")
        pytest.skip(f"Could not connect to database: {e}")


    yield age_conn

    # Teardown
    print("Deleting Test Graph.....")
    try:
        await age.deleteGraph(age_conn.connection, age_conn.graphName)
    except Exception as e:
        print(f"Could not delete graph: {e}")
    finally:
        await age_conn.close()


@pytest.mark.asyncio
class TestAgeBasic:

    async def testExec(self, ag):
        print("\n---------------------------------------------------")
        print("Test 1: Checking single and multi column Returns.....")
        print("---------------------------------------------------\n")

        # Create and Return single column
        rows = await ag.execCypher(
            "CREATE (n:Person {name: $name, title: $title}) RETURN n",
            params={"name": "Andy", "title": "Developer"}
        )
        for row in rows:
            print("Vertex: %s , Type: %s " % (age.models.Vertex, type(row[0])))
            assert isinstance(row[0], age.models.Vertex)

        # Create and Return multi columns
        rows = await ag.execCypher(
            "CREATE (n:Person {name: $name, title: $title}) RETURN id(n), n.name",
            cols=["id", "name"],
            params={"name": "Jack", "title": "Manager"}
        )
        row = rows[0]
        print("Id: %s , Name: %s" % (row[0], row[1]))
        assert isinstance(row[0], int)

        print("\nTest 1 Successful....")

    async def testQuery(self, ag):
        print("\n--------------------------------------------------")
        print("Test 2: Testing CREATE and query relationships.....")
        print("--------------------------------------------------\n")

        async with ag.connection.transaction():
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Jack"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Andy"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Smith"})
            await ag.execCypher(
                "MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE (a)-[r:worksWith {weight: $weight}]->(b)",
                params={"a_name": "Andy", "b_name": "Jack", "weight": 3}
            )
            await ag.execCypher(
                "MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE p=((a)-[r:worksWith]->(b))",
                params={"a_name": "Jack", "b_name": "Smith"}
            )

        rows = await ag.execCypher("MATCH p=()-[:worksWith]-() RETURN p")
        for row in rows:
            path = row[0]
            print("START:", path[0])
            print("EDGE:", path[1])
            print("END:", path[2])

        rows = await ag.execCypher(
            "MATCH p=(a)-[b]-(c) WHERE b.weight > $weight RETURN a,label(b), b.weight, c",
            cols=["a", "bl", "bw", "c"],
            params={"weight": 2}
        )
        for row in rows:
            start = row[0]
            edgel = row[1]
            edgew = row[2]
            end = row[3]
            print(
                "Relationship: %s %s %s. Edge weight: %s"
                % (start["name"], edgel, end["name"], edgew)
            )
            assert edgew > 2
        print("\nTest 2 Successful...")

    async def testChangeData(self, ag):
        print("\n-------------------------------------------------------")
        print("Test 3: Testing changes in data using SET and REMOVE.....")
        print("-------------------------------------------------------\n")

        await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Joe"})
        rows = await ag.execCypher(
            "CREATE (n:Person {name: $name, title: $title}) RETURN n",
            params={"name": "Smith", "title": "Developer"}
        )
        row = rows[0]
        print("CREATED: ", row[0])

        rows = await ag.execCypher(
            "MATCH (n:Person {name: $name}) SET n.title=$title RETURN n",
            params={"name": "Smith", "title": "Manager"}
        )
        row = rows[0]
        vertex = row[0]
        title1 = vertex["title"]
        print("SET title: ", title1)

        rows = await ag.execCypher("MATCH (p:Person {name: $name}) RETURN p.title", params={"name": "Smith"})
        row = rows[0]
        title2 = row[0]

        assert title1 == title2

        rows = await ag.execCypher(
            "MATCH (n:Person {name: $name}) SET n.bigNum=$num::numeric RETURN n",
            params={"name": "Smith", "num": "-6.45161e+46"}
        )
        row = rows[0]
        vertex = row[0]
        print("SET bigNum: ", vertex["bigNum"])
        bigNum1 = vertex["bigNum"]
        assert decimal.Decimal("-6.45161e+46") == bigNum1

        rows = await ag.execCypher("MATCH (p:Person {name: $name}) RETURN p.bigNum", params={"name": "Smith"})
        row = rows[0]
        bigNum2 = row[0]
        assert bigNum1 == bigNum2

        rows = await ag.execCypher(
            "MATCH (n:Person {name: $name}) REMOVE n.title RETURN n",
            params={"name": "Smith"}
        )
        for row in rows:
            print("REMOVE Prop title: ", row[0])
            assert "title" not in row[0].properties
        print("\nTest 3 Successful....")

    async def testMultipleEdges(self, ag):
        print("\n------------------------------------")
        print("Test 5: Testing Multiple Edges.....")
        print("------------------------------------\n")

        async with ag.connection.transaction():
            await ag.execCypher("CREATE (n:Country {name: $name})", params={"name": "USA"})
            await ag.execCypher("CREATE (n:Country {name: $name})", params={"name": "France"})
            await ag.execCypher("CREATE (n:Country {name: $name})", params={"name": "Korea"})
            await ag.execCypher("CREATE (n:Country {name: $name})", params={"name": "Russia"})

        async with ag.connection.transaction():
            await ag.execCypher(
                "MATCH (a:Country {name: $a_name}), (b:Country {name: $b_name}) CREATE (a)-[r:distance {unit:$unit, value: $value}]->(b)",
                params={"a_name": "USA", "b_name": "France", "unit": "miles", "value": 4760}
            )
            await ag.execCypher(
                "MATCH (a:Country {name: $a_name}), (b:Country {name: $b_name}) CREATE (a)-[r:distance {unit: $unit, value: $value}]->(b)",
                params={"a_name": "France", "b_name": "Korea", "unit": "km", "value": 9228}
            )
            await ag.execCypher(
                "MATCH (a:Country {name: $a_name}), (b:Country {name: $b_name}) CREATE (a)-[r:distance {unit:$unit, value: $value}]->(b)",
                params={"a_name": "Korea", "b_name": "Russia", "unit": "km", "value": 3078}
            )

        rows = await ag.execCypher("""MATCH p=(:Country {name:"USA"})-[:distance]-(:Country)-[:distance]-(:Country)
                RETURN p""")

        count = 0
        output = []
        for row in rows:
            path = row[0]
            for e in path:
                if e.gtype == age.TP_VERTEX:
                    output.append(e.label + " " + e["name"])
                elif e.gtype == age.TP_EDGE:
                    output.append(
                        "---- (distance " + str(e["value"]) + " " + e["unit"] + ") --->"
                    )
                else:
                    output.append("Unknown element. " + str(e))
                count += 1

        formatted_output = " ".join(output)
        print("PATH WITH MULTIPLE EDGES: %s" % formatted_output)
        assert count == 5
        print("\nTest 5 Successful...")

    async def testCollect(self, ag):
        print("\n--------------------------")
        print("Test 6: Testing COLLECT.....")
        print("--------------------------\n")

        async with ag.connection.transaction():
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Joe"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Jack"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Andy"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Smith"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Tom"})

        async with ag.connection.transaction():
            await ag.execCypher(
                "MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE (a)-[r:worksWith {weight: $weight}]->(b)",
                params={"a_name": "Joe", "b_name": "Smith", "weight": 3}
            )
            await ag.execCypher(
                "MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE (a)-[r:worksWith {weight: $weight}]->(b)",
                params={"a_name": "Joe", "b_name": "Tom", "weight": 1}
            )
            await ag.execCypher(
                "MATCH (a:Person {name: $a_name}), (b:Person {name: $b_name}) CREATE (a)-[r:worksWith {weight: $weight}]->(b)",
                params={"a_name": "Joe", "b_name": "Andy", "weight": 5}
            )

        print(" -------- TESTING COLLECT #1 --------")
        rows = await ag.execCypher(
            "MATCH (a)-[:worksWith]->(c) WITH a as V, COLLECT(c) as CV RETURN V.name, CV",
            cols=["V", "CV"],
        )
        for row in rows:
            nm = row[0]
            collected = row[1]
            print(nm, "worksWith", [i["name"] for i in collected])
            assert len(collected) == 3

        print(" -------- TESTING COLLECT #2 --------")
        rows = await ag.execCypher(
            "MATCH (a)-[:worksWith]->(c) WITH a as V, COLLECT(c) as CV RETURN V.name, CV",
            cols=["V1", "CV"],
        )
        for row in rows:
            nm = row[0]
            collected = row[1]
            print(nm, "worksWith", [i["name"] for i in collected])
            assert len(collected) == 3
        print("\nTest 6 Successful...")

    async def testSerialization(self, ag):
        print("\n---------------------------------------")
        print("Test 6: Testing Vertex Serialization.....")
        print("-----------------------------------------\n")

        async with ag.connection.transaction():
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Joe"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Jack"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Andy"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Smith"})
            await ag.execCypher("CREATE (n:Person {name: $name})", params={"name": "Tom"})

        print(" -------- TESTING Output #1 --------")
        rows = await ag.execCypher("MATCH (n) RETURN n")

        for row in rows:
            vertex = row[0]
            try:
                # json.loads will fail if the json str is not properly formatted
                json.loads(vertex.toJson())
                print("Vertex.toJson() returns a correct json string.")
                assert True
            except:
                assert False, "vertex.toJson() did not produce a valid JSON string"

        print(" -------- TESTING Output #2 --------")
        rows = await ag.execCypher("MATCH (n) RETURN n")

        for row in rows:
            vertex = row[0]
            as_str = vertex.toString()
            # Checking if the trailing comma appears in .toString() output
            assert not as_str.endswith(", }}::VERTEX")
        print("Vertex.toString() 'properties' field is formatted properly.")
