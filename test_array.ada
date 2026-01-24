procedure TEST_ARRAY is
   type INT_ARRAY is array (1..10) of INTEGER;
   DATA : INT_ARRAY;
   SUM : INTEGER := 0;
begin
   DATA(1) := 10;
   DATA(2) := 20;
   SUM := DATA(1) + DATA(2);
end TEST_ARRAY;
