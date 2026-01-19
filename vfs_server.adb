------------------------------------------------------------------------------
-- Virtual File System (VFS) Server - MINIX Style with FAT32 Support
-- Handles: open, close, read, write, stat, FAT32 mounting
------------------------------------------------------------------------------

with System;
with System.Machine_Code; use System.Machine_Code;
with Interfaces; use Interfaces;
with FAT32;

procedure VFS_Server is

   MAX_FILES : constant := 128;
   MAX_INODES : constant := 256;

   type File_Descriptor is range 0 .. MAX_FILES - 1;
   type Inode_Number is range 0 .. MAX_INODES - 1;

   type Inode is record
      Number : Inode_Number := 0;
      Size   : Unsigned_32 := 0;
      Mode   : Unsigned_16 := 0;  -- Permissions
      Refs   : Integer := 0;      -- Reference count
   end record;

   type Open_File is record
      Inode_Num : Inode_Number := 0;
      Position  : Unsigned_32 := 0;
      In_Use    : Boolean := False;
   end record;

   Inode_Table : array (Inode_Number) of Inode;
   File_Table  : array (File_Descriptor) of Open_File;

   -- FAT32 filesystem state
   Main_FS : FAT32.FAT32_FS;
   LF : constant Character := Character'Val (10);

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
   -- File Operations
   ----------------------------------------------------------------------------

   function Do_Open (Path : String) return File_Descriptor is
   begin
      for FD in File_Table'Range loop
         if not File_Table (FD).In_Use then
            File_Table (FD).In_Use := True;
            File_Table (FD).Inode_Num := 0;  -- Root inode
            File_Table (FD).Position := 0;

            Put_String ("[VFS] Open: " & Path & " -> FD=" &
               File_Descriptor'Image (FD) & ASCII.LF);

            return FD;
         end if;
      end loop;

      Put_String ("[VFS] Open failed: no free FDs" & ASCII.LF);
      return File_Descriptor'Last;  -- Error
   end Do_Open;

   procedure Do_Close (FD : File_Descriptor) is
   begin
      if FD < File_Table'Last and then File_Table (FD).In_Use then
         File_Table (FD).In_Use := False;

         Put_String ("[VFS] Close: FD=" &
            File_Descriptor'Image (FD) & ASCII.LF);
      end if;
   end Do_Close;

   function Do_Read (FD : File_Descriptor; Buf : Unsigned_32; Count : Integer)
      return Integer is
   begin
      if FD >= File_Table'Last or else not File_Table (FD).In_Use then
         Put_String ("[VFS] Read failed: invalid FD" & ASCII.LF);
         return -1;
      end if;

      -- For now, just return 0 (EOF)
      Put_String ("[VFS] Read: FD=" & File_Descriptor'Image (FD) &
         " count=" & Integer'Image (Count) & ASCII.LF);

      return 0;
   end Do_Read;

   function Do_Write (FD : File_Descriptor; Buf : Unsigned_32; Count : Integer)
      return Integer is
   begin
      if FD >= File_Table'Last or else not File_Table (FD).In_Use then
         return -1;
      end if;

      -- Special case: FD 1 = stdout (console)
      if FD = 1 then
         declare
            Addr : System.Address := System.Storage_Elements.To_Address (
               System.Storage_Elements.Integer_Address (Buf));
         begin
            for I in 0 .. Count - 1 loop
               declare
                  C : Character with Address => Addr + System.Storage_Elements.Storage_Offset (I);
               begin
                  Asm ("ldrb r0, [%0]" & LF &
                       "ldr r1, =0x09000000" & LF &
                       "strb r0, [r1]",
                       Inputs => System.Address'Asm_Input ("r", Addr),
                       Volatile => True,
                       Clobber => "r0,r1,memory");
               end;
            end loop;
         end;

         return Count;
      end if;

      return -1;
   end Do_Write;

   ----------------------------------------------------------------------------
   -- Main Loop
   ----------------------------------------------------------------------------

begin
   Put_String (ASCII.LF);
   Put_String ("╔══════════════════════════════════════════════════╗" & ASCII.LF);
   Put_String ("║   Virtual File System (VFS) Server Starting      ║" & ASCII.LF);
   Put_String ("╚══════════════════════════════════════════════════╝" & ASCII.LF);
   Put_String (ASCII.LF);

   -- Initialize inode table
   for I in Inode_Table'Range loop
      Inode_Table (I).Number := I;
      Inode_Table (I).Size := 0;
      Inode_Table (I).Refs := 0;
   end loop;

   -- Initialize file table
   for FD in File_Table'Range loop
      File_Table (FD).In_Use := False;
   end loop;

   -- Create stdin, stdout, stderr
   File_Table (0).In_Use := True;  -- stdin
   File_Table (0).Inode_Num := 0;

   File_Table (1).In_Use := True;  -- stdout
   File_Table (1).Inode_Num := 1;

   File_Table (2).In_Use := True;  -- stderr
   File_Table (2).Inode_Num := 2;

   Put_String ("[VFS] Standard FDs created (0=stdin, 1=stdout, 2=stderr)" & ASCII.LF);
   Put_String (ASCII.LF);

   -- Mount FAT32 filesystem
   Put_String ("╔══════════════════════════════════════════════════╗" & ASCII.LF);
   Put_String ("║   Mounting FAT32 Filesystem                      ║" & ASCII.LF);
   Put_String ("╚══════════════════════════════════════════════════╝" & ASCII.LF);
   Put_String (ASCII.LF);

   FAT32.Mount_FAT32 (Main_FS, 0);  -- Device 0

   if Main_FS.Mounted then
      Put_String (ASCII.LF);
      Put_String ("╔══════════════════════════════════════════════════╗" & ASCII.LF);
      Put_String ("║   Listing Root Directory                         ║" & ASCII.LF);
      Put_String ("╚══════════════════════════════════════════════════╝" & ASCII.LF);
      Put_String (ASCII.LF);

      -- List root directory
      FAT32.Read_Directory (Main_FS, Main_FS.Root_Cluster);
   end if;

   Put_String (ASCII.LF);
   Put_String ("[VFS] Entering main loop..." & ASCII.LF & ASCII.LF);

   -- Main server loop
   loop
      -- Receive IPC requests
      Asm ("wfe", Volatile => True);
   end loop;

end VFS_Server;
