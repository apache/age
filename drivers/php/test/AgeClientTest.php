<?php
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

namespace Test\Apache\AgePhp;

use Apache\AgePhp\AgeClient\AgeClient;
use Apache\AgePhp\AgeClient\Exceptions\AgeClientQueryException;
use PHPUnit\Framework\TestCase;

class AgeClientTest extends TestCase
{
    private AgeClient $ageClient;
    private const CONNECTION_DETAILS = [
        'host' => '127.0.0.1',
        'port' => 5455,
        'database' => 'pg_test',
        'user' => 'db_user',
        'password' => 'db_password',
    ];
    private const TEST_GRAPH_NAME = 'age-test';

    private static function connect(): AgeClient
    {
        return new AgeClient(
            self::CONNECTION_DETAILS['host'],
            self::CONNECTION_DETAILS['port'],
            self::CONNECTION_DETAILS['database'],
            self::CONNECTION_DETAILS['user'],
            self::CONNECTION_DETAILS['password'],
        );
    }

    public function setUp(): void
    {
        $this->ageClient = self::connect();
        $this->ageClient->createGraph(self::TEST_GRAPH_NAME);
    }

    public function tearDown(): void
    {
        $this->ageClient->dropGraph(self::TEST_GRAPH_NAME);
        $this->ageClient->close();
    }

    public function testCreateAndMatchFetchAll()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                CREATE
                    (a:Part {part_num: '123'}),
                    (b:Part {part_num: '345'}),
                    (c:Part {part_num: '456'}),
                    (d:Part {part_num: '789'})
            $$) as (a agtype);
        ");

        $results = $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                MATCH
                    (a:Part)
                RETURN a
            $$) as (a agtype);
        ")->fetchAll();

        $this->assertEquals(
            [
                [
                    'a' => (object) [
                        'id' => 844424930131969,
                        'label' => 'Part',
                        'properties' => (object) [
                            'part_num' => '123'
                        ]
                    ]
                ],
                [
                    'a' => (object) [
                        'id' => 844424930131970,
                        'label' => 'Part',
                        'properties' => (object) [
                            'part_num' => '345'
                        ]
                    ]
                ],[
                    'a' => (object) [
                        'id' => 844424930131971,
                        'label' => 'Part',
                        'properties' => (object) [
                            'part_num' => '456'
                        ]
                    ]
                ],[
                    'a' => (object) [
                        'id' => 844424930131972,
                        'label' => 'Part',
                        'properties' => (object) [
                            'part_num' => '789'
                        ]
                    ]
                ],
            ],
            $results
        );
    }

    public function testCreateAndMatchFetchRow()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                CREATE
                    (a:Part {part_num: '123'}),
                    (b:Part {part_num: '345'}),
                    (c:Part {part_num: '456'}),
                    (d:Part {part_num: '789'})
            $$) as (a agtype);
        ");

        $results = $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                MATCH
                    (a:Part)
                RETURN a
            $$) as (a agtype);
        ")->fetchRow();

        $this->assertEquals(
            [
                (object) [
                    'id' => 844424930131969,
                    'label' => 'Part',
                    'properties' => (object) [
                        'part_num' => '123'
                    ]
                ]
            ],
            $results
        );
    }

    public function testCreateAndMatchFetchNextRow()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                CREATE
                    (a:Part {part_num: '123'}),
                    (b:Part {part_num: '345'}),
                    (c:Part {part_num: '456'}),
                    (d:Part {part_num: '789'})
            $$) as (a agtype);
        ");

        $results = $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                MATCH
                    (a:Part)
                RETURN a
            $$) as (a agtype);
        ")->fetchRow(1);

        $this->assertEquals(
            [
                (object) [
                    'id' => 844424930131970,
                    'label' => 'Part',
                    'properties' => (object) [
                        'part_num' => '345'
                    ]
                ]
            ],
            $results
        );
    }

    public function testCreateAndMatchFetchInvalidRow()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                CREATE
                    (a:Part {part_num: '123'}),
                    (b:Part {part_num: '345'}),
                    (c:Part {part_num: '456'}),
                    (d:Part {part_num: '789'})
            $$) as (a agtype);
        ");

        $this->expectWarning('PHPUnit\Framework\Error\Warning');
        
        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                MATCH
                    (a:Part)
                RETURN a
            $$) as (a agtype);
        ")->fetchRow(4);
    }

    public function testReturnValue()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                CREATE
                    (a:Part {part_num: '123'}),
                    (b:Part {part_num: '345'}),
                    (c:Part {part_num: '456'}),
                    (d:Part {part_num: '789'})
            $$) as (a agtype);
        ");

        $results = $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                MATCH
                    (a:Part)
                RETURN a.part_num
            $$) as (a agtype);
        ")->fetchAll();

        $this->assertEquals(
            [
                [
                    'a' => 123
                ],
                [
                    'a' => 345
                ],
                [
                    'a' => 456
                ],
                [
                    'a' => 789
                ],
            ],
            $results
        );
    }

    public function testReturnProperties()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                CREATE
                    (a:Part {part_num: '123'}),
                    (b:Part {part_num: '345'}),
                    (c:Part {part_num: '456'}),
                    (d:Part {part_num: '789'})
            $$) as (a agtype);
        ");

        $results = $this->ageClient->query("
            SELECT * FROM cypher('$graphName', $$
                MATCH
                    (a:Part)
                RETURN properties(a)
            $$) as (a agtype);
        ")->fetchAll();

        $this->assertEquals(
            [
                [
                    'a' => (object) [
                        'part_num' => '123'
                    ]
                ],
                [
                    'a' => (object) [
                        'part_num' => '345'
                    ]
                    ],
                [
                    'a' => (object) [
                        'part_num' => '456'
                    ]
                    ],
                [
                    'a' => (object) [
                        'part_num' => '789'
                    ]
                ]
            ],
            $results
        );
    }

    public function testCypherQuery()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->cypherQuery($graphName, 0, "
            CREATE
                (a:Part {part_num: \$p1}),
                (b:Part {part_num: \$p2}),
                (c:Part {part_num: \$p3}),
                (d:Part {part_num: \$p4})
        ",[
            'p1' => '123',
            'p2' => '345',
            'p3' => '456',
            'p4' => '789'
        ]);

        $results = $this->ageClient->cypherQuery($graphName, 0, "
            MATCH
                (a:Part)
            RETURN a
        ")->fetchRow();

        $this->assertEquals(
            [
                (object) [
                    'id' => 844424930131969,
                    'label' => 'Part',
                    'properties' => (object) [
                        'part_num' => '123'
                    ]
                ]
            ],
            $results
        );
    }

    public function testCypherQueryMultipleAgtypes()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->cypherQuery($graphName, 0, "
            CREATE
                (a:Person {name: \$p1}),
                (b:Person {name: \$p2}),
                (a)-[:KNOWS]->(b)
        ",[
            'p1' => 'Steven',
            'p2' => 'Mary'
        ]);

        $results = $this->ageClient->cypherQuery($graphName, 3, "
            MATCH
                (a:Person {name: \$p1})-[r]-(b)
            RETURN *
        ",[
            'p1' => 'Steven'
        ])->fetchRow();

        $this->assertEquals(
            [
                (object) [
                    'id' => 844424930131969,
                    'label' => 'Person',
                    'properties' => (object) [
                        'name' => 'Steven'
                    ]
                ],
                (object) [
                    'id' => 1125899906842625,
                    'label' => 'KNOWS',
                    'properties' => (object) [
                    ],
                    'end_id' => 844424930131970,
                    'start_id' => 844424930131969
                ],
                (object) [
                    'id' => 844424930131970,
                    'label' => 'Person',
                    'properties' => (object) [
                        'name' => 'Mary'
                    ]
                ],
            ],
            $results
        );
    }

    public function testCypherQueryMultipleAgtypesInvalidColumnCount()
    {
        $graphName = self::TEST_GRAPH_NAME;

        $this->ageClient->cypherQuery($graphName, 0, "
            CREATE
                (a:Person {name: \$p1}),
                (b:Person {name: \$p2}),
                (a)-[:KNOWS]->(b)
        ",[
            'p1' => 'Steven',
            'p2' => 'Mary'
        ]);
        
        $this->expectWarning('PHPUnit\Framework\Error\Warning');
        $this->expectWarningMessage('Query failed: ERROR:  return row and column definition list do not match');

        $results = $this->ageClient->cypherQuery($graphName, 1, "
            MATCH
                (a:Person {name: \$p1})-[r]-(b)
            RETURN *
        ",[
            'p1' => 'Steven'
        ])->fetchRow();

        $this->assertEquals(
            [
                (object) [
                    'id' => 844424930131969,
                    'label' => 'Person',
                    'properties' => (object) [
                        'name' => 'Steven'
                    ]
                ]
            ],
            $results
        );
    }

    public function testCreateDuplicateGraph()
    {
        $this->ageClient->dropGraph(self::TEST_GRAPH_NAME);
        $this->ageClient->createGraph(self::TEST_GRAPH_NAME);
        
        $this->expectWarning('PHPUnit\Framework\Error\Warning');
        $this->ageClient->createGraph(self::TEST_GRAPH_NAME);
    }

    public function testFetchWithoutQuery()
    {
        $this->expectException(AgeClientQueryException::class);
        $this->expectExceptionMessage('No result from prior query to fetch from');
        $this->ageClient->fetchRow();
    }
}