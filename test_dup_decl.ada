-- Test duplicate declaration detection
procedure Test_Dup_Decl is
   X : Integer := 5;
   X : Integer := 10;  -- Duplicate declaration
begin
   null;
end Test_Dup_Decl;
