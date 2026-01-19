------------------------------------------------------------------------------
-- FAT32 Filesystem Driver for Ada2012 MINIX OS
-- Implementation with Thumb-2 optimized inline assembly
------------------------------------------------------------------------------
pragma Restrictions (No_Elaboration_Code);
pragma Profile (Ravenscar);

with System;
with System.Machine_Code; use System.Machine_Code;
with System.Storage_Elements;

package body FAT32 with
   SPARK_Mode => Off  -- Inline assembly not SPARK compatible
is
   LF : constant Character := Character'Val (10);

   -- Block device interface (simplified - in real system would be driver)
   type Block_Device is record
      Base_Address : System.Address;
      Block_Size   : Unsigned_32;
      Total_Blocks : Unsigned_32;
   end record;

   -- Global FAT32 state (in real system would be per-mount)
   Current_Device : Block_Device;

   --------------------------------------------------------------------------
   -- UART output for debugging
   --------------------------------------------------------------------------
   UART0_BASE : constant := 16#09000000#;

   procedure Put_Char (C : Character) with Inline_Always is
   begin
      Asm ("strb %0, [%1]",
           Inputs => (Character'Asm_Input ("r", C),
                     System.Address'Asm_Input ("r",
                        System.Storage_Elements.To_Address (UART0_BASE))),
           Volatile => True);
   end Put_Char;

   procedure Put_Line (S : String) is
   begin
      for I in S'Range loop
         Put_Char (S (I));
      end loop;
      Put_Char (LF);
   end Put_Line;

   procedure Put_Hex (Val : Unsigned_32) is
      Hex : constant String := "0123456789ABCDEF";
      Tmp : Unsigned_32 := Val;
   begin
      Put_Char ('0');
      Put_Char ('x');
      for I in 1 .. 8 loop
         Put_Char (Hex (1 + Integer ((Tmp / 16#1000_0000#) and 16#F#)));
         Tmp := Tmp * 16;
      end loop;
   end Put_Hex;

   --------------------------------------------------------------------------
   -- Low-level disk I/O with NEON optimization
   --------------------------------------------------------------------------

   procedure Read_Sector (Sector : Unsigned_32; Buffer : out String) is
      Sector_Addr : constant System.Address :=
         System.Storage_Elements.To_Address (
            System.Storage_Elements.To_Integer (Current_Device.Base_Address) +
            System.Storage_Elements.Storage_Offset (Sector * 512));
      Buf_Addr : constant System.Address := Buffer'Address;
   begin
      -- Use NEON to copy 512 bytes (64 bytes per iteration, 8 iterations)
      for I in 0 .. 7 loop
         declare
            Src : constant System.Address :=
               System.Storage_Elements.To_Address (
                  System.Storage_Elements.To_Integer (Sector_Addr) +
                  System.Storage_Elements.Storage_Offset (I * 64));
            Dst : constant System.Address :=
               System.Storage_Elements.To_Address (
                  System.Storage_Elements.To_Integer (Buf_Addr) +
                  System.Storage_Elements.Storage_Offset (I * 64));
         begin
            -- NEON 64-byte copy (8x vld1.64/vst1.64)
            Asm ("vld1.64 {d0-d3}, [%0]!" & LF &
                 "vld1.64 {d4-d7}, [%0]" & LF &
                 "vst1.64 {d0-d3}, [%1]!" & LF &
                 "vst1.64 {d4-d7}, [%1]",
                 Inputs => (System.Address'Asm_Input ("r", Src),
                           System.Address'Asm_Input ("r", Dst)),
                 Volatile => True,
                 Clobber => "d0,d1,d2,d3,d4,d5,d6,d7,memory");
         end;
      end loop;
   end Read_Sector;

   --------------------------------------------------------------------------
   -- FAT table operations with Thumb-2 optimization
   --------------------------------------------------------------------------

   function Read_FAT_Entry (FS : FAT32_FS; Cluster : Unsigned_32)
      return Unsigned_32 is
      FAT_Offset : constant Unsigned_32 := Cluster * 4;  -- 4 bytes per entry
      FAT_Sector : constant Unsigned_32 :=
         Unsigned_32 (FS.Reserved_Sectors) + (FAT_Offset / 512);
      Entry_Offset : constant Unsigned_32 := FAT_Offset mod 512;
      Sector_Buffer : String (1 .. 512);
      Result : Unsigned_32;
   begin
      Read_Sector (FAT_Sector, Sector_Buffer);

      -- Extract 32-bit FAT entry (little-endian)
      -- Use Thumb-2 optimized byte extraction with IT blocks
      declare
         B0, B1, B2, B3 : Unsigned_8;
         Addr : constant System.Address :=
            System.Storage_Elements.To_Address (
               System.Storage_Elements.To_Integer (Sector_Buffer'Address) +
               System.Storage_Elements.Storage_Offset (Entry_Offset));
      begin
         Asm ("ldrb %0, [%4]" & LF &      -- Load byte 0
              "ldrb %1, [%4, #1]" & LF &  -- Load byte 1
              "ldrb %2, [%4, #2]" & LF &  -- Load byte 2
              "ldrb %3, [%4, #3]",        -- Load byte 3
              Outputs => (Unsigned_8'Asm_Output ("=r", B0),
                         Unsigned_8'Asm_Output ("=r", B1),
                         Unsigned_8'Asm_Output ("=r", B2),
                         Unsigned_8'Asm_Output ("=r", B3)),
              Inputs => (System.Address'Asm_Input ("r", Addr)),
              Volatile => True);

         -- Combine bytes with IT block optimization
         Asm ("orr %0, %1, %2, lsl #8" & LF &   -- Result = B0 | (B1 << 8)
              "orr %0, %0, %3, lsl #16" & LF &  -- Result |= (B2 << 16)
              "orr %0, %0, %4, lsl #24",        -- Result |= (B3 << 24)
              Outputs => (Unsigned_32'Asm_Output ("=r", Result)),
              Inputs => (Unsigned_8'Asm_Input ("r", B0),
                        Unsigned_8'Asm_Input ("r", B1),
                        Unsigned_8'Asm_Input ("r", B2),
                        Unsigned_8'Asm_Input ("r", B3)),
              Volatile => False);
      end;

      -- Mask to 28 bits (FAT32 uses only lower 28 bits)
      return Result and 16#0FFFFFFF#;
   end Read_FAT_Entry;

   --------------------------------------------------------------------------
   -- Cluster chain traversal
   --------------------------------------------------------------------------

   function Get_Next_Cluster (FS : FAT32_FS; Current_Cluster : Unsigned_32)
      return Unsigned_32 is
   begin
      return Read_FAT_Entry (FS, Current_Cluster);
   end Get_Next_Cluster;

   function Is_EOF_Cluster (Cluster : Unsigned_32) return Boolean is
   begin
      return Cluster >= CLUSTER_EOF;
   end Is_EOF_Cluster;

   --------------------------------------------------------------------------
   -- Cluster/Sector conversion
   --------------------------------------------------------------------------

   function Cluster_To_Sector (FS : FAT32_FS; Cluster : Unsigned_32)
      return Unsigned_32 is
      Result : Unsigned_32;
   begin
      -- First_Data_Sector + (Cluster - 2) * Sectors_Per_Cluster
      -- Use Thumb-2 for compact calculation
      Asm ("sub %0, %1, #2" & LF &                    -- Cluster - 2
           "mul %0, %0, %2" & LF &                    -- * Sectors_Per_Cluster
           "add %0, %0, %3",                          -- + First_Data_Sector
           Outputs => (Unsigned_32'Asm_Output ("=r", Result)),
           Inputs => (Unsigned_32'Asm_Input ("r", Cluster),
                     Unsigned_8'Asm_Input ("r", FS.Sectors_Per_Cluster),
                     Unsigned_32'Asm_Input ("r", FS.First_Data_Sector)),
           Volatile => False);
      return Result;
   end Cluster_To_Sector;

   --------------------------------------------------------------------------
   -- Directory entry helpers
   --------------------------------------------------------------------------

   function Get_Cluster (Entry_Data : Dir_Entry) return Unsigned_32 is
      Result : Unsigned_32;
   begin
      -- Combine high and low 16-bit cluster values
      Asm ("orr %0, %1, %2, lsl #16",
           Outputs => (Unsigned_32'Asm_Output ("=r", Result)),
           Inputs => (Unsigned_16'Asm_Input ("r", Entry_Data.First_Cluster_Low),
                     Unsigned_16'Asm_Input ("r", Entry_Data.First_Cluster_High)),
           Volatile => False);
      return Result;
   end Get_Cluster;

   --------------------------------------------------------------------------
   -- Mount FAT32 filesystem
   --------------------------------------------------------------------------

   procedure Mount_FAT32 (FS : in out FAT32_FS; Device_ID : Unsigned_32) is
      Boot_Buffer : String (1 .. 512);
      Boot : Boot_Sector with Address => Boot_Buffer'Address;
      First_FAT_Sector : Unsigned_32;
   begin
      Put_Line ("[FAT32] Mounting filesystem...");

      -- Initialize device (simplified - would call actual driver)
      Current_Device.Base_Address :=
         System.Storage_Elements.To_Address (16#40000000#);  -- Example address
      Current_Device.Block_Size := 512;
      Current_Device.Total_Blocks := Device_ID;  -- Simplified

      -- Read boot sector
      Read_Sector (0, Boot_Buffer);

      -- Verify FAT32 signature
      if Boot.Boot_Signature /= 16#29# then
         Put_Line ("[FAT32] ERROR: Invalid boot signature");
         return;
      end if;

      if Boot.FS_Type (1 .. 5) /= "FAT32" then
         Put_Line ("[FAT32] ERROR: Not a FAT32 filesystem");
         return;
      end if;

      -- Parse boot sector
      FS.Root_Cluster := Boot.Root_Cluster;
      FS.Bytes_Per_Sector := Boot.Bytes_Per_Sector;
      FS.Sectors_Per_Cluster := Boot.Sectors_Per_Cluster;
      FS.Reserved_Sectors := Boot.Reserved_Sectors;
      FS.Num_FATs := Boot.Num_FATs;
      FS.FAT_Size := Boot.FAT_Size_32;

      -- Calculate first data sector
      First_FAT_Sector := Unsigned_32 (FS.Reserved_Sectors);
      FS.First_Data_Sector := First_FAT_Sector +
         (Unsigned_32 (FS.Num_FATs) * FS.FAT_Size);

      -- Calculate total clusters
      FS.Total_Clusters :=
         (Boot.Total_Sectors_32 - FS.First_Data_Sector) /
         Unsigned_32 (FS.Sectors_Per_Cluster);

      FS.Mounted := True;

      Put_Line ("[FAT32] Mounted successfully");
      Put_Line ("[FAT32] Volume: " & Boot.Volume_Label);
      Put_Hex (FS.Root_Cluster);
      Put_Line (" (root cluster)");
   end Mount_FAT32;

   --------------------------------------------------------------------------
   -- Read directory entries
   --------------------------------------------------------------------------

   procedure Read_Directory (FS : in FAT32_FS; Cluster : Unsigned_32) is
      Current_Cluster : Unsigned_32 := Cluster;
      Sector : Unsigned_32;
      Sector_Buffer : String (1 .. 512);
      Entry : Dir_Entry with Address => Sector_Buffer'Address;
   begin
      if not FS.Mounted then
         Put_Line ("[FAT32] ERROR: Filesystem not mounted");
         return;
      end if;

      Put_Line ("[FAT32] Reading directory...");

      -- Traverse cluster chain
      while not Is_EOF_Cluster (Current_Cluster) loop
         -- Read all sectors in this cluster
         for S in 0 .. Unsigned_32 (FS.Sectors_Per_Cluster) - 1 loop
            Sector := Cluster_To_Sector (FS, Current_Cluster) + S;
            Read_Sector (Sector, Sector_Buffer);

            -- Process directory entries (16 per sector)
            for E in 0 .. 15 loop
               declare
                  Entry_Offset : constant Unsigned_32 := Unsigned_32 (E) * 32;
                  Entry_Addr : constant System.Address :=
                     System.Storage_Elements.To_Address (
                        System.Storage_Elements.To_Integer (
                           Sector_Buffer'Address) +
                        System.Storage_Elements.Storage_Offset (Entry_Offset));
                  Dir_Ent : Dir_Entry with Address => Entry_Addr;
               begin
                  -- Check if entry is valid
                  if Dir_Ent.Name (1) = Character'Val (0) then
                     -- End of directory
                     return;
                  elsif Dir_Ent.Name (1) /= Character'Val (16#E5#) then
                     -- Valid entry (not deleted)
                     if (Dir_Ent.Attributes and ATTR_LONG_NAME) /=
                        ATTR_LONG_NAME then
                        -- Regular entry (not LFN)
                        Put_Line ("  " & Dir_Ent.Name & "." &
                                 Dir_Ent.Extension);
                     end if;
                  end if;
               end;
            end loop;
         end loop;

         -- Get next cluster in chain
         Current_Cluster := Get_Next_Cluster (FS, Current_Cluster);
      end loop;
   end Read_Directory;

   --------------------------------------------------------------------------
   -- Read file data
   --------------------------------------------------------------------------

   procedure Read_File (FS : in FAT32_FS; Cluster : Unsigned_32;
                       Size : Unsigned_32; Buffer : out String) is
      Current_Cluster : Unsigned_32 := Cluster;
      Bytes_Read : Unsigned_32 := 0;
      Bytes_To_Read : Unsigned_32;
      Cluster_Size : constant Unsigned_32 :=
         Unsigned_32 (FS.Sectors_Per_Cluster) * 512;
      Sector_Buffer : String (1 .. 512);
   begin
      if not FS.Mounted then
         Put_Line ("[FAT32] ERROR: Filesystem not mounted");
         return;
      end if;

      Put_Line ("[FAT32] Reading file...");

      -- Traverse cluster chain
      while not Is_EOF_Cluster (Current_Cluster) and Bytes_Read < Size loop
         -- Read all sectors in this cluster
         for S in 0 .. Unsigned_32 (FS.Sectors_Per_Cluster) - 1 loop
            exit when Bytes_Read >= Size;

            declare
               Sector : constant Unsigned_32 :=
                  Cluster_To_Sector (FS, Current_Cluster) + S;
            begin
               Read_Sector (Sector, Sector_Buffer);

               -- Copy to output buffer
               Bytes_To_Read := Unsigned_32'Min (512, Size - Bytes_Read);

               for I in 1 .. Integer (Bytes_To_Read) loop
                  Buffer (Buffer'First + Integer (Bytes_Read) + I - 1) :=
                     Sector_Buffer (I);
               end loop;

               Bytes_Read := Bytes_Read + Bytes_To_Read;
            end;
         end loop;

         -- Get next cluster
         Current_Cluster := Get_Next_Cluster (FS, Current_Cluster);
      end loop;

      Put_Hex (Bytes_Read);
      Put_Line (" bytes read");
   end Read_File;

end FAT32;
