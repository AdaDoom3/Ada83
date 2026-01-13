-- Comprehensive test for nested procedure correctness
-- Tests: frame allocation, variable access, static links, multiple nesting levels
procedure TEST_NESTED_COMPREHENSIVE is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   -- Test 1: Simple nested procedure accessing parent variable
   procedure TEST1 is
      X : INTEGER;

      procedure INNER is
      begin
         PRINT_INT(X);  -- Access parent variable
      end INNER;
   begin
      X := 100;
      INNER;
      PRINT_NEWLINE;
   end TEST1;

   -- Test 2: Multiple nested procedures accessing same parent variable
   procedure TEST2 is
      Y : INTEGER;

      procedure INNER1 is
      begin
         PRINT_INT(Y);
      end INNER1;

      procedure INNER2 is
      begin
         PRINT_INT(Y + 1);
      end INNER2;
   begin
      Y := 200;
      INNER1;
      INNER2;
      PRINT_NEWLINE;
   end TEST2;

   -- Test 3: Three-level nesting
   procedure TEST3 is
      Z : INTEGER;

      procedure LEVEL2 is
         procedure LEVEL3 is
         begin
            PRINT_INT(Z);  -- Access grandparent variable
         end LEVEL3;
      begin
         LEVEL3;
      end LEVEL2;
   begin
      Z := 300;
      LEVEL2;
      PRINT_NEWLINE;
   end TEST3;

   -- Test 4: Nested procedure with unconstrained array parameter
   procedure TEST4 is
      type INT_ARRAY is array (INTEGER range <>) of INTEGER;

      procedure PROCESS(A : INT_ARRAY) is
         SUM : INTEGER;
      begin
         SUM := 0;
         for I in A'FIRST .. A'LAST loop
            SUM := SUM + A(I);
         end loop;
         PRINT_INT(SUM);
      end PROCESS;

      ARR : INT_ARRAY(1 .. 3);
   begin
      ARR(1) := 10;
      ARR(2) := 20;
      ARR(3) := 30;
      PROCESS(ARR);
      PRINT_NEWLINE;
   end TEST4;

begin
   TEST1;  -- Should print: 100
   TEST2;  -- Should print: 200 201
   TEST3;  -- Should print: 300
   TEST4;  -- Should print: 60
end TEST_NESTED_COMPREHENSIVE;
