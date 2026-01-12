procedure TEST_BOUNDS_ONLY is
   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   procedure SHOW(A : INT_ARRAY; NAME : INTEGER) is
   begin
      PRINT_INT(NAME);
      PRINT_INT(A'FIRST);
      PRINT_INT(A'LAST);
      PRINT_NEWLINE;
   end SHOW;

   ARR1 : INT_ARRAY(1 .. 5);
   ARR2 : INT_ARRAY(10 .. 12);
begin
   ARR1(1) := 1;
   ARR2(10) := 10;
   SHOW(ARR1, 1);  -- Should print: 1 1 5
   SHOW(ARR2, 2);  -- Should print: 2 10 12
end TEST_BOUNDS_ONLY;
