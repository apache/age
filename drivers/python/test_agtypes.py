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
import math
import unittest
from decimal import Decimal

import age


class TestAgtype(unittest.TestCase):
    resultHandler = None

    def __init__(self, methodName: str) -> None:
        super().__init__(methodName=methodName)
        self.resultHandler = age.newResultHandler()

    def parse(self, exp):
        return self.resultHandler.parse(exp)

    def test_scalar(self):
        print("\nTesting Scalar Value Parsing. Result : ",  end='')

        mapStr = '{"name": "Smith", "num":123, "yn":true, "bigInt":123456789123456789123456789123456789::numeric}'
        arrStr = '["name", "Smith", "num", 123, "yn", true, 123456789123456789123456789123456789.8888::numeric]'
        strStr = '"abcd"'
        intStr = '1234'
        floatStr = '1234.56789'
        floatStr2 = '6.45161290322581e+46'
        numericStr1 = '12345678901234567890123456789123456789.789::numeric'
        numericStr2 = '12345678901234567890123456789123456789::numeric'
        boolStr = 'true'
        nullStr = ''
        nanStr = "NaN"
        infpStr = "Infinity"
        infnStr = "-Infinity"

        mapVal = self.parse(mapStr)
        arrVal = self.parse(arrStr)
        str = self.parse(strStr)
        intVal = self.parse(intStr)
        floatVal = self.parse(floatStr)
        floatVal2 = self.parse(floatStr2)
        bigFloat = self.parse(numericStr1)
        bigInt = self.parse(numericStr2)
        boolVal = self.parse(boolStr)
        nullVal = self.parse(nullStr)
        nanVal = self.parse(nanStr)
        infpVal = self.parse(infpStr)
        infnVal = self.parse(infnStr)

        self.assertEqual(mapVal, {'name': 'Smith', 'num': 123, 'yn': True, 'bigInt': Decimal(
            '123456789123456789123456789123456789')})
        self.assertEqual(arrVal, ["name", "Smith", "num", 123, "yn", True, Decimal(
            "123456789123456789123456789123456789.8888")])
        self.assertEqual(str,  "abcd")
        self.assertEqual(intVal, 1234)
        self.assertEqual(floatVal, 1234.56789)
        self.assertEqual(floatVal2, 6.45161290322581e+46)
        self.assertEqual(bigFloat, Decimal(
            "12345678901234567890123456789123456789.789"))
        self.assertEqual(bigInt, Decimal(
            "12345678901234567890123456789123456789"))
        self.assertEqual(boolVal, True)
        self.assertTrue(math.isnan(nanVal))
        self.assertTrue(math.isinf(infpVal))
        self.assertTrue(math.isinf(infnVal))

    def test_vertex(self):

        print("\nTesting vertex Parsing. Result : ",  end='')

        vertexExp = '''{"id": 2251799813685425, "label": "Person", 
            "properties": {"name": "Smith", "numInt":123, "numFloat": 384.23424, 
            "bigInt":123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789::numeric, 
            "bigFloat":123456789123456789123456789123456789.12345::numeric, 
            "yn":true, "nullVal": null}}::vertex'''

        vertex = self.parse(vertexExp)
        self.assertEqual(vertex.id,  2251799813685425)
        self.assertEqual(vertex.label,  "Person")
        self.assertEqual(vertex["name"],  "Smith")
        self.assertEqual(vertex["numInt"],  123)
        self.assertEqual(vertex["numFloat"],  384.23424)
        self.assertEqual(vertex["bigInt"],  Decimal(
            "123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789"))
        self.assertEqual(vertex["bigFloat"],  Decimal(
            "123456789123456789123456789123456789.12345"))
        self.assertEqual(vertex["yn"],  True)
        self.assertEqual(vertex["nullVal"],  None)

    def test_path(self):

        print("\nTesting Path Parsing. Result : ",  end='')

        pathExp = '''[{"id": 2251799813685425, "label": "Person", "properties": {"name": "Smith"}}::vertex, 
            {"id": 2533274790396576, "label": "workWith", "end_id": 2251799813685425, "start_id": 2251799813685424, 
                "properties": {"weight": 3, "bigFloat":123456789123456789123456789.12345::numeric}}::edge, 
            {"id": 2251799813685424, "label": "Person", "properties": {"name": "Joe"}}::vertex]::path'''

        path = self.parse(pathExp)
        vertexStart = path[0]
        edge = path[1]
        vertexEnd = path[2]
        self.assertEqual(vertexStart.id,  2251799813685425)
        self.assertEqual(vertexStart.label,  "Person")
        self.assertEqual(vertexStart["name"],  "Smith")

        self.assertEqual(edge.id,  2533274790396576)
        self.assertEqual(edge.label,  "workWith")
        self.assertEqual(edge["weight"],  3)
        self.assertEqual(edge["bigFloat"],  Decimal(
            "123456789123456789123456789.12345"))

        self.assertEqual(vertexEnd.id,  2251799813685424)
        self.assertEqual(vertexEnd.label,  "Person")
        self.assertEqual(vertexEnd["name"],  "Joe")

    def test_vertex_large_array_properties(self):
        """Issue #2367: Parser should handle vertices with large array properties."""
        vertexExp = (
            '{"id": 1125899906842625, "label": "TestNode", '
            '"properties": {"name": "test", '
            '"tags": ["tag1", "tag2", "tag3", "tag4", "tag5", "tag6", "tag7", '
            '"tag8", "tag9", "tag10", "tag11", "tag12"]}}::vertex'
        )
        vertex = self.parse(vertexExp)
        self.assertEqual(vertex.id, 1125899906842625)
        self.assertEqual(vertex.label, "TestNode")
        self.assertEqual(vertex["name"], "test")
        self.assertEqual(len(vertex["tags"]), 12)
        self.assertEqual(vertex["tags"][0], "tag1")
        self.assertEqual(vertex["tags"][11], "tag12")

    def test_vertex_special_characters_in_properties(self):
        """Issue #2367: Parser accepts JSON-escaped property strings and UTF-8."""
        # Input uses json.dumps so quotes, backslashes, and newlines are valid JSON.
        logical_description = 'Quoted "text", path C:\\tmp\\file, line1\nline2, café 雪'
        props = json.dumps(
            {"name": "test", "description": logical_description},
            ensure_ascii=False,
        )
        vertexExp = (
            '{"id": 1125899906842626, "label": "TestNode", '
            '"properties": ' + props + '}::vertex'
        )
        vertex = self.parse(vertexExp)
        self.assertEqual(vertex.id, 1125899906842626)
        self.assertEqual(vertex["name"], "test")
        # The agtype visitor keeps JSON string escapes as literal characters
        # (except UTF-8 code points, which decode normally).
        self.assertEqual(
            vertex["description"],
            'Quoted \\"text\\", path C:\\\\tmp\\\\file, line1\\nline2, café 雪',
        )

    def test_vertex_nested_properties(self):
        """Issue #2367: Parser should handle deeply nested property structures."""
        vertexExp = (
            '{"id": 1125899906842627, "label": "TestNode", '
            '"properties": {"name": "test", '
            '"metadata": {"level1": {"level2": {"level3": "deep_value"}}}}}::vertex'
        )
        vertex = self.parse(vertexExp)
        self.assertEqual(vertex.id, 1125899906842627)
        self.assertEqual(vertex["name"], "test")
        self.assertEqual(vertex["metadata"]["level1"]["level2"]["level3"], "deep_value")

    def test_vertex_empty_properties(self):
        """Parser should handle vertices with empty properties dict."""
        vertexExp = '{"id": 1125899906842628, "label": "EmptyNode", "properties": {}}::vertex'
        vertex = self.parse(vertexExp)
        self.assertEqual(vertex.id, 1125899906842628)
        self.assertEqual(vertex.label, "EmptyNode")
        self.assertEqual(vertex.properties, {})

    def test_vertex_null_property_values(self):
        """Parser should handle vertices with null property values."""
        vertexExp = (
            '{"id": 1125899906842629, "label": "TestNode", '
            '"properties": {"name": "test", "optional": null, "also_null": null}}::vertex'
        )
        vertex = self.parse(vertexExp)
        self.assertEqual(vertex["name"], "test")
        self.assertIsNone(vertex["optional"])
        self.assertIsNone(vertex["also_null"])

    def test_edge_with_complex_properties(self):
        """Parser should handle edges with complex property structures."""
        edgeExp = (
            '{"id": 2533274790396577, "label": "HAS_RELATION", '
            '"end_id": 1125899906842625, "start_id": 1125899906842626, '
            '"properties": {"weight": 3, "tags": ["a", "b", "c"], "active": true}}::edge'
        )
        edge = self.parse(edgeExp)
        self.assertEqual(edge.id, 2533274790396577)
        self.assertEqual(edge.label, "HAS_RELATION")
        self.assertEqual(edge.start_id, 1125899906842626)
        self.assertEqual(edge.end_id, 1125899906842625)
        self.assertEqual(edge["weight"], 3)
        self.assertEqual(edge["tags"], ["a", "b", "c"])
        self.assertEqual(edge["active"], True)

    def test_path_with_multiple_edges(self):
        """Parser should handle paths with multiple edges and complex properties."""
        pathExp = (
            '[{"id": 1, "label": "A", "properties": {"name": "start"}}::vertex, '
            '{"id": 10, "label": "r1", "end_id": 2, "start_id": 1, "properties": {"w": 1}}::edge, '
            '{"id": 2, "label": "B", "properties": {"name": "middle"}}::vertex, '
            '{"id": 11, "label": "r2", "end_id": 3, "start_id": 2, "properties": {"w": 2}}::edge, '
            '{"id": 3, "label": "C", "properties": {"name": "end"}}::vertex]::path'
        )
        path = self.parse(pathExp)
        self.assertEqual(len(path), 5)
        self.assertEqual(path[0]["name"], "start")
        self.assertEqual(path[2]["name"], "middle")
        self.assertEqual(path[4]["name"], "end")

    def test_empty_input(self):
        """Parser should handle empty/null input gracefully."""
        self.assertIsNone(self.parse(''))
        self.assertIsNone(self.parse(None))

    def test_array_of_mixed_types(self):
        """Parser should handle arrays with mixed types including nested arrays."""
        arrStr = '["str", 42, true, null, [1, 2, 3], {"key": "val"}]'
        result = self.parse(arrStr)
        self.assertEqual(result[0], "str")
        self.assertEqual(result[1], 42)
        self.assertEqual(result[2], True)
        self.assertIsNone(result[3])
        self.assertEqual(result[4], [1, 2, 3])
        self.assertEqual(result[5], {"key": "val"})

    def test_malformed_vertex_raises_agtypeerror_or_recovers(self):
        """Issue #2367: Malformed agtype must raise AGTypeError or recover gracefully."""
        from age.exceptions import AGTypeError

        malformed_inputs = [
            '{"id": 1, "label":}::vertex',
            '{"id": 1, "label": "X", "properties": {}::vertex',
            '{::vertex',
            '{"id": 1, "label": "X", "properties": {"key":}}::vertex',
        ]
        for inp in malformed_inputs:
            try:
                result = self.parse(inp)
                # Parser recovery is acceptable — verify the result is a
                # usable Python value (None, container, or model object).
                self.assertTrue(
                    result is None
                    or isinstance(result, (dict, list, tuple))
                    or hasattr(result, "__dict__"),
                    f"Recovered to unexpected type {type(result).__name__}: {inp}"
                )
            except AGTypeError:
                pass  # expected
            except AttributeError:
                self.fail(
                    f"Malformed input raised AttributeError instead of "
                    f"AGTypeError: {inp}"
                )

    def test_truncated_agtype_does_not_crash(self):
        """Issue #2367: Truncated agtype must raise AGTypeError or recover, never AttributeError."""
        from age.exceptions import AGTypeError

        truncated_inputs = [
            '{"id": 1, "label": "X", "properties": {"name": "te',
            '{"id": 1, "label": "X"',
            '[{"id": 1}::vertex, {"id": 2',
        ]
        for inp in truncated_inputs:
            try:
                result = self.parse(inp)
                # Recovery is acceptable for truncated input
                self.assertTrue(
                    result is None
                    or isinstance(result, (dict, list, tuple))
                    or hasattr(result, "__dict__"),
                    f"Recovered to unexpected type {type(result).__name__}: {inp}"
                )
            except AGTypeError:
                pass  # expected
            except AttributeError:
                self.fail(
                    f"Truncated input raised AttributeError instead of "
                    f"AGTypeError: {inp}"
                )


if __name__ == '__main__':
    unittest.main()
