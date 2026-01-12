procedure TEST_FUNCTION is

   function ADD(A : INTEGER; B : INTEGER) return INTEGER is
   begin
      return A + B;
   end ADD;

   X : INTEGER;
   Y : INTEGER;
   Z : INTEGER;
begin
   X := 10;
   Y := 20;
   Z := ADD(X, Y);
end TEST_FUNCTION;
