procedure TEST_FUNC is
   X : INTEGER := 10;

   function DOUBLE(N : INTEGER) return INTEGER is
   begin
      return N * 2;
   end DOUBLE;

begin
   X := DOUBLE(X);
end TEST_FUNC;
