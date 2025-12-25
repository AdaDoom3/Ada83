-- Test malformed integer literal
procedure Test_Malformed_Int is
   X : Integer := 123abc;  -- Number followed by letters
begin
   null;
end Test_Malformed_Int;
