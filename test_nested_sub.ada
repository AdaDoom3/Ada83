procedure TEST_NESTED_SUB is
   OUTER_VAR : INTEGER := 100;

   procedure INNER is
      INNER_VAR : INTEGER;
   begin
      -- Access variable from enclosing scope
      INNER_VAR := OUTER_VAR + 1;
   end INNER;

begin
   OUTER_VAR := 50;
   INNER;
end TEST_NESTED_SUB;
