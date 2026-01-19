------------------------------------------------------------------------------
-- System Call Implementation
------------------------------------------------------------------------------

with Microkernel;

package body Syscalls is

   ----------------------------------------------------------------------------
   -- Syscall Wrappers (User Space - Thumb-2 optimized)
   ----------------------------------------------------------------------------

   function Syscall_0 (Num : Syscall_Number) return Syscall_Result is
      Result : Integer_32;
      Errno : Unsigned_32;
   begin
      Asm ("mov r7, %2" & LF &        -- r7 = syscall number
           "svc #0" & LF &             -- Supervisor call
           "mov %0, r0" & LF &         -- Return value
           "mov %1, r1",               -- Error code
           Outputs => (Integer_32'Asm_Output ("=r", Result),
                      Unsigned_32'Asm_Output ("=r", Errno)),
           Inputs => Unsigned_32'Asm_Input ("r", Unsigned_32 (Num)),
           Volatile => True,
           Clobber => "r7,r0,r1,memory");

      return (Value => Result, Error => Errno /= 0);
   end Syscall_0;

   function Syscall_1 (Num : Syscall_Number; Arg1 : Unsigned_32)
      return Syscall_Result is
      Result : Integer_32;
      Errno : Unsigned_32;
   begin
      Asm ("mov r7, %2" & LF &
           "mov r0, %3" & LF &
           "svc #0" & LF &
           "mov %0, r0" & LF &
           "mov %1, r1",
           Outputs => (Integer_32'Asm_Output ("=r", Result),
                      Unsigned_32'Asm_Output ("=r", Errno)),
           Inputs => (Unsigned_32'Asm_Input ("r", Unsigned_32 (Num)),
                     Unsigned_32'Asm_Input ("r", Arg1)),
           Volatile => True,
           Clobber => "r7,r0,r1,memory");

      return (Value => Result, Error => Errno /= 0);
   end Syscall_1;

   function Syscall_2 (Num : Syscall_Number; Arg1, Arg2 : Unsigned_32)
      return Syscall_Result is
      Result : Integer_32;
      Errno : Unsigned_32;
   begin
      Asm ("mov r7, %2" & LF &
           "mov r0, %3" & LF &
           "mov r1, %4" & LF &
           "svc #0" & LF &
           "mov %0, r0" & LF &
           "mov %1, r1",
           Outputs => (Integer_32'Asm_Output ("=r", Result),
                      Unsigned_32'Asm_Output ("=r", Errno)),
           Inputs => (Unsigned_32'Asm_Input ("r", Unsigned_32 (Num)),
                     Unsigned_32'Asm_Input ("r", Arg1),
                     Unsigned_32'Asm_Input ("r", Arg2)),
           Volatile => True,
           Clobber => "r7,r0,r1,memory");

      return (Value => Result, Error => Errno /= 0);
   end Syscall_2;

   function Syscall_3 (Num : Syscall_Number; Arg1, Arg2, Arg3 : Unsigned_32)
      return Syscall_Result is
      Result : Integer_32;
      Errno : Unsigned_32;
   begin
      Asm ("mov r7, %2" & LF &
           "mov r0, %3" & LF &
           "mov r1, %4" & LF &
           "mov r2, %5" & LF &
           "svc #0" & LF &
           "mov %0, r0" & LF &
           "mov %1, r1",
           Outputs => (Integer_32'Asm_Output ("=r", Result),
                      Unsigned_32'Asm_Output ("=r", Errno)),
           Inputs => (Unsigned_32'Asm_Input ("r", Unsigned_32 (Num)),
                     Unsigned_32'Asm_Input ("r", Arg1),
                     Unsigned_32'Asm_Input ("r", Arg2),
                     Unsigned_32'Asm_Input ("r", Arg3)),
           Volatile => True,
           Clobber => "r7,r0,r1,r2,memory");

      return (Value => Result, Error => Errno /= 0);
   end Syscall_3;

   ----------------------------------------------------------------------------
   -- SVC Handler (Kernel Space)
   ----------------------------------------------------------------------------

   procedure SVC_Handler is
      Syscall_Num : Unsigned_32;
      Arg0, Arg1, Arg2 : Unsigned_32;
   begin
      -- Extract syscall number and arguments from registers
      -- r7 = syscall number, r0-r2 = arguments
      Asm ("mov %0, r7" & LF &
           "mov %1, r0" & LF &
           "mov %2, r1" & LF &
           "mov %3, r2",
           Outputs => (Unsigned_32'Asm_Output ("=r", Syscall_Num),
                      Unsigned_32'Asm_Output ("=r", Arg0),
                      Unsigned_32'Asm_Output ("=r", Arg1),
                      Unsigned_32'Asm_Output ("=r", Arg2)),
           Volatile => True);

      -- Dispatch syscall
      case Syscall_Num is
         when SYS_EXIT =>
            Handle_Exit (Integer (Arg0));

         when SYS_WRITE =>
            Handle_Write (Integer (Arg0), Arg1, Integer (Arg2));

         when SYS_SEND =>
            Handle_Send (Arg0, Arg1);

         when SYS_RECEIVE =>
            Handle_Receive (Arg0);

         when others =>
            -- Unknown syscall - return error
            Asm ("mov r0, #-1" & LF &  -- ENOSYS
                 "mov r1, #1",          -- Error flag
                 Volatile => True,
                 Clobber => "r0,r1");
      end case;
   end SVC_Handler;

   ----------------------------------------------------------------------------
   -- Syscall Implementations
   ----------------------------------------------------------------------------

   procedure Handle_Exit (Code : Integer) is
   begin
      -- Terminate current process
      -- Set return value in r0
      Asm ("mov r0, #0" & LF &    -- Success
           "mov r1, #0",           -- No error
           Volatile => True,
           Clobber => "r0,r1");
      -- Process will be marked as terminated by scheduler
   end Handle_Exit;

   procedure Handle_Write (FD : Integer; Buf : Unsigned_32; Count : Integer) is
      Char_Ptr : System.Address := System.Storage_Elements.To_Address (
         System.Storage_Elements.Integer_Address (Buf));
   begin
      -- For now, only support stdout (fd=1)
      if FD = 1 then
         for I in 0 .. Count - 1 loop
            declare
               C : Character with Address => Char_Ptr;
            begin
               -- UART output via inline assembly
               Asm ("ldrb r0, [%0]" & LF &
                    "ldr r1, =0x09000000" & LF &  -- UART base
                    "str r0, [r1]",
                    Inputs => System.Address'Asm_Input ("r", Char_Ptr),
                    Volatile => True,
                    Clobber => "r0,r1,memory");

               Char_Ptr := Char_Ptr + 1;
            end;
         end loop;

         -- Return bytes written
         Asm ("mov r0, %0" & LF &
              "mov r1, #0",        -- No error
              Inputs => Integer'Asm_Input ("r", Count),
              Volatile => True,
              Clobber => "r0,r1");
      else
         -- Bad file descriptor
         Asm ("mov r0, #-1" & LF &  -- EBADF
              "mov r1, #1",          -- Error
              Volatile => True,
              Clobber => "r0,r1");
      end if;
   end Handle_Write;

   procedure Handle_Send (Dest : Unsigned_32; Msg_Addr : Unsigned_32) is
   begin
      -- Send IPC message via microkernel
      -- For now, just acknowledge
      Asm ("mov r0, #0" & LF &    -- Success
           "mov r1, #0",           -- No error
           Volatile => True,
           Clobber => "r0,r1");
   end Handle_Send;

   procedure Handle_Receive (Msg_Addr : Unsigned_32) is
   begin
      -- Receive IPC message via microkernel
      Asm ("mov r0, #0" & LF &
           "mov r1, #0",
           Volatile => True,
           Clobber => "r0,r1");
   end Handle_Receive;

end Syscalls;
