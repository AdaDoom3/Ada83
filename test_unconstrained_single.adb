procedure TEST_UNCONSTRAINED_SINGLE is
   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   procedure PROCESS(A : INT_ARRAY; NAME : INTEGER) is
      SUM : INTEGER;
   begin
      SUM := 0;
      for I in A'FIRST .. A'LAST loop
         SUM := SUM + A(I);
      end loop;
      PRINT_INT(NAME);
      PRINT_INT(SUM);
      PRINT_NEWLINE;
   end PROCESS;

   ARR1 : INT_ARRAY(1 .. 5);
begin
   ARR1(1) := 1;
   ARR1(2) := 2;
   ARR1(3) := 3;
   ARR1(4) := 4;
   ARR1(5) := 5;
   PROCESS(ARR1, 1);  -- Should print: 1 15
end TEST_UNCONSTRAINED_SINGLE;
