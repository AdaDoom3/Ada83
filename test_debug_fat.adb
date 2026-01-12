procedure TEST_DEBUG_FAT is
   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   procedure SHOW_BOUNDS(A : INT_ARRAY) is
   begin
      PRINT_INT(A'FIRST);
      PRINT_INT(A'LAST);
      PRINT_NEWLINE;
   end SHOW_BOUNDS;

   ARR1 : INT_ARRAY(3 .. 7);
begin
   ARR1(3) := 10;
   ARR1(4) := 20;
   ARR1(5) := 30;
   ARR1(6) := 40;
   ARR1(7) := 50;
   SHOW_BOUNDS(ARR1);  -- Should print: 3 7
end TEST_DEBUG_FAT;
