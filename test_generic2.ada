-- Test generic procedure with IN OUT parameters
procedure TEST_GENERIC2 is

   -- Generic procedure for swapping
   generic
      type ELEMENT is private;
   procedure SWAP(A, B : in out ELEMENT);

   procedure SWAP(A, B : in out ELEMENT) is
      TEMP : ELEMENT;
   begin
      TEMP := A;
      A := B;
      B := TEMP;
   end SWAP;

   -- Instantiate for INTEGER
   procedure SWAP_INT is new SWAP(INTEGER);

   X : INTEGER := 10;
   Y : INTEGER := 20;

begin
   SWAP_INT(X, Y);
   -- X should be 20, Y should be 10
end TEST_GENERIC2;
