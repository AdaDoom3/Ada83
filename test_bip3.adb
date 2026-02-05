-- BIP test with getter
procedure TEST_BIP3 is
   type Limited_Record is limited record
      Data : INTEGER;
   end record;

   function Make(Value : INTEGER) return Limited_Record is
   begin
      return (Data => Value);
   end Make;

   function Get_Data(R : Limited_Record) return INTEGER is
   begin
      return R.Data;
   end Get_Data;

   X : Limited_Record := Make(42);
   Y : INTEGER;
begin
   Y := Get_Data(X);
   if Y /= 42 then
      raise CONSTRAINT_ERROR;
   end if;
end TEST_BIP3;
