procedure TEST_RECORD is
   type POINT is record
      X : INTEGER;
      Y : INTEGER;
   end record;

   P : POINT;
   SUM : INTEGER;
begin
   P.X := 10;
   P.Y := 20;
   SUM := P.X + P.Y;
end TEST_RECORD;
