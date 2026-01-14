-- Test simple procedure call
procedure TEST_PROC_CALL is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   X : INTEGER;
begin
   X := 42;
   PRINT_INT(X);  -- This should work
end TEST_PROC_CALL;
