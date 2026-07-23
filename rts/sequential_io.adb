with SYSTEM;

package body SEQUENTIAL_IO is

   -- Real external files, mirroring TEXT_IO: each ELEMENT_TYPE value is read
   -- and written as its raw representation through the C stdio library, so a
   -- file created, written, closed and reopened by name returns its contents,
   -- and DELETE removes the external file (RM 14.2).
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
   function C_Feof(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Feof, "feof");
   function C_Fseek(Stream : SYSTEM.ADDRESS; Offset : Integer; Whence : Integer) return Integer;
   pragma Import(C, C_Fseek, "fseek");
   function C_Fflush(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fflush, "fflush");
   function C_Fgetc(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fgetc, "fgetc");
   function C_Ungetc(C : Integer; Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Ungetc, "ungetc");
   function C_Tmpfile return SYSTEM.ADDRESS;
   pragma Import(C, C_Tmpfile, "tmpfile");

   Null_Address : constant SYSTEM.ADDRESS := SYSTEM.ADDRESS(0);
   Seek_Set     : constant Integer := 0;

   -- One element occupies its raw representation, rounded up to whole bytes.
   Element_Bytes : constant Integer := (ELEMENT_TYPE'SIZE + 7) / 8;

   type FCB is record
      Stream   : SYSTEM.ADDRESS := Null_Address;
      Mode     : FILE_MODE      := IN_FILE;
      Is_Open  : Boolean        := False;
      Name_Len : Natural        := 0;
      Name     : String(1..1024);
   end record;
   FCBs     : array(1..99) of FCB;

   function Is_Open_Index(Idx : Integer) return Boolean is
   begin
      return Idx >= 1 and then Idx <= 99 and then FCBs(Idx).Is_Open;
   end Is_Open_Index;

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
      -- Reuse a closed slot: a CLOSE/DELETE frees its FCB, so USE_ERROR is
      -- raised only when all slots are actually open (RM 14.2.1), not after
      -- 99 cumulative opens as a monotonic counter would (ce2117a).
      Idx := 0;
      for J in FCBs'Range loop
         if not FCBs(J).Is_Open then Idx := J; exit; end if;
      end loop;
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
      FILE := (Handle => Idx);
   end Attach;

   procedure CREATE(FILE : in out FILE_TYPE; MODE : in FILE_MODE := OUT_FILE;
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
      FCBs(Idx).Mode := MODE;
   end RESET;

   procedure RESET(FILE : in out FILE_TYPE) is
   begin
      RESET(FILE, MODE(FILE));
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

   procedure READ(FILE : in FILE_TYPE; ITEM : out ELEMENT_TYPE) is
      Idx : Integer := Require_Open(FILE);
   begin
      if FCBs(Idx).Mode = OUT_FILE then raise MODE_ERROR; end if;
      if C_Fread(ITEM'Address, Element_Bytes, 1, FCBs(Idx).Stream) /= 1 then
         raise END_ERROR;
      end if;
   end READ;

   procedure WRITE(FILE : in FILE_TYPE; ITEM : in ELEMENT_TYPE) is
      Idx : Integer := Require_Open(FILE);
   begin
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      if C_Fwrite(ITEM'Address, Element_Bytes, 1, FCBs(Idx).Stream) /= 1 then
         raise DEVICE_ERROR;
      end if;
   end WRITE;

   function END_OF_FILE(FILE : in FILE_TYPE) return Boolean is
      Idx : Integer := Require_Open(FILE);
      C   : Integer;
      Ignore : Integer;
   begin
      if FCBs(Idx).Mode = OUT_FILE then raise MODE_ERROR; end if;
      C := C_Fgetc(FCBs(Idx).Stream);
      if C = -1 then
         return True;
      else
         Ignore := C_Ungetc(C, FCBs(Idx).Stream);
         return False;
      end if;
   end END_OF_FILE;

end SEQUENTIAL_IO;
