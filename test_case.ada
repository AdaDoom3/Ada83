procedure TEST_CASE is
   X : INTEGER := 3;
   Y : INTEGER;
begin
   case X is
      when 0 => Y := 100;
      when 1 => Y := 200;
      when 2 | 3 => Y := 300;
      when others => Y := 400;
   end case;
end TEST_CASE;
