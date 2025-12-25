-- Comprehensive derived type constraint testing
procedure Test_Derived_Comprehensive is
   
   -- Base constrained type
   type Small_Int is range 1..10;
   
   -- Derived type inherits constraint
   type Tiny_Int is new Small_Int;
   
   -- Subtype also has constraint
   subtype Small_Sub is Integer range 1..10;
   
   -- Valid declarations
   X1 : Tiny_Int := 5;   -- OK: within 1..10
   X2 : Small_Int := 1;  -- OK: at lower bound
   X3 : Tiny_Int := 10;  -- OK: at upper bound
   X4 : Small_Sub := 7;  -- OK: subtype constraint
   
   -- Variables for runtime assignment tests  
   Y1 : Tiny_Int;
   Y2 : Small_Int;
   Y3 : Small_Sub;

begin
   -- Valid assignments
   Y1 := 3;   -- OK
   Y2 := 8;   -- OK  
   Y3 := 5;   -- OK
   
   -- These would fail at runtime if enabled:
   -- Y1 := 15;  -- Would raise CONSTRAINT_ERROR: 15 > 10
   -- Y2 := 0;   -- Would raise CONSTRAINT_ERROR: 0 < 1
   -- Y3 := 20;  -- Would raise CONSTRAINT_ERROR: 20 > 10
   
end Test_Derived_Comprehensive;
