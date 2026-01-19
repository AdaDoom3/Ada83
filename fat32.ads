------------------------------------------------------------------------------
-- FAT32 Filesystem Driver for Ada2012 MINIX OS
-- Specification
------------------------------------------------------------------------------
pragma Restrictions (No_Elaboration_Code);
pragma Profile (Ravenscar);

with Interfaces; use Interfaces;

package FAT32 with
   SPARK_Mode => On,
   Pure
is
   pragma Elaborate_Body;

   -- FAT32 Boot Sector (Standard structure)
   type Boot_Sector is record
      Jump_Boot       : Unsigned_8;       -- 0xEB or 0xE9
      OEM_Name        : String (1 .. 8);  -- "MSWIN4.1" etc.
      Bytes_Per_Sector : Unsigned_16;     -- Usually 512
      Sectors_Per_Cluster : Unsigned_8;   -- Power of 2
      Reserved_Sectors : Unsigned_16;     -- Usually 32 for FAT32
      Num_FATs        : Unsigned_8;       -- Usually 2
      Root_Entry_Count : Unsigned_16;     -- 0 for FAT32
      Total_Sectors_16 : Unsigned_16;     -- 0 for FAT32
      Media_Type      : Unsigned_8;       -- 0xF8 for hard disk
      FAT_Size_16     : Unsigned_16;      -- 0 for FAT32
      Sectors_Per_Track : Unsigned_16;
      Num_Heads       : Unsigned_16;
      Hidden_Sectors  : Unsigned_32;
      Total_Sectors_32 : Unsigned_32;     -- Total sectors

      -- FAT32 Extended Boot Record
      FAT_Size_32     : Unsigned_32;      -- Sectors per FAT
      Ext_Flags       : Unsigned_16;
      FS_Version      : Unsigned_16;
      Root_Cluster    : Unsigned_32;      -- Usually 2
      FS_Info_Sector  : Unsigned_16;      -- Usually 1
      Backup_Boot_Sector : Unsigned_16;   -- Usually 6
      Reserved        : String (1 .. 12);
      Drive_Number    : Unsigned_8;
      Reserved1       : Unsigned_8;
      Boot_Signature  : Unsigned_8;       -- 0x29
      Volume_ID       : Unsigned_32;
      Volume_Label    : String (1 .. 11);
      FS_Type         : String (1 .. 8);  -- "FAT32   "
   end record;

   for Boot_Sector use record
      Jump_Boot       at 0 range 0 .. 7;
      OEM_Name        at 3 range 0 .. 63;
      Bytes_Per_Sector at 11 range 0 .. 15;
      Sectors_Per_Cluster at 13 range 0 .. 7;
      Reserved_Sectors at 14 range 0 .. 15;
      Num_FATs        at 16 range 0 .. 7;
      Root_Entry_Count at 17 range 0 .. 15;
      Total_Sectors_16 at 19 range 0 .. 15;
      Media_Type      at 21 range 0 .. 7;
      FAT_Size_16     at 22 range 0 .. 15;
      Sectors_Per_Track at 24 range 0 .. 15;
      Num_Heads       at 26 range 0 .. 15;
      Hidden_Sectors  at 28 range 0 .. 31;
      Total_Sectors_32 at 32 range 0 .. 31;
      FAT_Size_32     at 36 range 0 .. 31;
      Ext_Flags       at 40 range 0 .. 15;
      FS_Version      at 42 range 0 .. 15;
      Root_Cluster    at 44 range 0 .. 31;
      FS_Info_Sector  at 48 range 0 .. 15;
      Backup_Boot_Sector at 50 range 0 .. 15;
      Reserved        at 52 range 0 .. 95;
      Drive_Number    at 64 range 0 .. 7;
      Reserved1       at 65 range 0 .. 7;
      Boot_Signature  at 66 range 0 .. 7;
      Volume_ID       at 67 range 0 .. 31;
      Volume_Label    at 71 range 0 .. 87;
      FS_Type         at 82 range 0 .. 63;
   end record;

   for Boot_Sector'Size use 512 * 8;  -- 512 bytes

   -- FAT32 Directory Entry (32 bytes)
   type Dir_Entry is record
      Name            : String (1 .. 8);   -- Short name
      Extension       : String (1 .. 3);   -- Extension
      Attributes      : Unsigned_8;        -- File attributes
      Reserved        : Unsigned_8;
      Create_Time_Tenth : Unsigned_8;
      Create_Time     : Unsigned_16;
      Create_Date     : Unsigned_16;
      Last_Access_Date : Unsigned_16;
      First_Cluster_High : Unsigned_16;    -- High word of cluster
      Write_Time      : Unsigned_16;
      Write_Date      : Unsigned_16;
      First_Cluster_Low : Unsigned_16;     -- Low word of cluster
      File_Size       : Unsigned_32;       -- File size in bytes
   end record
      with Size => 32 * 8;

   for Dir_Entry use record
      Name            at 0 range 0 .. 63;
      Extension       at 8 range 0 .. 23;
      Attributes      at 11 range 0 .. 7;
      Reserved        at 12 range 0 .. 7;
      Create_Time_Tenth at 13 range 0 .. 7;
      Create_Time     at 14 range 0 .. 15;
      Create_Date     at 16 range 0 .. 15;
      Last_Access_Date at 18 range 0 .. 15;
      First_Cluster_High at 20 range 0 .. 15;
      Write_Time      at 22 range 0 .. 15;
      Write_Date      at 24 range 0 .. 15;
      First_Cluster_Low at 26 range 0 .. 15;
      File_Size       at 28 range 0 .. 31;
   end record;

   -- File Attributes
   ATTR_READ_ONLY  : constant := 16#01#;
   ATTR_HIDDEN     : constant := 16#02#;
   ATTR_SYSTEM     : constant := 16#04#;
   ATTR_VOLUME_ID  : constant := 16#08#;
   ATTR_DIRECTORY  : constant := 16#10#;
   ATTR_ARCHIVE    : constant := 16#20#;
   ATTR_LONG_NAME  : constant := 16#0F#;  -- Long filename entry

   -- Special cluster values
   CLUSTER_FREE    : constant := 16#00000000#;
   CLUSTER_EOF     : constant := 16#0FFFFFF8#;
   CLUSTER_BAD     : constant := 16#0FFFFFF7#;

   -- FAT32 Filesystem state
   type FAT32_FS is record
      Mounted         : Boolean := False;
      Root_Cluster    : Unsigned_32 := 0;
      Bytes_Per_Sector : Unsigned_16 := 512;
      Sectors_Per_Cluster : Unsigned_8 := 0;
      Reserved_Sectors : Unsigned_16 := 0;
      Num_FATs        : Unsigned_8 := 0;
      FAT_Size        : Unsigned_32 := 0;
      First_Data_Sector : Unsigned_32 := 0;
      Total_Clusters  : Unsigned_32 := 0;
   end record;

   -- Initialize FAT32 filesystem
   procedure Mount_FAT32 (FS : in out FAT32_FS; Device_ID : Unsigned_32);

   -- Read directory
   procedure Read_Directory (FS : in FAT32_FS; Cluster : Unsigned_32);

   -- Read file
   procedure Read_File (FS : in FAT32_FS; Cluster : Unsigned_32;
                       Size : Unsigned_32; Buffer : out String);

   -- Utility: Get full cluster number from directory entry
   function Get_Cluster (Entry_Data : Dir_Entry) return Unsigned_32
      with Inline_Always;

   -- Utility: Convert cluster to sector
   function Cluster_To_Sector (FS : FAT32_FS; Cluster : Unsigned_32)
      return Unsigned_32
      with Inline_Always;

   -- Utility: Get next cluster from FAT
   function Get_Next_Cluster (FS : FAT32_FS; Current_Cluster : Unsigned_32)
      return Unsigned_32;

   -- Utility: Check if cluster is EOF
   function Is_EOF_Cluster (Cluster : Unsigned_32) return Boolean
      with Inline_Always;

private
   -- Internal: Read sector from disk
   procedure Read_Sector (Sector : Unsigned_32; Buffer : out String);

   -- Internal: Read from FAT table
   function Read_FAT_Entry (FS : FAT32_FS; Cluster : Unsigned_32)
      return Unsigned_32;

end FAT32;
