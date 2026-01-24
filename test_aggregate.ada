procedure TEST_AGGREGATE is
   -- Test record aggregates
   type POINT is record
      X : INTEGER;
      Y : INTEGER;
   end record;

   -- Test array aggregates
   type INT_ARRAY is array (1..5) of INTEGER;

   P1 : POINT;
   P2 : POINT;
   ARR : INT_ARRAY;
   SUM : INTEGER := 0;
begin
   -- Positional record aggregate
   P1 := (10, 20);

   -- Named record aggregate
   P2 := (X => 100, Y => 200);

   -- Array with positional
   ARR := (1, 2, 3, 4, 5);

   -- Sum array elements
   SUM := ARR(1) + ARR(2) + ARR(3);
end TEST_AGGREGATE;
