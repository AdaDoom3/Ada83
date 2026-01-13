-- Test deeply nested procedures accessing grandparent variables
procedure TEST_DEEP_NESTING is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   -- Test 1: Three-level nesting
   procedure LEVEL1 is
      X : INTEGER;

      procedure LEVEL2 is
         Y : INTEGER;

         procedure LEVEL3 is
         begin
            PRINT_INT(X);  -- Access grandparent variable
            PRINT_INT(Y);  -- Access parent variable
         end LEVEL3;
      begin
         Y := 200;
         LEVEL3;
      end LEVEL2;
   begin
      X := 100;
      LEVEL2;
      PRINT_NEWLINE;
   end LEVEL1;

   -- Test 2: Four-level nesting
   procedure DEEP is
      A : INTEGER;

      procedure L2 is
         B : INTEGER;

         procedure L3 is
            C : INTEGER;

            procedure L4 is
            begin
               PRINT_INT(A);  -- Great-grandparent
               PRINT_INT(B);  -- Grandparent
               PRINT_INT(C);  -- Parent
            end L4;
         begin
            C := 3;
            L4;
         end L3;
      begin
         B := 2;
         L3;
      end L2;
   begin
      A := 1;
      L2;
      PRINT_NEWLINE;
   end DEEP;

begin
   LEVEL1;  -- Should print: 100 200
   DEEP;    -- Should print: 1 2 3
end TEST_DEEP_NESTING;
