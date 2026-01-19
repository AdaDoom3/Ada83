------------------------------------------------------------------------------
-- Init Process (PID 1) - MINIX Style
-- First user space process, spawns servers and shell
------------------------------------------------------------------------------

with System;
with System.Machine_Code; use System.Machine_Code;
with Interfaces; use Interfaces;

procedure Init is

   ----------------------------------------------------------------------------
   -- UART Output
   ----------------------------------------------------------------------------

   procedure Put_Char (C : Character) with Inline_Always is
   begin
      Asm ("mov r0, %0" & LF &
           "ldr r1, =0x09000000" & LF &
           "str r0, [r1]",
           Inputs => Character'Asm_Input ("r", C),
           Volatile => True,
           Clobber => "r0,r1,memory");
   end Put_Char;

   procedure Put_String (S : String) is
   begin
      for C of S loop
         Put_Char (C);
      end loop;
   end Put_String;

   procedure Put_Line (S : String) is
   begin
      Put_String (S);
      Put_Char (ASCII.LF);
   end Put_Line;

   ----------------------------------------------------------------------------
   -- Banner
   ----------------------------------------------------------------------------

   procedure Print_Banner is
   begin
      Put_Line ("");
      Put_Line ("╔════════════════════════════════════════════════════════════╗");
      Put_Line ("║                                                            ║");
      Put_Line ("║          Ada2012 ARM MINIX Microkernel v1.0                ║");
      Put_Line ("║                                                            ║");
      Put_Line ("║   Architecture:  ARMv7-A Cortex-A15 SMP (4 cores)         ║");
      Put_Line ("║   Features:      Thumb-2, NEON, Priority IPC               ║");
      Put_Line ("║   Language:      Pure Ada2012 + inline assembly            ║");
      Put_Line ("║   Profile:       Ravenscar real-time                       ║");
      Put_Line ("║                                                            ║");
      Put_Line ("╚════════════════════════════════════════════════════════════╝");
      Put_Line ("");
   end Print_Banner;

   ----------------------------------------------------------------------------
   -- Server Spawning (simulated - real version would use fork/exec)
   ----------------------------------------------------------------------------

   procedure Spawn_Servers is
   begin
      Put_Line ("[Init] Starting system servers...");
      Put_Line ("");

      -- In real system, we'd fork and exec each server
      -- For now, just announce them

      Put_Line ("[Init] Starting Process Manager (PM)...");
      -- fork(); exec("/sbin/pm");

      Put_Line ("[Init] Starting Virtual File System (VFS)...");
      -- fork(); exec("/sbin/vfs");

      Put_Line ("[Init] Starting Reincarnation Server (RS)...");
      -- fork(); exec("/sbin/rs");

      Put_Line ("");
      Put_Line ("[Init] All servers started successfully");
      Put_Line ("");
   end Spawn_Servers;

   ----------------------------------------------------------------------------
   -- System Information
   ----------------------------------------------------------------------------

   procedure Show_System_Info is
      CPU_ID : Unsigned_32;
   begin
      Put_Line ("System Information:");
      Put_Line ("━━━━━━━━━━━━━━━━━━");

      -- Read CPU ID
      Asm ("mrc p15, 0, %0, c0, c0, 5" & LF &
           "and %0, %0, #3",
           Outputs => Unsigned_32'Asm_Output ("=r", CPU_ID),
           Volatile => True);

      Put_String ("  Current CPU:          ");
      Put_Char (Character'Val (Character'Pos ('0') + Integer (CPU_ID)));
      Put_Line ("");

      Put_Line ("  Total CPUs:           4");
      Put_Line ("  CPU Type:             Cortex-A15");
      Put_Line ("  Instruction Set:      Thumb-2 mixed mode");
      Put_Line ("  SIMD:                 NEON (128-bit)");
      Put_Line ("  IPC Priorities:       8 levels (0-7)");
      Put_Line ("  Max Processes:        64");
      Put_Line ("  Message Queue:        256 entries");
      Put_Line ("");
   end Show_System_Info;

   ----------------------------------------------------------------------------
   -- Simple Shell Commands
   ----------------------------------------------------------------------------

   procedure Handle_Command (Cmd : String) is
   begin
      if Cmd = "help" then
         Put_Line ("Available commands:");
         Put_Line ("  help      - Show this help");
         Put_Line ("  info      - Display system information");
         Put_Line ("  ps        - List processes");
         Put_Line ("  reboot    - Reboot system");
         Put_Line ("  halt      - Halt system");

      elsif Cmd = "info" then
         Show_System_Info;

      elsif Cmd = "ps" then
         Put_Line ("PID   STATE      COMMAND");
         Put_Line ("━━━━━━━━━━━━━━━━━━━━━━━━");
         Put_Line ("  1   Running    init");
         Put_Line ("  2   Sleeping   pm");
         Put_Line ("  3   Sleeping   vfs");
         Put_Line ("  4   Running    shell");

      elsif Cmd = "reboot" then
         Put_Line ("[Init] Rebooting system...");
         -- System reboot via watchdog timer
         Asm ("b .", Volatile => True);  -- Infinite loop for now

      elsif Cmd = "halt" then
         Put_Line ("[Init] System halted.");
         Asm ("wfi", Volatile => True);

      else
         Put_String ("Unknown command: ");
         Put_Line (Cmd);
         Put_Line ("Type 'help' for available commands");
      end if;

      Put_Line ("");
   end Handle_Command;

   ----------------------------------------------------------------------------
   -- Mini Shell
   ----------------------------------------------------------------------------

   procedure Run_Shell is
      Input_Buffer : String (1 .. 64);
      Input_Pos : Natural := 0;
      C : Character;
   begin
      Put_Line ("╔════════════════════════════════════════════════════════════╗");
      Put_Line ("║              Ada2012 MINIX Shell v1.0                      ║");
      Put_Line ("╚════════════════════════════════════════════════════════════╝");
      Put_Line ("");
      Put_Line ("Type 'help' for available commands");
      Put_Line ("");

      loop
         Put_String ("# ");

         -- Read command (simplified - no actual UART input in simulator)
         -- In real system, this would read from UART
         -- For now, just execute a demo sequence

         -- Simulate user typing "info"
         Handle_Command ("info");

         -- Simulate user typing "ps"
         Handle_Command ("ps");

         -- Simulate user typing "help"
         Handle_Command ("help");

         -- Now just loop
         Put_Line ("[Init] Demo complete. Entering idle loop...");
         Put_Line ("[Init] In real system, shell would wait for UART input");
         Put_Line ("");

         loop
            Asm ("wfi", Volatile => True);
         end loop;
      end loop;
   end Run_Shell;

   ----------------------------------------------------------------------------
   -- Main
   ----------------------------------------------------------------------------

begin
   -- Print startup banner
   Print_Banner;

   -- Show system information
   Show_System_Info;

   -- Spawn system servers
   Spawn_Servers;

   -- Run shell
   Run_Shell;

end Init;
