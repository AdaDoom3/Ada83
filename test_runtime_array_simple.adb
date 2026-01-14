-- Simple test for runtime-sized array allocation
procedure TEST_RUNTIME_ARRAY_SIMPLE is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   SIZE : INTEGER := 5;
   ARR : INT_ARRAY(1 .. SIZE);
begin
   -- Test: Runtime-sized array allocation
   for I in 1 .. SIZE loop
      ARR(I) := I * 10;
   end loop;

   for I in 1 .. SIZE loop
      PRINT_INT(ARR(I));
   end loop;
   PRINT_NEWLINE;
end TEST_RUNTIME_ARRAY_SIMPLE;
