procedure TEST_ARRAY is
   type INT_ARRAY is array (1 .. 5) of INTEGER;
   A : INT_ARRAY;
   SUM : INTEGER;
begin
   A(1) := 10;
   A(2) := 20;
   A(3) := 30;
   A(4) := 40;
   A(5) := 50;
   SUM := A(1) + A(2) + A(3) + A(4) + A(5);
end TEST_ARRAY;
