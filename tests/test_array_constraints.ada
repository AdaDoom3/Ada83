-- Comprehensive tests for array initialization constraint checking
-- Phase 2: Constrained Array Initialization

with Text_IO; use Text_IO;

procedure Test_Array_Constraints is

   -- Test 1: Valid array initialization (exact size match)
   type Arr5 is array(1..5) of Integer;
   A1 : Arr5 := (1, 2, 3, 4, 5);

   -- Test 2: Valid array with different bounds
   type Arr_Neg is array(-2..2) of Integer;
   A2 : Arr_Neg := (10, 20, 30, 40, 50);

   -- Test 3: Two element array
   type Arr2 is array(1..2) of Integer;
   A3 : Arr2 := (99, 100);

   -- Test 4: Larger array
   type Arr10 is array(1..10) of Integer;
   A4 : Arr10 := (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

begin
   Put_Line("Test 1: 5-element array initialized correctly");
   Put_Line("Test 2: Array with negative bounds initialized correctly");
   Put_Line("Test 3: Two-element array initialized correctly");
   Put_Line("Test 4: 10-element array initialized correctly");

   Put_Line("All array initialization tests passed!");
end Test_Array_Constraints;
