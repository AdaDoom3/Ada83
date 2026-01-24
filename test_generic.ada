-- Test generic procedure with simple IN parameters
procedure TEST_GENERIC is

   -- Generic function with type parameter
   generic
      type ELEMENT is private;
   function ADD_ONE(X : ELEMENT) return ELEMENT;

   function ADD_ONE(X : ELEMENT) return ELEMENT is
   begin
      return X;  -- Just return X for now (can't add 1 without knowing type)
   end ADD_ONE;

   -- Instantiate for INTEGER
   function ADD_ONE_INT is new ADD_ONE(INTEGER);

   A : INTEGER := 10;
   B : INTEGER;

begin
   B := ADD_ONE_INT(A);
   -- B should be 10
end TEST_GENERIC;
