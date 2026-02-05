-- Simple BIP test
procedure TEST_BIP2 is
   type Limited_Record is limited record
      Data : INTEGER;
   end record;

   function Make(Value : INTEGER) return Limited_Record is
   begin
      return (Data => Value);
   end Make;

   X : Limited_Record := Make(42);
begin
   null;
end TEST_BIP2;
