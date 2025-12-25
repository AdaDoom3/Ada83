-- Test array initialization constraint checking
procedure Test_Array_Init is
   type Arr5 is array(1..5) of Integer;

   -- This should fail: too many elements (6 vs 5)
   A : Arr5 := (1, 2, 3, 4, 5, 6);

begin
   null;
end Test_Array_Init;
