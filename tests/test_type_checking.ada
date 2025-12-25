-- Test type mismatch detection (Phase 3)
-- This file should NOT compile due to type mismatches

procedure Test_Type_Checking is
   X : Integer := 5;
   Y : Boolean;
begin
   -- Should error: assigning Integer to Boolean
   Y := X;
end Test_Type_Checking;
