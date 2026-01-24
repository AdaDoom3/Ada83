procedure TEST_NESTED is
   X : INTEGER := 5;

   procedure INCREMENT is
   begin
      X := X + 1;
   end INCREMENT;

begin
   INCREMENT;
   X := X * 2;
end TEST_NESTED;
