-- Test lexer error detection
procedure Test_Lexer_Errors is
   -- Test 1: Number followed by letter (should error)
   X : Integer := 123abc;

   -- Test 2: Keyword with extra characters (should error)
   Y : Integer := beginxyz;

   -- Test 3: Unterminated string (should error)
   S : String := "hello;

begin
   null;
end Test_Lexer_Errors;
