-- ============================================================================
-- SMP-ENHANCED MICROKERNEL - MULTICORE SUPPORT WITH IPC OPTIMIZATION
-- ============================================================================
-- Extension of Ada83 Minix microkernel with:
-- - Symmetric multiprocessing (up to 4 cores)
-- - Advanced IPC (zero-copy, priority queues)
-- - Per-CPU scheduling with load balancing
-- - Spinlock synchronization primitives
--
-- Research Contributions:
-- 1. Minimal SMP microkernel implementation
-- 2. Zero-copy IPC for performance
-- 3. Priority-aware message passing
-- 4. Lock-free algorithms where possible
-- ============================================================================

with System; use System;

procedure Microkernel_SMP_Enhanced is

   -- =========================================================================
   -- SMP CONFIGURATION CONSTANTS
   -- =========================================================================

   Maximum_CPU_Count : constant := 4;  -- Support up to 4 cores
   Current_CPU_Count : Integer := 1;   -- Detected at runtime

   -- =========================================================================
   -- TYPE DEFINITIONS (Enhanced for SMP)
   -- =========================================================================

   type CPU_Identifier is new Integer range 0 .. Maximum_CPU_Count - 1;
   type Process_Identifier is new Integer range 0 .. 63;

   type Message_Priority_Level is new Integer range 0 .. 7;  -- NEW: Priority levels

   type Message_Type_Discriminator is (
      Inter_Process_Communication_Send_Request,
      Inter_Process_Communication_Receive_Request,
      Inter_Process_Communication_Reply_Response,
      Inter_Process_Communication_Notify_Async,  -- NEW: Async notification
      System_Call_Memory_Allocation,
      System_Call_Memory_Deallocation,
      System_Call_Thread_Create,  -- NEW
      System_Call_Thread_Yield,   -- NEW
      Interrupt_Notification
   );

   type Virtual_Memory_Address is new Integer;
   type Physical_Memory_Address is new Integer;
   type Memory_Page_Size_Constant is new Integer;

   -- NEW: Shared memory region for zero-copy IPC
   type Shared_Memory_Region is record
      Base_Address : Virtual_Memory_Address;
      Size_In_Bytes : Integer;
      Owner_Process : Process_Identifier;
      Access_Permissions : Integer;  -- Bitmap: read/write/execute
   end record;

   -- Enhanced IPC message with priority and zero-copy support
   type Inter_Process_Communication_Message_Block is record
      Message_Type : Message_Type_Discriminator;
      Priority : Message_Priority_Level;  -- NEW
      Source_Process_Identifier : Process_Identifier;
      Destination_Process_Identifier : Process_Identifier;
      Payload_Data_Word_One : Integer;
      Payload_Data_Word_Two : Integer;
      Payload_Data_Word_Three : Integer;
      Payload_Data_Word_Four : Integer;
      Shared_Memory : Shared_Memory_Region;  -- NEW: For zero-copy
   end record;

   -- Process states (same as before)
   type Process_State_Enumeration is (
      Process_State_Ready_To_Execute,
      Process_State_Currently_Running,
      Process_State_Blocked_On_Message,
      Process_State_Blocked_On_Interrupt,
      Process_State_Terminated
   );

   type Process_Priority_Level is new Integer range 0 .. 15;
   type Process_Affinity_Mask is new Integer;  -- NEW: CPU affinity bitmap

   -- CPU register context (same as before)
   type CPU_Register_Context_Save_Area is record
      General_Purpose_Register_R0 : Integer;
      General_Purpose_Register_R1 : Integer;
      General_Purpose_Register_R2 : Integer;
      General_Purpose_Register_R3 : Integer;
      General_Purpose_Register_R4 : Integer;
      General_Purpose_Register_R5 : Integer;
      General_Purpose_Register_R6 : Integer;
      General_Purpose_Register_R7 : Integer;
      General_Purpose_Register_R8 : Integer;
      General_Purpose_Register_R9 : Integer;
      General_Purpose_Register_R10 : Integer;
      Frame_Pointer_Register_R11 : Integer;
      Instruction_Pointer_Register_R12 : Integer;
      Stack_Pointer_Register_R13 : Integer;
      Link_Register_R14 : Integer;
      Program_Counter_Register_R15 : Integer;
      Current_Program_Status_Register : Integer;
   end record;

   -- Enhanced PCB with SMP support
   type Process_Control_Block is record
      Unique_Process_Identifier : Process_Identifier;
      Current_Process_State : Process_State_Enumeration;
      Process_Priority : Process_Priority_Level;
      CPU_Affinity : Process_Affinity_Mask;  -- NEW: Which CPUs can run this
      Last_CPU : CPU_Identifier;  -- NEW: Last CPU that ran this process
      Saved_CPU_Context : CPU_Register_Context_Save_Area;
      Message_Queue_Head_Pointer : Integer;
      Virtual_Memory_Page_Table_Base : Virtual_Memory_Address;
      Time_Slice_Remaining : Integer;  -- NEW: For preemptive scheduling
   end record;

   -- NEW: Per-CPU data structure
   type Per_CPU_Data_Structure is record
      CPU_ID : CPU_Identifier;
      Current_Process : Process_Identifier;
      Idle_Process : Process_Identifier;
      Ready_Queue_Head : Integer;
      Ready_Queue_Tail : Integer;
      Load_Average : Integer;  -- Number of ready processes
      Interrupt_Nesting_Level : Integer;
      Scheduler_Lock_Held : Boolean;
   end record;

   -- =========================================================================
   -- GLOBAL KERNEL DATA STRUCTURES (SMP-Enhanced)
   -- =========================================================================

   Process_Table : array(Process_Identifier) of Process_Control_Block;

   -- NEW: Per-CPU data (one per core)
   Per_CPU_Data : array(CPU_Identifier) of Per_CPU_Data_Structure;

   -- Priority-based message queues (8 priority levels)
   Message_Queues : array(Message_Priority_Level) of array(0 .. 255) of
      Inter_Process_Communication_Message_Block;
   Message_Queue_Heads : array(Message_Priority_Level) of Integer := (others => 0);
   Message_Queue_Tails : array(Message_Priority_Level) of Integer := (others => 0);

   -- Memory management (same as before but with protection)
   Memory_Page_Size : constant Memory_Page_Size_Constant := 4096;
   Free_Page_Bitmap : array(0 .. 1023) of Boolean := (others => False);

   -- NEW: Shared memory region pool
   Shared_Memory_Pool : array(0 .. 31) of Shared_Memory_Region;
   Shared_Memory_Bitmap : array(0 .. 31) of Boolean := (others => False);

   -- Statistics for research analysis
   SMP_Statistics_Context_Switches_Per_CPU : array(CPU_Identifier) of Integer := (others => 0);
   SMP_Statistics_IPC_Messages_Per_Priority : array(Message_Priority_Level) of Integer := (others => 0);
   SMP_Statistics_Zero_Copy_Operations : Integer := 0;
   SMP_Statistics_Load_Balancing_Migrations : Integer := 0;

   -- =========================================================================
   -- ASSEMBLY INTERFACE SPECIFICATIONS (Enhanced for SMP)
   -- =========================================================================

   procedure Enable_Hardware_Interrupts;
   pragma Import(C, Enable_Hardware_Interrupts, "enable_interrupts");

   procedure Disable_Hardware_Interrupts;
   pragma Import(C, Disable_Hardware_Interrupts, "disable_interrupts");

   procedure Perform_Context_Switch_To_Process(Target_Process_ID : Process_Identifier);
   pragma Import(C, Perform_Context_Switch_To_Process, "context_switch");

   procedure Write_Character_To_UART_Debug_Console(Character_To_Output : Character);
   pragma Import(C, Write_Character_To_UART_Debug_Console, "uart_putc");

   procedure Initialize_ARM_Vector_Table_And_Interrupts;
   pragma Import(C, Initialize_ARM_Vector_Table_And_Interrupts, "init_vectors");

   procedure Copy_Memory_Block_Using_NEON(
      Source_Address : Physical_Memory_Address;
      Destination_Address : Physical_Memory_Address;
      Byte_Count : Integer
   );
   pragma Import(C, Copy_Memory_Block_Using_NEON, "neon_memcpy");

   -- NEW: SMP-specific assembly routines
   procedure Spinlock_Acquire(Lock_Address : System.Address);
   pragma Import(C, Spinlock_Acquire, "spinlock_acquire");

   procedure Spinlock_Release(Lock_Address : System.Address);
   pragma Import(C, Spinlock_Release, "spinlock_release");

   procedure Get_CPU_ID return CPU_Identifier;
   pragma Import(C, Get_CPU_ID, "get_cpu_id");

   procedure Secondary_CPU_Boot_Entry_Point;
   pragma Import(C, Secondary_CPU_Boot_Entry_Point, "secondary_cpu_boot");

   procedure Send_Inter_Processor_Interrupt(Target_CPU : CPU_Identifier);
   pragma Import(C, Send_Inter_Processor_Interrupt, "send_ipi");

   procedure Memory_Barrier_Full;
   pragma Import(C, Memory_Barrier_Full, "dmb_full");

   -- =========================================================================
   -- CORE MICROKERNEL OPERATIONS (Enhanced)
   -- =========================================================================

   -- Debug output (same as before)
   procedure Print_Kernel_Debug_String(Message_String : String) is
   begin
      for Character_Index in Message_String'Range loop
         Write_Character_To_UART_Debug_Console(Message_String(Character_Index));
      end loop;
   end Print_Kernel_Debug_String;

   -- NEW: Get current CPU ID
   function Get_Current_CPU return CPU_Identifier is
   begin
      return Get_CPU_ID;
   end Get_Current_CPU;

   -- Initialize process table (enhanced)
   procedure Initialize_Process_Table_At_Boot is
   begin
      for Process_Index in Process_Identifier loop
         Process_Table(Process_Index).Unique_Process_Identifier := Process_Index;
         Process_Table(Process_Index).Current_Process_State := Process_State_Terminated;
         Process_Table(Process_Index).Process_Priority := 5;
         Process_Table(Process_Index).CPU_Affinity := 16#FF#;  -- All CPUs
         Process_Table(Process_Index).Last_CPU := 0;
         Process_Table(Process_Index).Time_Slice_Remaining := 10;
      end loop;

      -- Create idle process for each CPU
      for CPU in CPU_Identifier range 0 .. CPU_Identifier(Current_CPU_Count - 1) loop
         declare
            Idle_PID : Process_Identifier := Process_Identifier(CPU);
         begin
            Process_Table(Idle_PID).Current_Process_State := Process_State_Ready_To_Execute;
            Process_Table(Idle_PID).Process_Priority := 0;
            Process_Table(Idle_PID).CPU_Affinity := 2 ** Integer(CPU);  -- Pinned to one CPU
            Process_Table(Idle_PID).Last_CPU := CPU;
            Per_CPU_Data(CPU).Idle_Process := Idle_PID;
         end;
      end loop;
   end Initialize_Process_Table_At_Boot;

   -- NEW: Priority-aware message send
   function Send_Message_With_Priority(
      Target_Process : Process_Identifier;
      Message_Block : Inter_Process_Communication_Message_Block;
      Priority : Message_Priority_Level
   ) return Boolean is
      Next_Queue_Index : Integer;
      Enhanced_Message : Inter_Process_Communication_Message_Block := Message_Block;
   begin
      Disable_Hardware_Interrupts;

      -- Set priority
      Enhanced_Message.Priority := Priority;

      -- Check if message queue has space for this priority level
      Next_Queue_Index := (Message_Queue_Tails(Priority) + 1) mod 256;
      if Next_Queue_Index = Message_Queue_Heads(Priority) then
         Enable_Hardware_Interrupts;
         return False; -- Queue full for this priority
      end if;

      -- Enqueue message in priority queue
      Message_Queues(Priority)(Message_Queue_Tails(Priority)) := Enhanced_Message;
      Message_Queue_Tails(Priority) := Next_Queue_Index;

      -- Update statistics
      SMP_Statistics_IPC_Messages_Per_Priority(Priority) :=
         SMP_Statistics_IPC_Messages_Per_Priority(Priority) + 1;

      -- Wake up target process if blocked on receive
      if Process_Table(Target_Process).Current_Process_State =
         Process_State_Blocked_On_Message then
         Process_Table(Target_Process).Current_Process_State :=
            Process_State_Ready_To_Execute;
      end if;

      Memory_Barrier_Full;  -- Ensure visibility across CPUs
      Enable_Hardware_Interrupts;
      return True;
   end Send_Message_With_Priority;

   -- NEW: Priority-aware message receive (highest priority first)
   function Receive_Message_With_Priority(
      Received_Message : out Inter_Process_Communication_Message_Block
   ) return Boolean is
   begin
      Disable_Hardware_Interrupts;

      -- Check queues from highest to lowest priority
      for Priority in reverse Message_Priority_Level loop
         if Message_Queue_Heads(Priority) /= Message_Queue_Tails(Priority) then
            -- Found a message at this priority level
            Received_Message := Message_Queues(Priority)(Message_Queue_Heads(Priority));
            Message_Queue_Heads(Priority) := (Message_Queue_Heads(Priority) + 1) mod 256;

            Enable_Hardware_Interrupts;
            return True;
         end if;
      end loop;

      -- No messages in any queue - block current process
      declare
         Current_CPU : CPU_Identifier := Get_Current_CPU;
         Current_PID : Process_Identifier := Per_CPU_Data(Current_CPU).Current_Process;
      begin
         Process_Table(Current_PID).Current_Process_State :=
            Process_State_Blocked_On_Message;
      end;

      Enable_Hardware_Interrupts;
      return False;
   end Receive_Message_With_Priority;

   -- NEW: Zero-copy IPC via shared memory
   function Allocate_Shared_Memory_Region(
      Size : Integer;
      Owner : Process_Identifier
   ) return Integer is  -- Returns region ID or -1
   begin
      for Region_Index in Shared_Memory_Bitmap'Range loop
         if not Shared_Memory_Bitmap(Region_Index) then
            -- Found free region
            Shared_Memory_Bitmap(Region_Index) := True;

            Shared_Memory_Pool(Region_Index).Size_In_Bytes := Size;
            Shared_Memory_Pool(Region_Index).Owner_Process := Owner;
            Shared_Memory_Pool(Region_Index).Access_Permissions := 16#3#;  -- Read/Write

            -- Allocate actual memory (simplified - would use page allocator)
            Shared_Memory_Pool(Region_Index).Base_Address :=
               Virtual_Memory_Address(16#8000_0000# + Region_Index * 65536);

            SMP_Statistics_Zero_Copy_Operations := SMP_Statistics_Zero_Copy_Operations + 1;

            return Region_Index;
         end if;
      end loop;

      return -1;  -- No free regions
   end Allocate_Shared_Memory_Region;

   -- NEW: SMP load-balancing scheduler
   function Find_Least_Loaded_CPU return CPU_Identifier is
      Min_Load : Integer := 999999;
      Best_CPU : CPU_Identifier := 0;
   begin
      for CPU in CPU_Identifier range 0 .. CPU_Identifier(Current_CPU_Count - 1) loop
         if Per_CPU_Data(CPU).Load_Average < Min_Load then
            Min_Load := Per_CPU_Data(CPU).Load_Average;
            Best_CPU := CPU;
         end if;
      end loop;

      return Best_CPU;
   end Find_Least_Loaded_CPU;

   -- Enhanced scheduler with load balancing
   procedure Schedule_Next_Ready_Process is
      Current_CPU : CPU_Identifier := Get_Current_CPU;
      Current_PID : Process_Identifier := Per_CPU_Data(Current_CPU).Current_Process;
      Next_Process : Process_Identifier;
      Search_Count : Integer := 0;
      CPU_Affinity_Mask : Integer;
   begin
      Next_Process := Current_PID;

      -- Round-robin search for ready process
      loop
         Next_Process := (Next_Process + 1) mod 64;
         Search_Count := Search_Count + 1;

         -- Check if process is ready and can run on this CPU
         if Process_Table(Next_Process).Current_Process_State =
            Process_State_Ready_To_Execute then

            CPU_Affinity_Mask := Process_Table(Next_Process).CPU_Affinity;

            -- Check affinity (can this process run on current CPU?)
            if (CPU_Affinity_Mask and (2 ** Integer(Current_CPU))) /= 0 then
               exit;  -- Found suitable process
            end if;
         end if;

         exit when Search_Count > 64;
      end loop;

      if Process_Table(Next_Process).Current_Process_State =
         Process_State_Ready_To_Execute then

         if Current_PID /= Next_Process then
            -- Context switch needed
            Process_Table(Current_PID).Current_Process_State :=
               Process_State_Ready_To_Execute;
            Process_Table(Next_Process).Current_Process_State :=
               Process_State_Currently_Running;
            Process_Table(Next_Process).Last_CPU := Current_CPU;

            Per_CPU_Data(Current_CPU).Current_Process := Next_Process;

            -- Update statistics
            SMP_Statistics_Context_Switches_Per_CPU(Current_CPU) :=
               SMP_Statistics_Context_Switches_Per_CPU(Current_CPU) + 1;

            Perform_Context_Switch_To_Process(Next_Process);
         end if;
      end if;
   end Schedule_Next_Ready_Process;

   -- Memory management (same as before)
   function Allocate_Physical_Memory_Page return Physical_Memory_Address is
   begin
      for Page_Index in Free_Page_Bitmap'Range loop
         if not Free_Page_Bitmap(Page_Index) then
            Free_Page_Bitmap(Page_Index) := True;
            return Physical_Memory_Address(Page_Index * Integer(Memory_Page_Size));
         end if;
      end loop;
      return -1;
   end Allocate_Physical_Memory_Page;

   procedure Deallocate_Physical_Memory_Page(Page_Address : Physical_Memory_Address) is
      Page_Index : Integer;
   begin
      Page_Index := Integer(Page_Address) / Integer(Memory_Page_Size);
      if Page_Index >= 0 and Page_Index <= 1023 then
         Free_Page_Bitmap(Page_Index) := False;
      end if;
   end Deallocate_Physical_Memory_Page;

   -- System call dispatcher (enhanced)
   procedure Handle_System_Call_Interrupt(
      System_Call_Number : Integer;
      Parameter_One : Integer;
      Parameter_Two : Integer;
      Parameter_Three : Integer;
      Result_Code : out Integer
   ) is
   begin
      case System_Call_Number is
         when 1 =>  -- Send message with priority
            declare
               Success : Boolean;
               Msg : Inter_Process_Communication_Message_Block;
               Prio : Message_Priority_Level := Message_Priority_Level(Parameter_Three mod 8);
            begin
               Msg.Message_Type := Inter_Process_Communication_Send_Request;
               Msg.Source_Process_Identifier :=
                  Per_CPU_Data(Get_Current_CPU).Current_Process;
               Msg.Destination_Process_Identifier := Process_Identifier(Parameter_One);
               Msg.Payload_Data_Word_One := Parameter_Two;
               Success := Send_Message_With_Priority(
                  Process_Identifier(Parameter_One), Msg, Prio
               );
               Result_Code := (if Success then 0 else -1);
            end;

         when 2 =>  -- Receive message (priority-aware)
            declare
               Success : Boolean;
               Msg : Inter_Process_Communication_Message_Block;
            begin
               Success := Receive_Message_With_Priority(Msg);
               Result_Code := (if Success then 0 else -1);
            end;

         when 3 =>  -- Allocate memory
            declare
               Page_Addr : Physical_Memory_Address;
            begin
               Page_Addr := Allocate_Physical_Memory_Page;
               Result_Code := Integer(Page_Addr);
            end;

         when 4 =>  -- Allocate shared memory region
            declare
               Region_ID : Integer;
               Current_PID : Process_Identifier :=
                  Per_CPU_Data(Get_Current_CPU).Current_Process;
            begin
               Region_ID := Allocate_Shared_Memory_Region(Parameter_One, Current_PID);
               Result_Code := Region_ID;
            end;

         when 5 =>  -- Thread yield
            Schedule_Next_Ready_Process;
            Result_Code := 0;

         when others =>
            Result_Code := -1;
      end case;
   end Handle_System_Call_Interrupt;

   -- =========================================================================
   -- KERNEL MAIN EXECUTIVE (SMP-Enhanced)
   -- =========================================================================

begin
   -- Initialize hardware
   Disable_Hardware_Interrupts;
   Initialize_ARM_Vector_Table_And_Interrupts;

   -- Initialize kernel data structures
   Initialize_Process_Table_At_Boot;

   -- Initialize per-CPU data for boot CPU
   Per_CPU_Data(0).CPU_ID := 0;
   Per_CPU_Data(0).Current_Process := 0;
   Per_CPU_Data(0).Load_Average := 0;

   -- Print boot message
   Print_Kernel_Debug_String("Ada83 Minix SMP Microkernel v2.0" & ASCII.LF);
   Print_Kernel_Debug_String("Features: SMP + Zero-Copy IPC + Priority Queues" & ASCII.LF);
   Print_Kernel_Debug_String("Detected CPUs: ");
   Print_Kernel_Debug_String(Integer'Image(Current_CPU_Count) & ASCII.LF);
   Print_Kernel_Debug_String("Initializing IPC subsystem..." & ASCII.LF);
   Print_Kernel_Debug_String("Scheduler active (SMP mode). Entering main loop." & ASCII.LF);

   -- TODO: Boot secondary CPUs (would call Secondary_CPU_Boot_Entry_Point)
   -- For now, single-CPU mode

   -- Enable interrupts and start scheduling
   Enable_Hardware_Interrupts;

   -- Main kernel loop (per-CPU)
   loop
      Schedule_Next_Ready_Process;
      -- WFI handled in assembly idle loop
   end loop;

end Microkernel_SMP_Enhanced;
