--
-- Regression tests for scalar functions
--

-- Test for the tointeger() function

RETURN tointeger('42'), tointeger('not a number'), tointeger(true);

RETURN tointeger(45), tointeger(null), tointeger(false);

-- Test for the tofloat() function

RETURN tofloat('11.5'), tofloat('not a number'), tofloat(true);

RETURN tofloat(45), tofloat('42'), tofloat(null), tofloat(false);

-- Test for the tofloatornull() function

RETURN tofloatornull('11.5'), tofloatornull('not a number'), tofloatornull(true);

RETURN tofloatornull(null), tofloatornull(false), tofloatornull(45), tofloatornull(45.5);
