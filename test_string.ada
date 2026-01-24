procedure TEST_STRING is
   S1 : STRING(1..5) := "Hello";
   S2 : STRING(1..6) := " World";
   S3 : STRING(1..11);
begin
   -- Test string concatenation
   S3 := S1 & S2;
end TEST_STRING;
