-- Test incomplete type completion with access
PROCEDURE TEST_INCOMPLETE IS
     TYPE T1;
     TYPE ACC IS ACCESS T1;
     TYPE T1 IS ARRAY(1..2) OF INTEGER;
     A1 : ACC;
     X1 : INTEGER := A1(1);  -- implicit dereference
BEGIN
     NULL;
END TEST_INCOMPLETE;
