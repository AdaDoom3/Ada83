-- Simple elaboration test with BIP
procedure Test_Elab2 is
   type Limited_Record is limited record
      X : Integer;
      Y : Integer;
   end record;

   function Make(A, B : Integer) return Limited_Record is
   begin
      return (X => A, Y => B);
   end Make;

   function Sum(R : Limited_Record) return Integer is
   begin
      return R.X + R.Y;
   end Sum;

   Obj : Limited_Record := Make(10, 20);
   Result : Integer;
begin
   Result := Sum(Obj);
   if Result /= 30 then
      raise Constraint_Error;
   end if;
end Test_Elab2;
