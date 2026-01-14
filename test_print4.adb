procedure TEST_PRINT4 is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   X : INTEGER;
begin
   X := 42;
   PRINT_INT(X);  -- This should work
   TEXT_IO.PUT(X);  -- Does this work?
end TEST_PRINT4;
