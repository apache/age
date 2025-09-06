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

from decimal import Decimal
import math
import age


class TestAgtype:
    def parse(self, exp):
        return age.newResultHandler().parse(exp)

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
        str_val = self.parse(strStr)
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

        assert mapVal == {'name': 'Smith', 'num': 123, 'yn': True, 'bigInt': Decimal(
            '123456789123456789123456789123456789')}
        assert arrVal == ["name", "Smith", "num", 123, "yn", True, Decimal(
            "123456789123456789123456789123456789.8888")]
        assert str_val == "abcd"
        assert intVal == 1234
        assert floatVal == 1234.56789
        assert floatVal2 == 6.45161290322581e+46
        assert bigFloat == Decimal(
            "12345678901234567890123456789123456789.789")
        assert bigInt == Decimal(
            "12345678901234567890123456789123456789")
        assert boolVal is True
        assert math.isnan(nanVal)
        assert math.isinf(infpVal)
        assert math.isinf(infnVal)
        assert nullVal is None

    def test_vertex(self):

        print("\nTesting vertex Parsing. Result : ",  end='')

        vertexExp = '''{"id": 2251799813685425, "label": "Person",
            "properties": {"name": "Smith", "numInt":123, "numFloat": 384.23424,
            "bigInt":123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789::numeric,
            "bigFloat":123456789123456789123456789123456789.12345::numeric,
            "yn":true, "nullVal": null}}::vertex'''

        vertex = self.parse(vertexExp)
        assert vertex.id == 2251799813685425
        assert vertex.label == "Person"
        assert vertex["name"] == "Smith"
        assert vertex["numInt"] == 123
        assert vertex["numFloat"] == 384.23424
        assert vertex["bigInt"] == Decimal(
            "123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789")
        assert vertex["bigFloat"] == Decimal(
            "123456789123456789123456789123456789.12345")
        assert vertex["yn"] is True
        assert vertex["nullVal"] is None

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
        assert vertexStart.id == 2251799813685425
        assert vertexStart.label == "Person"
        assert vertexStart["name"] == "Smith"

        assert edge.id == 2533274790396576
        assert edge.label == "workWith"
        assert edge["weight"] == 3
        assert edge["bigFloat"] == Decimal(
            "123456789123456789123456789.12345")

        assert vertexEnd.id == 2251799813685424
        assert vertexEnd.label == "Person"
        assert vertexEnd["name"] == "Joe"
