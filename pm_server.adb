------------------------------------------------------------------------------
-- Process Manager (PM) Server - MINIX Style
-- Handles: fork, exec, exit, wait, signals
------------------------------------------------------------------------------

with System;
with System.Machine_Code; use System.Machine_Code;
with Interfaces; use Interfaces;
with Syscalls; use Syscalls;

procedure PM_Server is

   MAX_PROCS : constant := 64;

   type Process_State is (Free, Runnable, Sleeping, Zombie);

   type Process_Slot is record
      PID          : Integer := 0;
      Parent_PID   : Integer := 0;
      State        : Process_State := Free;
      Exit_Status  : Integer := 0;
      Stack_Ptr    : Unsigned_32 := 0;
      Program_Counter : Unsigned_32 := 0;
   end record;

   Process_Table : array (0 .. MAX_PROCS - 1) of Process_Slot;
   Next_PID : Integer := 1;

   ----------------------------------------------------------------------------
   -- UART Output
   ----------------------------------------------------------------------------

   procedure Put_String (S : String) is
   begin
      for C of S loop
         Asm ("mov r0, %0" & LF &
              "ldr r1, =0x09000000" & LF &
              "strb r0, [r1]",
              Inputs => Character'Asm_Input ("r", C),
              Volatile => True,
              Clobber => "r0,r1,memory");
      end loop;
   end Put_String;

   ----------------------------------------------------------------------------
   -- Process Operations
   ----------------------------------------------------------------------------

   function Allocate_Process return Integer is
   begin
      for I in Process_Table'Range loop
         if Process_Table (I).State = Free then
            Process_Table (I).PID := Next_PID;
            Next_PID := Next_PID + 1;
            Process_Table (I).State := Runnable;
            return I;
         end if;
      end loop;
      return -1;  -- No free slots
   end Allocate_Process;

   procedure Do_Fork (Parent_Slot : Integer) is
      Child_Slot : Integer;
   begin
      Child_Slot := Allocate_Process;

      if Child_Slot >= 0 then
         -- Copy parent context to child
         Process_Table (Child_Slot).Parent_PID :=
            Process_Table (Parent_Slot).PID;
         Process_Table (Child_Slot).Stack_Ptr :=
            Process_Table (Parent_Slot).Stack_Ptr;
         Process_Table (Child_Slot).Program_Counter :=
            Process_Table (Parent_Slot).Program_Counter;

         Put_String ("[PM] Fork: parent=" &
            Integer'Image (Process_Table (Parent_Slot).PID) &
            " child=" &
            Integer'Image (Process_Table (Child_Slot).PID) & ASCII.LF);
      else
         Put_String ("[PM] Fork failed: no free slots" & ASCII.LF);
      end if;
   end Do_Fork;

   procedure Do_Exit (Slot : Integer; Status : Integer) is
   begin
      Process_Table (Slot).State := Zombie;
      Process_Table (Slot).Exit_Status := Status;

      Put_String ("[PM] Process " &
         Integer'Image (Process_Table (Slot).PID) &
         " exited with status " &
         Integer'Image (Status) & ASCII.LF);

      -- Wake up parent if waiting
      for I in Process_Table'Range loop
         if Process_Table (I).PID = Process_Table (Slot).Parent_PID and then
            Process_Table (I).State = Sleeping
         then
            Process_Table (I).State := Runnable;
         end if;
      end loop;
   end Do_Exit;

   procedure Do_Wait (Parent_Slot : Integer) is
   begin
      -- Check for zombie children
      for I in Process_Table'Range loop
         if Process_Table (I).Parent_PID =
            Process_Table (Parent_Slot).PID and then
            Process_Table (I).State = Zombie
         then
            -- Reap zombie child
            Put_String ("[PM] Reaping zombie PID=" &
               Integer'Image (Process_Table (I).PID) & ASCII.LF);

            Process_Table (I).State := Free;
            return;
         end if;
      end loop;

      -- No zombies - sleep until child exits
      Process_Table (Parent_Slot).State := Sleeping;
   end Do_Wait;

   ----------------------------------------------------------------------------
   -- IPC Message Processing
   ----------------------------------------------------------------------------

   type PM_Message is record
      Msg_Type : Unsigned_32;  -- fork=1, exit=2, wait=3
      PID      : Integer;
      Arg1     : Integer;
      Arg2     : Integer;
   end record with Pack, Size => 128;

   procedure Process_Message (Msg : PM_Message) is
   begin
      case Msg.Msg_Type is
         when 1 =>  -- Fork
            for I in Process_Table'Range loop
               if Process_Table (I).PID = Msg.PID then
                  Do_Fork (I);
                  exit;
               end if;
            end loop;

         when 2 =>  -- Exit
            for I in Process_Table'Range loop
               if Process_Table (I).PID = Msg.PID then
                  Do_Exit (I, Msg.Arg1);
                  exit;
               end if;
            end loop;

         when 3 =>  -- Wait
            for I in Process_Table'Range loop
               if Process_Table (I).PID = Msg.PID then
                  Do_Wait (I);
                  exit;
               end if;
            end loop;

         when others =>
            Put_String ("[PM] Unknown message type" & ASCII.LF);
      end case;
   end Process_Message;

   ----------------------------------------------------------------------------
   -- Main Loop
   ----------------------------------------------------------------------------

   Msg : PM_Message;
   Init_Slot : Integer;

begin
   Put_String (ASCII.LF);
   Put_String ("╔══════════════════════════════════════════════════╗" & ASCII.LF);
   Put_String ("║    Process Manager (PM) Server Starting         ║" & ASCII.LF);
   Put_String ("╚══════════════════════════════════════════════════╝" & ASCII.LF);
   Put_String (ASCII.LF);

   -- Initialize process table
   for I in Process_Table'Range loop
      Process_Table (I).State := Free;
   end loop;

   -- Create init process (PID 1)
   Init_Slot := Allocate_Process;
   if Init_Slot >= 0 then
      Process_Table (Init_Slot).Parent_PID := 0;  -- No parent
      Put_String ("[PM] Init process created (PID=1)" & ASCII.LF);
   end if;

   Put_String ("[PM] Entering main loop..." & ASCII.LF & ASCII.LF);

   -- Main server loop
   loop
      -- Receive IPC message (blocking)
      -- In real system, this would be Syscall_Receive
      -- For now, just yield
      Asm ("wfe", Volatile => True);

      -- Process message
      -- Process_Message (Msg);
   end loop;

end PM_Server;
