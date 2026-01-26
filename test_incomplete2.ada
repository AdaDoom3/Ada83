-- Test incomplete type with nested package in between
PROCEDURE TEST_INCOMPLETE2 IS
     TYPE T1;  -- Incomplete

     PACKAGE INNER IS
     END INNER;

     TYPE T1 IS ARRAY(1..2) OF INTEGER;  -- Complete T1

     TYPE ACC IS ACCESS T1;  -- ACC after T1 is complete
     A1 : ACC;
     X1 : INTEGER := A1(1);  -- should work
BEGIN
     NULL;
END TEST_INCOMPLETE2;
