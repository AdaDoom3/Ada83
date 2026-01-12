procedure TEST_NESTED is
   X : INTEGER;

   procedure INNER is
      Y : INTEGER;
   begin
      Y := 10;
      X := Y + 5;
   end INNER;

begin
   X := 0;
   INNER;
end TEST_NESTED;
