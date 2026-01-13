-- Test unconstrained array return values
procedure TEST_UNCONSTRAINED_RETURN is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   -- Function that creates and returns an unconstrained array
   function MAKE_ARRAY(SIZE : INTEGER) return INT_ARRAY is
      RESULT : INT_ARRAY(1 .. SIZE);
   begin
      for I in 1 .. SIZE loop
         RESULT(I) := I * 10;
      end loop;
      return RESULT;
   end MAKE_ARRAY;

   --  Function that returns a slice
   --  function GET_SLICE(A : INT_ARRAY; FROM : INTEGER; TO : INTEGER) return INT_ARRAY is
   --  begin
   --     return A(FROM .. TO);
   --  end GET_SLICE;

   ARR : INT_ARRAY(1 .. 3);
begin
   -- Test: Function returning unconstrained array
   ARR := MAKE_ARRAY(3);
   for I in ARR'FIRST .. ARR'LAST loop
      PRINT_INT(ARR(I));  -- Should print: 10 20 30
   end loop;
   PRINT_NEWLINE;
end TEST_UNCONSTRAINED_RETURN;
