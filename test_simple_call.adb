procedure TEST_SIMPLE_CALL is
   procedure FOO is
   begin
      null;
   end FOO;

   X : INTEGER;
begin
   X := 42;
   FOO;
end TEST_SIMPLE_CALL;
