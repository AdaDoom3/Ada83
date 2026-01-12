procedure TEST_LOOP is
   I : INTEGER;
   SUM : INTEGER;
begin
   I := 1;
   SUM := 0;
   while I <= 10 loop
      SUM := SUM + I;
      I := I + 1;
   end loop;
end TEST_LOOP;
