-- Test negative bounds
procedure Test_Neg_Bounds is
   type Arr is array(-2..2) of Integer;
   A : Arr := (1, 2, 3, 4, 5);
begin
   null;
end Test_Neg_Bounds;
