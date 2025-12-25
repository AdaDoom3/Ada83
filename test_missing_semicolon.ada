-- Test missing semicolon detection
procedure Test_Missing_Semicolon is
   X : Integer := 5  -- Missing semicolon
   Y : Integer := 10;
begin
   null;
end Test_Missing_Semicolon;
