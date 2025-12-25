-- Test division by zero - should raise CONSTRAINT_ERROR
procedure Test_DivZero is
   X : Integer := 10;
   Y : Integer := 0;
   Z : Integer;
begin
   Z := X / Y;
end Test_DivZero;
