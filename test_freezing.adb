procedure TEST_FREEZING is
   -- Test that types are properly frozen and sized
   type POINT is record
      X : INTEGER;
      Y : INTEGER;
   end record;

   type POINT_ARRAY is array (1 .. 3) of POINT;

   PA : POINT_ARRAY;
   P : POINT;
   SUM : INTEGER;
begin
   -- Initialize array of records
   PA(1).X := 10;
   PA(1).Y := 20;
   PA(2).X := 30;
   PA(2).Y := 40;
   PA(3).X := 50;
   PA(3).Y := 60;

   -- Read back and sum
   SUM := PA(1).X + PA(1).Y + PA(2).X + PA(2).Y + PA(3).X + PA(3).Y;

   -- Copy a record
   P := PA(2);
   SUM := P.X + P.Y;
end TEST_FREEZING;
