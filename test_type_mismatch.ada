-- Test type mismatch detection
procedure Test_Type_Mismatch is
   X : Integer := 5;
   Y : Boolean;
begin
   Y := X;  -- Type mismatch: assigning Integer to Boolean
end Test_Type_Mismatch;
