-- Very simple test for unconstrained array return
procedure TEST_UNCONSTRAINED_SIMPLE_RETURN is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   function MAKE_ARRAY return INT_ARRAY is
      RESULT : INT_ARRAY(1 .. 3);
   begin
      RESULT(1) := 10;
      RESULT(2) := 20;
      RESULT(3) := 30;
      return RESULT;
   end MAKE_ARRAY;

   ARR : INT_ARRAY(1 .. 3);
begin
   ARR := MAKE_ARRAY;
   PRINT_INT(ARR(1));
end TEST_UNCONSTRAINED_SIMPLE_RETURN;
