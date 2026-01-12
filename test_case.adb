procedure TEST_CASE is
   X : INTEGER;
   Y : INTEGER;
begin
   X := 2;
   case X is
      when 1 =>
         Y := 10;
      when 2 =>
         Y := 20;
      when 3 =>
         Y := 30;
      when others =>
         Y := 0;
   end case;
end TEST_CASE;
