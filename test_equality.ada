procedure TEST_EQUALITY is
   type POINT is record
      X : INTEGER;
      Y : INTEGER;
   end record;

   P1 : POINT;
   P2 : POINT;
   RESULT : BOOLEAN;
begin
   P1.X := 10;
   P1.Y := 20;
   P2.X := 10;
   P2.Y := 20;
   RESULT := P1 = P2;
end TEST_EQUALITY;
