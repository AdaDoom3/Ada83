procedure TEST_ENUM is
   type COLOR is (RED, GREEN, BLUE);
   C : COLOR;
   X : INTEGER;
begin
   C := RED;
   case C is
      when RED =>
         X := 1;
      when GREEN =>
         X := 2;
      when BLUE =>
         X := 3;
   end case;
end TEST_ENUM;
