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

"""Security tests for the Apache AGE Python driver.

Tests input validation, SQL injection prevention, and exception handling.
"""

import unittest
from age.age import (
    validate_graph_name,
    validate_identifier,
    buildCypher,
    _validate_column,
)
from age.exceptions import (
    AgeNotSet,
    GraphNotFound,
    GraphAlreadyExists,
    GraphNotSet,
    InvalidGraphName,
    InvalidIdentifier,
)


class TestGraphNameValidation(unittest.TestCase):
    """Test validate_graph_name rejects dangerous inputs."""

    def test_rejects_empty_string(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('')

    def test_rejects_none(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name(None)

    def test_rejects_non_string(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name(123)

    def test_rejects_digit_start(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('123graph')

    def test_rejects_sql_injection_drop_table(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name("'; DROP TABLE ag_graph; --")

    def test_rejects_sql_injection_semicolon(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name("test'); DROP TABLE users; --")

    def test_rejects_sql_injection_select(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name("graph; SELECT * FROM pg_shadow")

    def test_accepts_hyphenated_graph_name(self):
        # AGE allows hyphens in middle positions of graph names.
        validate_graph_name('my-graph')

    def test_rejects_space(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('my graph')

    def test_accepts_dotted_graph_name(self):
        # AGE allows dots in middle positions of graph names.
        validate_graph_name('my.graph')

    def test_rejects_dollar(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('my$graph')

    def test_rejects_exceeding_63_chars(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('a' * 64)

    def test_accepts_valid_names(self):
        # These should NOT raise
        validate_graph_name('my_graph')
        validate_graph_name('MyGraph')
        validate_graph_name('_pr_ivate')
        validate_graph_name('graph123')
        validate_graph_name('my-graph')
        validate_graph_name('my.graph')
        validate_graph_name('a-b.c_d')
        validate_graph_name('abc')
        validate_graph_name('a' * 63)

    def test_rejects_shorter_than_3_chars(self):
        # AGE requires minimum 3 character graph names.
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('a')
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('ab')

    def test_rejects_name_ending_with_hyphen(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('graph-')

    def test_rejects_name_ending_with_dot(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('graph.')

    def test_rejects_name_starting_with_hyphen(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('-graph')

    def test_rejects_name_starting_with_dot(self):
        with self.assertRaises(InvalidGraphName):
            validate_graph_name('.graph')

    def test_error_message_contains_name(self):
        try:
            validate_graph_name("bad;name")
            self.fail("Expected InvalidGraphName")
        except InvalidGraphName as e:
            self.assertIn("bad;name", str(e))
            self.assertIn("Invalid graph name", str(e))


class TestIdentifierValidation(unittest.TestCase):
    """Test validate_identifier rejects dangerous inputs."""

    def test_rejects_empty_string(self):
        with self.assertRaises(InvalidIdentifier):
            validate_identifier('')

    def test_rejects_none(self):
        with self.assertRaises(InvalidIdentifier):
            validate_identifier(None)

    def test_rejects_sql_injection(self):
        with self.assertRaises(InvalidIdentifier):
            validate_identifier("Person'; DROP TABLE--")

    def test_rejects_special_chars(self):
        with self.assertRaises(InvalidIdentifier):
            validate_identifier("col; DROP TABLE")

    def test_accepts_valid_identifiers(self):
        validate_identifier('Person')
        validate_identifier('KNOWS')
        validate_identifier('_internal')
        validate_identifier('col1')

    def test_error_includes_context(self):
        try:
            validate_identifier("bad;name", "Column name")
            self.fail("Expected InvalidIdentifier")
        except InvalidIdentifier as e:
            self.assertIn("Column name", str(e))


class TestColumnValidation(unittest.TestCase):
    """Test _validate_column prevents injection through column specs."""

    def test_plain_column_name(self):
        self.assertEqual(_validate_column('v'), 'v agtype')

    def test_column_with_type(self):
        self.assertEqual(_validate_column('n agtype'), 'n agtype')

    def test_empty_column(self):
        self.assertEqual(_validate_column(''), '')
        self.assertEqual(_validate_column('  '), '')

    def test_rejects_injection_in_column_name(self):
        with self.assertRaises(InvalidIdentifier):
            _validate_column("v); DROP TABLE ag_graph; --")

    def test_rejects_injection_in_column_type(self):
        with self.assertRaises(InvalidIdentifier):
            _validate_column("v agtype); DROP TABLE")

    def test_rejects_three_part_column(self):
        with self.assertRaises(InvalidIdentifier):
            _validate_column("a b c")

    def test_rejects_semicolon_in_name(self):
        with self.assertRaises(InvalidIdentifier):
            _validate_column("col;")


class TestBuildCypher(unittest.TestCase):
    """Test buildCypher validates columns and rejects injection."""

    def test_default_column(self):
        result = buildCypher('test_graph', 'MATCH (n) RETURN n', None)
        self.assertIn('v agtype', result)

    def test_single_column(self):
        result = buildCypher('test_graph', 'MATCH (n) RETURN n', ['n'])
        self.assertIn('n agtype', result)

    def test_typed_column(self):
        result = buildCypher('test_graph', 'MATCH (n) RETURN n', ['n agtype'])
        self.assertIn('n agtype', result)

    def test_multiple_columns(self):
        result = buildCypher('test_graph', 'MATCH (n) RETURN n', ['a', 'b'])
        self.assertIn('a agtype', result)
        self.assertIn('b agtype', result)

    def test_rejects_injection_in_column(self):
        with self.assertRaises(InvalidIdentifier):
            buildCypher('test_graph', 'MATCH (n) RETURN n',
                        ["v); DROP TABLE ag_graph;--"])

    def test_rejects_none_graph_name(self):
        with self.assertRaises(GraphNotSet):
            buildCypher(None, 'MATCH (n) RETURN n', None)


class TestExceptionConstructors(unittest.TestCase):
    """Test that exception constructors work correctly."""

    def test_age_not_set_no_args(self):
        """AgeNotSet() must work without arguments (previously crashed)."""
        e = AgeNotSet()
        self.assertIsNone(e.name)
        self.assertIn('not set', repr(e))

    def test_age_not_set_with_message(self):
        e = AgeNotSet("custom message")
        self.assertEqual(e.name, "custom message")

    def test_graph_not_found_no_args(self):
        e = GraphNotFound()
        self.assertIsNone(e.name)
        self.assertIn('does not exist', repr(e))

    def test_graph_not_found_with_name(self):
        e = GraphNotFound("test_graph")
        self.assertEqual(e.name, "test_graph")
        self.assertIn('test_graph', repr(e))

    def test_graph_already_exists_no_args(self):
        e = GraphAlreadyExists()
        self.assertIsNone(e.name)
        self.assertIn('already exists', repr(e))

    def test_graph_already_exists_with_name(self):
        e = GraphAlreadyExists("test_graph")
        self.assertEqual(e.name, "test_graph")
        self.assertIn('test_graph', repr(e))

    def test_invalid_graph_name_fields(self):
        e = InvalidGraphName("bad;name", "must be valid")
        self.assertEqual(e.name, "bad;name")
        self.assertEqual(e.reason, "must be valid")
        self.assertIn("bad;name", str(e))
        self.assertIn("must be valid", str(e))

    def test_invalid_identifier_fields(self):
        e = InvalidIdentifier("col;drop", "Column name")
        self.assertEqual(e.name, "col;drop")
        self.assertEqual(e.context, "Column name")
        self.assertIn("col;drop", str(e))


if __name__ == '__main__':
    unittest.main()
