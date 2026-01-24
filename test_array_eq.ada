procedure TEST_ARRAY_EQ is
   type INT_ARRAY is array (1..5) of INTEGER;
   A : INT_ARRAY;
   B : INT_ARRAY;
   SAME : BOOLEAN;
begin
   A(1) := 10;
   A(2) := 20;
   B(1) := 10;
   B(2) := 20;
   SAME := A = B;
end TEST_ARRAY_EQ;
