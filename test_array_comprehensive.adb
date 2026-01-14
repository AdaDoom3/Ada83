-- Comprehensive array operations test
procedure TEST_ARRAY_COMPREHENSIVE is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   -- Test 1: Fixed size array
   A : INT_ARRAY(1 .. 5);

   -- Test 2: Runtime-sized array
   SIZE : constant INTEGER := 3;
   B : INT_ARRAY(1 .. SIZE);

   -- Test 3: Array function
   function MAKE_ARRAY(N : INTEGER) return INT_ARRAY is
      RESULT : INT_ARRAY(1 .. N);
   begin
      for I in 1 .. N loop
         RESULT(I) := I * 10;
      end loop;
      return RESULT;
   end MAKE_ARRAY;

   -- Test 4: Array assignment
   C : INT_ARRAY(1 .. 3);

begin
   -- Initialize array A
   for I in 1 .. 5 loop
      A(I) := I;
   end loop;

   -- Print A
   for I in 1 .. 5 loop
      PRINT_INT(A(I));
   end loop;
   PRINT_NEWLINE;

   -- Initialize and print B
   for I in 1 .. SIZE loop
      B(I) := I + 10;
   end loop;

   for I in 1 .. SIZE loop
      PRINT_INT(B(I));
   end loop;
   PRINT_NEWLINE;

   -- Test function call
   C := MAKE_ARRAY(3);
   for I in 1 .. 3 loop
      PRINT_INT(C(I));
   end loop;
   PRINT_NEWLINE;

end TEST_ARRAY_COMPREHENSIVE;
