with SYSTEM;

package body DIRECT_IO is

   -- Real external files with random access by element index (RM 14.2.4).
   -- Every ELEMENT_TYPE value occupies a fixed number of bytes, so element N
   -- lives at byte offset (N-1)*Element_Bytes; positioning is a seek and READ
   -- and WRITE transfer one element through the C stdio library.
   function C_Fopen(Name : SYSTEM.ADDRESS; Mode : SYSTEM.ADDRESS) return SYSTEM.ADDRESS;
   pragma Import(C, C_Fopen, "fopen");
   function C_Fclose(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fclose, "fclose");
   function C_Fread(Ptr : SYSTEM.ADDRESS; Size : Integer; Count : Integer;
                    Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fread, "fread");
   function C_Fwrite(Ptr : SYSTEM.ADDRESS; Size : Integer; Count : Integer;
                     Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fwrite, "fwrite");
   function C_Remove(Name : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Remove, "remove");
   function C_Fseek(Stream : SYSTEM.ADDRESS; Offset : Integer; Whence : Integer) return Integer;
   pragma Import(C, C_Fseek, "fseek");
   function C_Ftell(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Ftell, "ftell");
   function C_Fflush(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fflush, "fflush");
   function C_Tmpfile return SYSTEM.ADDRESS;
   pragma Import(C, C_Tmpfile, "tmpfile");

   Null_Address : constant SYSTEM.ADDRESS := SYSTEM.ADDRESS(0);
   Seek_Set     : constant Integer := 0;
   Seek_End     : constant Integer := 2;

   -- One element occupies its raw representation, rounded up to whole bytes.
   Element_Bytes : constant Integer := (ELEMENT_TYPE'SIZE + 7) / 8;

   type FCB is record
      Stream   : SYSTEM.ADDRESS := Null_Address;
      Mode     : FILE_MODE      := INOUT_FILE;
      Is_Open  : Boolean        := False;
      Index    : POSITIVE_COUNT := 1;
      Name_Len : Natural        := 0;
      Name     : String(1..1024);
   end record;
   FCBs     : array(1..99) of FCB;

   function Is_Open_Index(Idx : Integer) return Boolean is
   begin
      return Idx >= 1 and then Idx <= 99 and then FCBs(Idx).Is_Open;
   end Is_Open_Index;

   -- First control block not currently in use, so slots freed by CLOSE and
   -- DELETE are reclaimed (RM 14.4 places no lifetime cap on internal files).
   -- Returns 0 when the table is full.
   function Free_FCB_Index return Integer is
   begin
      for I in FCBs'Range loop
         if not FCBs(I).Is_Open then return I; end if;
      end loop;
      return 0;
   end Free_FCB_Index;

   -- Raise STATUS_ERROR unless FILE denotes an open external file (RM 14.4).
   function Require_Open(FILE : FILE_TYPE) return Integer is
      Idx : Integer := FILE.Handle;
   begin
      if not Is_Open_Index(Idx) then raise STATUS_ERROR; end if;
      return Idx;
   end Require_Open;

   procedure To_C_String(S : String; Buffer : out String) is
      J : Integer := 1;
   begin
      for I in S'First .. S'Last loop
         Buffer(J) := S(I);
         J := J + 1;
      end loop;
      Buffer(J) := Character'Val(0);
   end To_C_String;

   -- Position the stream at the first byte of element Element_Index.
   procedure Seek_To(Idx : Integer; Element_Index : POSITIVE_COUNT) is
      Offset : Integer := Integer(Element_Index - 1) * Element_Bytes;
      Ignore : Integer;
   begin
      Ignore := C_Fseek(FCBs(Idx).Stream, Offset, Seek_Set);
   end Seek_To;

   -- Number of elements currently in the external file.
   function Element_Count(Idx : Integer) return COUNT is
      Ignore : Integer;
      Bytes  : Integer;
   begin
      Ignore := C_Fflush(FCBs(Idx).Stream);
      Ignore := C_Fseek(FCBs(Idx).Stream, 0, Seek_End);
      Bytes  := C_Ftell(FCBs(Idx).Stream);
      if Bytes <= 0 then return 0; end if;
      return COUNT(Bytes / Element_Bytes);
   end Element_Count;

   -- CREATE and OPEN both take a fresh control block for FILE. CREATE truncates
   -- (or makes an anonymous temporary when NAME is null); OPEN requires the
   -- external file to already exist. RM 14.2.1 forbids either on an already-open
   -- FILE.
   procedure Attach(FILE : in out FILE_TYPE; MODE : in FILE_MODE;
                    NAME : in String; Creating : in Boolean) is
      Idx      : Integer;
      Name_Buf : String(1..1026);
      Mode_Str : String(1..4);
   begin
      if Is_Open_Index(FILE.Handle) then raise STATUS_ERROR; end if;
      Idx := Free_FCB_Index;
      if Idx = 0 then raise USE_ERROR; end if;
      if not Creating and then NAME'Length = 0 then raise NAME_ERROR; end if;

      if Creating then
         Mode_Str := ('w', '+', 'b', Character'Val(0));   -- truncate, read/write
      elsif MODE = IN_FILE then
         Mode_Str := ('r', 'b', Character'Val(0), Character'Val(0));
      else
         Mode_Str := ('r', '+', 'b', Character'Val(0));   -- must exist, read/write
      end if;

      if NAME'Length > 0 then
         To_C_String(NAME, Name_Buf);
         FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
         if FCBs(Idx).Stream = Null_Address then
            if Creating then raise USE_ERROR; else raise NAME_ERROR; end if;
         end if;
         FCBs(Idx).Name_Len := NAME'Length;
         FCBs(Idx).Name(1..NAME'Length) := NAME;
      else
         FCBs(Idx).Stream := C_Tmpfile;
         if FCBs(Idx).Stream = Null_Address then raise USE_ERROR; end if;
         FCBs(Idx).Name_Len := 0;
      end if;

      FCBs(Idx).Mode    := MODE;
      FCBs(Idx).Is_Open := True;
      FCBs(Idx).Index   := 1;
      FILE := (Handle => Idx);
   end Attach;

   procedure CREATE(FILE : in out FILE_TYPE; MODE : in FILE_MODE := INOUT_FILE;
                    NAME : in String := ""; FORM : in String := "") is
   begin
      Attach(FILE, MODE, NAME, Creating => True);
   end CREATE;

   procedure OPEN(FILE : in out FILE_TYPE; MODE : in FILE_MODE;
                  NAME : in String; FORM : in String := "") is
   begin
      Attach(FILE, MODE, NAME, Creating => False);
   end OPEN;

   procedure CLOSE(FILE : in out FILE_TYPE) is
      Idx : Integer := Require_Open(FILE);
      Ignore : Integer;
   begin
      Ignore := C_Fclose(FCBs(Idx).Stream);
      FCBs(Idx).Stream  := Null_Address;
      FCBs(Idx).Is_Open := False;
      FILE := (Handle => 0);
   end CLOSE;

   procedure DELETE(FILE : in out FILE_TYPE) is
      Idx : Integer := Require_Open(FILE);
      Name_Buf : String(1..1026);
      Ignore : Integer;
   begin
      Ignore := C_Fclose(FCBs(Idx).Stream);
      if FCBs(Idx).Name_Len > 0 then
         To_C_String(FCBs(Idx).Name(1..FCBs(Idx).Name_Len), Name_Buf);
         Ignore := C_Remove(Name_Buf'Address);
      end if;
      FCBs(Idx).Stream  := Null_Address;
      FCBs(Idx).Is_Open := False;
      FILE := (Handle => 0);
   end DELETE;

   procedure RESET(FILE : in out FILE_TYPE; MODE : in FILE_MODE) is
      Idx : Integer := Require_Open(FILE);
      Ignore : Integer;
   begin
      Ignore := C_Fflush(FCBs(Idx).Stream);
      Ignore := C_Fseek(FCBs(Idx).Stream, 0, Seek_Set);
      FCBs(Idx).Mode  := MODE;
      FCBs(Idx).Index := 1;
   end RESET;

   procedure RESET(FILE : in out FILE_TYPE) is
      Idx : Integer := Require_Open(FILE);
      Ignore : Integer;
   begin
      Ignore := C_Fflush(FCBs(Idx).Stream);
      Ignore := C_Fseek(FCBs(Idx).Stream, 0, Seek_Set);
      FCBs(Idx).Index := 1;
   end RESET;

   function MODE(FILE : in FILE_TYPE) return FILE_MODE is
      Idx : Integer := Require_Open(FILE);
   begin
      return FCBs(Idx).Mode;
   end MODE;

   function NAME(FILE : in FILE_TYPE) return String is
      Idx : Integer := Require_Open(FILE);
   begin
      -- A temporary file created without a name has no external name to
      -- return, so NAME raises USE_ERROR (RM 14.2.1).
      if FCBs(Idx).Name_Len = 0 then raise USE_ERROR; end if;
      return FCBs(Idx).Name(1..FCBs(Idx).Name_Len);
   end NAME;

   function FORM(FILE : in FILE_TYPE) return String is
      Idx : Integer := Require_Open(FILE);
   begin
      return "";
   end FORM;

   function IS_OPEN(FILE : in FILE_TYPE) return Boolean is
   begin
      return Is_Open_Index(FILE.Handle);
   end IS_OPEN;

   procedure READ(FILE : in FILE_TYPE; ITEM : out ELEMENT_TYPE;
                  FROM : in POSITIVE_COUNT) is
      Idx : Integer := Require_Open(FILE);
   begin
      if FCBs(Idx).Mode = OUT_FILE then raise MODE_ERROR; end if;
      Seek_To(Idx, FROM);
      if C_Fread(ITEM'Address, Element_Bytes, 1, FCBs(Idx).Stream) /= 1 then
         raise END_ERROR;
      end if;
      FCBs(Idx).Index := FROM + 1;
   end READ;

   procedure READ(FILE : in FILE_TYPE; ITEM : out ELEMENT_TYPE) is
      Idx : Integer := Require_Open(FILE);
   begin
      if FCBs(Idx).Mode = OUT_FILE then raise MODE_ERROR; end if;
      Seek_To(Idx, FCBs(Idx).Index);
      if C_Fread(ITEM'Address, Element_Bytes, 1, FCBs(Idx).Stream) /= 1 then
         raise END_ERROR;
      end if;
      FCBs(Idx).Index := FCBs(Idx).Index + 1;
   end READ;

   -- Both WRITEs flush after a successful fwrite: several internal files may
   -- denote the same external file (RM 14.1), and a reader through another
   -- stream must see this write, not the on-disk state it predates.
   procedure WRITE(FILE : in FILE_TYPE; ITEM : in ELEMENT_TYPE;
                   TO : in POSITIVE_COUNT) is
      Idx    : Integer := Require_Open(FILE);
      Ignore : Integer;
   begin
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      Seek_To(Idx, TO);
      if C_Fwrite(ITEM'Address, Element_Bytes, 1, FCBs(Idx).Stream) /= 1 then
         raise DEVICE_ERROR;
      end if;
      Ignore := C_Fflush(FCBs(Idx).Stream);
      FCBs(Idx).Index := TO + 1;
   end WRITE;

   procedure WRITE(FILE : in FILE_TYPE; ITEM : in ELEMENT_TYPE) is
      Idx    : Integer := Require_Open(FILE);
      Ignore : Integer;
   begin
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      Seek_To(Idx, FCBs(Idx).Index);
      if C_Fwrite(ITEM'Address, Element_Bytes, 1, FCBs(Idx).Stream) /= 1 then
         raise DEVICE_ERROR;
      end if;
      Ignore := C_Fflush(FCBs(Idx).Stream);
      FCBs(Idx).Index := FCBs(Idx).Index + 1;
   end WRITE;

   procedure SET_INDEX(FILE : in FILE_TYPE; TO : in POSITIVE_COUNT) is
      Idx : Integer := Require_Open(FILE);
   begin
      FCBs(Idx).Index := TO;
   end SET_INDEX;

   function INDEX(FILE : in FILE_TYPE) return POSITIVE_COUNT is
      Idx : Integer := Require_Open(FILE);
   begin
      return FCBs(Idx).Index;
   end INDEX;

   function SIZE(FILE : in FILE_TYPE) return COUNT is
      Idx : Integer := Require_Open(FILE);
   begin
      return Element_Count(Idx);
   end SIZE;

   function END_OF_FILE(FILE : in FILE_TYPE) return Boolean is
      Idx : Integer := Require_Open(FILE);
   begin
      if FCBs(Idx).Mode = OUT_FILE then raise MODE_ERROR; end if;
      return COUNT(FCBs(Idx).Index) > Element_Count(Idx);
   end END_OF_FILE;

end DIRECT_IO;
