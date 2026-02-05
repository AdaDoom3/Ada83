-- Test elaboration with nested packages and variable initialization
procedure Test_Elab is
   package Counter is
      Value : Integer := 0;
      procedure Increment;
      function Get return Integer;
   end Counter;

   package body Counter is
      procedure Increment is
      begin
         Value := Value + 1;
      end Increment;

      function Get return Integer is
      begin
         return Value;
      end Get;
   begin
      -- Package body elaboration: initialize to 10
      Value := 10;
   end Counter;

   Result : Integer;
begin
   Counter.Increment;
   Counter.Increment;
   Result := Counter.Get;
   -- Result should be 12 (10 from elaboration + 2 increments)
   if Result /= 12 then
      raise Constraint_Error;
   end if;
end Test_Elab;
