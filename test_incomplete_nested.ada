-- Test incomplete type across nested package
PROCEDURE TEST_INCOMPLETE_NESTED IS
     TYPE T1;  -- Incomplete at outer level

     PACKAGE INNER IS
          TYPE ACC IS ACCESS T1;  -- Use incomplete T1 from outer
     END INNER;

     TYPE T1 IS ARRAY(1..2) OF INTEGER;  -- Complete T1

     A1 : INNER.ACC;
     X1 : INTEGER := A1(1);  -- implicit dereference of access-to-array
BEGIN
     NULL;
END TEST_INCOMPLETE_NESTED;
