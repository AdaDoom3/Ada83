-- Test access to array
PROCEDURE TEST_NESTED3 IS
     TYPE ARR IS ARRAY(1..2) OF INTEGER;
     TYPE ACC IS ACCESS ARR;
     A1 : ACC := NEW ARR'(1, 2);
     X1 : INTEGER := A1(1);  -- implicit dereference
BEGIN
     NULL;
END TEST_NESTED3;
