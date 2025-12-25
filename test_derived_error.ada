-- Test derived type constraint inheritance
procedure Test_Derived_Constraints is

   -- Base type with range constraint
   type Small_Int is range 1..10;

   -- Derived type should inherit constraint
   type Tiny_Int is new Small_Int;

   -- This should fail: value outside inherited constraint
   X : Tiny_Int := 15;  -- Should error: outside 1..10

begin
   null;
end Test_Derived_Constraints;
