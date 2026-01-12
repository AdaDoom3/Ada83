procedure TEST_NESTED_SIMPLE is
   procedure INNER is
   begin
      null;
   end INNER;
   
   X : INTEGER;
begin
   X := 42;
   INNER;
end TEST_NESTED_SIMPLE;
