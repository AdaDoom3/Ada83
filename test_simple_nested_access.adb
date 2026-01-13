-- Simple test: nested procedure accessing parent variable
procedure TEST_SIMPLE_NESTED_ACCESS is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   X : INTEGER;

   procedure INNER is
   begin
      PRINT_INT(X);  -- Access parent variable
   end INNER;
begin
   X := 100;
   INNER;
   PRINT_NEWLINE;
end TEST_SIMPLE_NESTED_ACCESS;
