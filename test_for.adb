procedure TEST_FOR is
   SUM : INTEGER;
   I : INTEGER;
begin
   SUM := 0;
   for I in 1..10 loop
      SUM := SUM + I;
   end loop;
end TEST_FOR;
