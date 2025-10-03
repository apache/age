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

use Apache\AgePhp\AgeClient\AGTypeParse;
use PHPUnit\Framework\TestCase;

class AgtypeTest extends TestCase
{
    public function testParseVertex()
    {
        $this->assertEquals(
            (object) [
                'id' => 844424930131969,
                'label' => "Part",
                'properties' => (object) [
                    "part_num" => "123",
                    "number" => 3141592653589793,
                    "float" => 3.141592653589793
                ]
            ],
            AGTypeParse::parse('{"id": 844424930131969, "label": "Part", "properties": {"part_num": "123", "number": 3141592653589793, "float": 3.141592653589793}}::vertex')
        );
    }

    public function testParseEdge()
    {
        $this->assertEquals(
            (object) [
                'id' => 1125899906842625,
                'label' => "used_by",
                'end_id' => 844424930131970,
                'start_id' => 844424930131969,
                'properties' => (object) [
                    'quantity' => 1
                ]
            ],
            AGTypeParse::parse('{"id": 1125899906842625, "label": "used_by", "end_id": 844424930131970, "start_id": 844424930131969, "properties": {"quantity": 1}}::edge')
        );
    }

    public function testPath()
    {
        $this->assertEquals(
            [
                (object) [
                    'id' => 844424930131969,
                    'label' => "Part",
                    'properties' => (object) [
                        "part_num" => "123",
                    ]
                ],
                (object) [
                    'id' => 1125899906842625,
                    'label' => "used_by",
                    'end_id' => 844424930131970,
                    'start_id' => 844424930131969,
                    'properties' => (object) [
                        'quantity' => 1
                    ]
                ],
                (object) [
                    'id' => 844424930131970,
                    'label' => "Part",
                    'properties' => (object) [
                        "part_num" => "345",
                    ]
                ],
            ],
            AGTypeParse::parse('[{"id": 844424930131969, "label": "Part", "properties": {"part_num": "123"}}::vertex, {"id": 1125899906842625, "label": "used_by", "end_id": 844424930131970, "start_id": 844424930131969, "properties": {"quantity": 1}}::edge, {"id": 844424930131970, "label": "Part", "properties": {"part_num": "345"}}::vertex]::path')
        );
    }

    public function testNullProperties()
    {
        $this->assertEquals(
            (object) [
                'id' => 1688849860263937,
                'label' => "car",
                'properties' => (object) [
                ]
            ],
            AGTypeParse::parse('{"id": 1688849860263937, "label": "car", "properties": {}}::vertex')
        );
    }

    public function testNestedAgtype()
    {
        $this->assertEquals(
            (object) [
                'id' => 1688849860263937,
                'label' => "car",
                'properties' => (object) [
                    'a' => (object) [
                        'b' => (object) [
                            'c' => (object) [
                                'd' => [
                                    1,
                                    2,
                                    'A'
                                ]
                            ]
                        ]
                    ]
                ]
            ],
            AGTypeParse::parse('{"id": 1688849860263937, "label": "car", "properties": {"a": {"b":{"c":{"d":[1, 2, "A"]}}}}}::vertex')
        );
    }
}