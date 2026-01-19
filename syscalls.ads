------------------------------------------------------------------------------
-- System Call Interface for Ada2012 Minix
-- Handles SVC (Supervisor Call) for user→kernel transitions
------------------------------------------------------------------------------

with System;
with System.Machine_Code; use System.Machine_Code;
with Interfaces; use Interfaces;

package Syscalls with
   SPARK_Mode => Off
is
   -- System call numbers (MINIX-compatible)
   SYS_EXIT     : constant := 1;
   SYS_FORK     : constant := 2;
   SYS_READ     : constant := 3;
   SYS_WRITE    : constant := 4;
   SYS_OPEN     : constant := 5;
   SYS_CLOSE    : constant := 6;
   SYS_WAIT     : constant := 7;
   SYS_EXEC     : constant := 11;
   SYS_KILL     : constant := 37;
   SYS_SEND     : constant := 100;  -- IPC send
   SYS_RECEIVE  : constant := 101;  -- IPC receive

   type Syscall_Number is range 1 .. 101;

   -- System call result
   type Syscall_Result is record
      Value : Integer_32;
      Error : Boolean;
   end record;

   -- Execute system call (inline assembly)
   function Syscall_0 (Num : Syscall_Number) return Syscall_Result
      with Inline_Always;

   function Syscall_1 (Num : Syscall_Number; Arg1 : Unsigned_32)
      return Syscall_Result with Inline_Always;

   function Syscall_2 (Num : Syscall_Number; Arg1, Arg2 : Unsigned_32)
      return Syscall_Result with Inline_Always;

   function Syscall_3 (Num : Syscall_Number; Arg1, Arg2, Arg3 : Unsigned_32)
      return Syscall_Result with Inline_Always;

   -- SVC handler (called from exception vector)
   procedure SVC_Handler with Export, Convention => C, External_Name => "svc_handler";

private
   -- Syscall handler table
   type Syscall_Handler is access procedure (Args : System.Address);
   Syscall_Table : array (Syscall_Number) of Syscall_Handler;

end Syscalls;
