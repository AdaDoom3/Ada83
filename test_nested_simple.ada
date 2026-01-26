-- Test nested package body visibility with simple type
PROCEDURE TEST_NESTED_SIMPLE IS

     TYPE ARR IS ARRAY(1..2) OF INTEGER;
     TYPE ACC IS ACCESS ARR;

     PACKAGE PACK1 IS
     END PACK1;

     PACKAGE BODY PACK1 IS
          A1 : ACC;

          PACKAGE PACK2 IS
          END PACK2;

          PACKAGE BODY PACK2 IS
               X1 : INTEGER := A1(1);  -- Can PACK2 body see A1 from PACK1 body?
          END PACK2;
     END PACK1;

BEGIN
     NULL;
END TEST_NESTED_SIMPLE;
