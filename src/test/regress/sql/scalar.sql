--
-- Regression tests for scalar functions
--

-- Test for the tointegerornull() function

RETURN tointegerornull('42'), tointegerornull('not a number'), tointegerornull(true), tointegerornull(['A', 'B', 'C']);

RETURN tointegerornull(12.5), tointegerornull('13.5'), tointegerornull(45), tointegerornull(false);
