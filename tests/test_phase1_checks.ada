-- Comprehensive tests for Phase 1 runtime constraint checking
-- Tests division-by-zero, null access, array bounds, and range checks

with Text_IO; use Text_IO;

procedure Test_Phase1_Checks is

   -- Test 1: Division by zero
   procedure Test_Div_By_Zero is
      X : Integer := 42;
      Y : Integer := 0;
      Z : Integer;
   begin
      Put_Line("Test 1: Division by zero...");
      Z := X / Y;  -- Should raise CONSTRAINT_ERROR
      Put_Line("FAIL: No exception raised");
   exception
      when others =>
         Put_Line("PASS: CONSTRAINT_ERROR raised for division by zero");
   end Test_Div_By_Zero;

   -- Test 2: Modulo by zero
   procedure Test_Mod_By_Zero is
      X : Integer := 42;
      Y : Integer := 0;
      Z : Integer;
   begin
      Put_Line("Test 2: Modulo by zero...");
      Z := X mod Y;  -- Should raise CONSTRAINT_ERROR
      Put_Line("FAIL: No exception raised");
   exception
      when others =>
         Put_Line("PASS: CONSTRAINT_ERROR raised for mod by zero");
   end Test_Mod_By_Zero;

   -- Test 3: Remainder by zero
   procedure Test_Rem_By_Zero is
      X : Integer := 42;
      Y : Integer := 0;
      Z : Integer;
   begin
      Put_Line("Test 3: Remainder by zero...");
      Z := X rem Y;  -- Should raise CONSTRAINT_ERROR
      Put_Line("FAIL: No exception raised");
   exception
      when others =>
         Put_Line("PASS: CONSTRAINT_ERROR raised for rem by zero");
   end Test_Rem_By_Zero;

   -- Test 4: Null pointer dereference
   procedure Test_Null_Deref is
      type Int_Ptr is access Integer;
      P : Int_Ptr := null;
      X : Integer;
   begin
      Put_Line("Test 4: Null pointer dereference...");
      X := P.all;  -- Should raise CONSTRAINT_ERROR
      Put_Line("FAIL: No exception raised");
   exception
      when others =>
         Put_Line("PASS: CONSTRAINT_ERROR raised for null dereference");
   end Test_Null_Deref;

   -- Test 5: Array bounds checking (out of bounds)
   procedure Test_Array_Bounds is
      type Arr is array(1..10) of Integer;
      A : Arr;
      X : Integer;
   begin
      Put_Line("Test 5: Array bounds check...");
      X := A(15);  -- Should raise CONSTRAINT_ERROR
      Put_Line("FAIL: No exception raised");
   exception
      when others =>
         Put_Line("PASS: CONSTRAINT_ERROR raised for array bounds");
   end Test_Array_Bounds;

   -- Test 6: Range constraint checking
   procedure Test_Range_Check is
      subtype Small is Integer range 1..10;
      X : Small;
   begin
      Put_Line("Test 6: Range constraint check...");
      X := 100;  -- Should raise CONSTRAINT_ERROR
      Put_Line("FAIL: No exception raised");
   exception
      when others =>
         Put_Line("PASS: CONSTRAINT_ERROR raised for range violation");
   end Test_Range_Check;

   -- Test 7: Valid division (should work)
   procedure Test_Valid_Division is
      X : Integer := 42;
      Y : Integer := 7;
      Z : Integer;
   begin
      Put_Line("Test 7: Valid division...");
      Z := X / Y;
      if Z = 6 then
         Put_Line("PASS: Valid division works correctly");
      else
         Put_Line("FAIL: Division result incorrect");
      end if;
   end Test_Valid_Division;

   -- Test 8: Valid pointer dereference (should work)
   procedure Test_Valid_Deref is
      type Int_Ptr is access Integer;
      P : Int_Ptr;
      X : Integer;
   begin
      Put_Line("Test 8: Valid pointer dereference...");
      P := new Integer;
      P.all := 123;
      X := P.all;
      if X = 123 then
         Put_Line("PASS: Valid dereference works correctly");
      else
         Put_Line("FAIL: Dereference result incorrect");
      end if;
   end Test_Valid_Deref;

begin
   Put_Line("=== Phase 1 Runtime Constraint Checking Tests ===");
   Put_Line("");

   Test_Div_By_Zero;
   Test_Mod_By_Zero;
   Test_Rem_By_Zero;
   Test_Null_Deref;
   Test_Array_Bounds;
   Test_Range_Check;
   Test_Valid_Division;
   Test_Valid_Deref;

   Put_Line("");
   Put_Line("=== All Phase 1 tests complete ===");
end Test_Phase1_Checks;
