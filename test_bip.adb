-- Test program for Build-in-Place limited type returns
with TEXT_IO;

procedure TEST_BIP is
   -- Define a limited private type
   package Limited_Pkg is
      type Limited_Record is limited private;
      function Make(Value : INTEGER) return Limited_Record;
      function Get_Value(R : Limited_Record) return INTEGER;
   private
      type Limited_Record is limited record
         Data : INTEGER;
      end record;
   end Limited_Pkg;

   package body Limited_Pkg is
      function Make(Value : INTEGER) return Limited_Record is
      begin
         return (Data => Value);
      end Make;

      function Get_Value(R : Limited_Record) return INTEGER is
      begin
         return R.Data;
      end Get_Value;
   end Limited_Pkg;

   use Limited_Pkg;
   X : Limited_Record := Make(42);
begin
   TEXT_IO.PUT_LINE("Limited type test");
   TEXT_IO.PUT("Value: ");
   TEXT_IO.PUT(INTEGER'IMAGE(Get_Value(X)));
   TEXT_IO.NEW_LINE;
end TEST_BIP;
