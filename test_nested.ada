-- Test nested package body visibility
PROCEDURE TEST_NESTED IS

     PACKAGE PACK1 IS
          TYPE T1;
          PACKAGE PACK2 IS
               TYPE ACC1 IS ACCESS T1;
          END PACK2;
          TYPE T1 IS ARRAY ( 1 .. 2 ) OF INTEGER;
     END PACK1;

     PACKAGE BODY PACK1 IS
          A1 : PACK2.ACC1 := NEW T1'(2,4);

          PACKAGE BODY PACK2 IS
               X1 : INTEGER := A1(1);
          END PACK2;
     END PACK1;

BEGIN
     NULL;
END TEST_NESTED;
