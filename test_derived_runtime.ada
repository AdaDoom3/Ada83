-- Test derived type constraints at runtime
procedure Test_Derived_Runtime is

   type Small_Int is range 1..10;
   type Tiny_Int is new Small_Int;

   X : Tiny_Int := 5;   -- Valid
   Y : Tiny_Int;

begin
   Y := 15;  -- Should raise CONSTRAINT_ERROR at runtime
end Test_Derived_Runtime;
