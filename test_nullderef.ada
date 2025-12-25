-- Test null pointer dereference - should raise CONSTRAINT_ERROR
procedure Test_NullDeref is
   type Int_Ptr is access Integer;
   P : Int_Ptr := null;
   X : Integer;
begin
   X := P.all;
end Test_NullDeref;
