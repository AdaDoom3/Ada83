procedure TEST_EXCEPTION is
   MY_ERROR : exception;
   VALUE : INTEGER := 0;
begin
   VALUE := 10;
   if VALUE > 5 then
      raise MY_ERROR;
   end if;
   VALUE := 20;
exception
   when MY_ERROR =>
      VALUE := -1;
   when others =>
      VALUE := -2;
end TEST_EXCEPTION;
