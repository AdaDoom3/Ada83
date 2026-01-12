procedure TEST_CALL is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   X : INTEGER;
begin
   X := 42;
   PRINT_INT(X);
   PRINT_NEWLINE;
end TEST_CALL;
