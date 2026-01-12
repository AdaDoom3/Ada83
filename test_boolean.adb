procedure TEST_BOOLEAN is
   A : BOOLEAN;
   B : BOOLEAN;
   C : BOOLEAN;
   X : INTEGER;
begin
   A := TRUE;
   B := FALSE;
   C := A and B;

   if A or B then
      X := 1;
   else
      X := 0;
   end if;

   if not B then
      X := 2;
   end if;
end TEST_BOOLEAN;
