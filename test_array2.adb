procedure TEST_ARRAY2 is
   type INT_ARRAY is array (1 .. 3) of INTEGER;
   A : INT_ARRAY;
   B : INT_ARRAY;
   I : INTEGER;
begin
   A(1) := 10;
   A(2) := 20;
   A(3) := 30;

   B(1) := A(1) * 2;
   B(2) := A(2) * 2;
   B(3) := A(3) * 2;

   I := B(1) + B(2) + B(3);
end TEST_ARRAY2;
