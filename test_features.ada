-- Test various features
procedure TEST_FEATURES is
   -- Basic variables
   X : INTEGER := 10;
   Y : INTEGER;

   -- Function declaration
   function DOUBLE(N : INTEGER) return INTEGER is
   begin
      return N * 2;
   end DOUBLE;

begin
   -- Basic assignment
   Y := X + 5;

   -- If statement
   if Y > 10 then
      X := 1;
   else
      X := 0;
   end if;

   -- Case statement
   case X is
      when 0 => Y := 100;
      when 1 => Y := 200;
      when others => Y := 300;
   end case;

   -- While loop
   while X < 100 loop
      X := X + 1;
   end loop;

   -- Function call
   Y := DOUBLE(X);

end TEST_FEATURES;
