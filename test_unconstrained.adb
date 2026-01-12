procedure TEST_UNCONSTRAINED is
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
   ARR2 : INT_ARRAY(10 .. 12);
begin
   ARR1(1) := 1;
   ARR1(2) := 2;
   ARR1(3) := 3;
   ARR1(4) := 4;
   ARR1(5) := 5;

   ARR2(10) := 10;
   ARR2(11) := 20;
   ARR2(12) := 30;

   PROCESS(ARR1, 1);  -- Should print: 1 15
   PROCESS(ARR2, 2);  -- Should print: 2 60
end TEST_UNCONSTRAINED;
