-- Test nested package body visibility (direct array)
PROCEDURE TEST_NESTED2 IS

     PACKAGE PACK1 IS
     END PACK1;

     PACKAGE BODY PACK1 IS
          A1 : ARRAY(1..2) OF INTEGER := (1, 2);

          PACKAGE PACK2 IS
          END PACK2;

          PACKAGE BODY PACK2 IS
               X1 : INTEGER := A1(1);
          END PACK2;
     END PACK1;

BEGIN
     NULL;
END TEST_NESTED2;
