with SYSTEM;

package body TEXT_IO is

   -- C runtime imports
   function C_Putchar(C : Integer) return Integer;
   pragma Import(C, C_Putchar, "putchar");

   function C_Getchar return Integer;
   pragma Import(C, C_Getchar, "getchar");

   function C_Fopen(Name : SYSTEM.ADDRESS; Mode : SYSTEM.ADDRESS) return SYSTEM.ADDRESS;
   pragma Import(C, C_Fopen, "fopen");

   function C_Fclose(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fclose, "fclose");

   function C_Fputc(C : Integer; Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fputc, "fputc");

   function C_Fgetc(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fgetc, "fgetc");

   function C_Ungetc(C : Integer; Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Ungetc, "ungetc");

   function C_Feof(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Feof, "feof");

   function C_Fflush(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Fflush, "fflush");

   function C_Remove(Name : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Remove, "remove");

   function C_Ftell(Stream : SYSTEM.ADDRESS) return Integer;
   pragma Import(C, C_Ftell, "ftell");

   function C_Fseek(Stream : SYSTEM.ADDRESS; Offset : Integer; Whence : Integer) return Integer;
   pragma Import(C, C_Fseek, "fseek");

   -- A nameless CREATE makes an anonymous temporary file (RM 14.2.1);
   -- tmpfile() opens one in "w+b" mode that is removed when closed.
   function C_Tmpfile return SYSTEM.ADDRESS;
   pragma Import(C, C_Tmpfile, "tmpfile");


   function C_Stdin return SYSTEM.ADDRESS;
   pragma Import(C, C_Stdin, "__ada_stdin");

   function C_Stdout return SYSTEM.ADDRESS;
   pragma Import(C, C_Stdout, "__ada_stdout");

   function C_Stderr return SYSTEM.ADDRESS;
   pragma Import(C, C_Stderr, "__ada_stderr");

   -- Pushback buffer: holds characters (and the -1 end marker) peeked ahead
   -- of the read cursor. Detecting a page terminator (which follows a line
   -- terminator) or the file terminator requires looking past the immediate
   -- line terminator, so several characters of lookahead are needed.
   type Lookahead_Buffer is array (0 .. 7) of Integer;

   -- Internal file control block
   type File_Control_Block is record
      Stream      : SYSTEM.ADDRESS;
      Mode        : FILE_MODE;
      Is_Open     : Boolean;
      Name_Len    : Integer;
      Name        : String(1..1024);
      Form_Len    : Integer;
      Form        : String(1..256);
      Col         : POSITIVE_COUNT;
      Line        : POSITIVE_COUNT;
      Page        : POSITIVE_COUNT;
      Line_Length : COUNT;
      Page_Length : COUNT;
      Is_Standard : Boolean;
      Look        : Lookahead_Buffer;
      Look_Count  : Integer;
      Page_Active : Boolean;  -- content written on the current page since the
                              -- last page terminator (drives the final
                              -- line/page terminator emitted at CLOSE)
   end record;

   type FCB_Array is array (0 .. 99) of File_Control_Block;
   FCBs : FCB_Array;

   -- Index 0 is the "no file" sentinel: a freshly declared FILE_TYPE is
   -- zero-initialized and therefore denotes a closed file (RM 14.1). The
   -- three standard files occupy 1, 2, 3.
   Current_In_Idx  : Integer := 1;
   Current_Out_Idx : Integer := 2;
   Current_Err_Idx : Integer := 3;

   Null_Address : constant SYSTEM.ADDRESS := SYSTEM.ADDRESS(0);

   -- Helper to convert string to C string (null-terminated)
   procedure To_C_String(S : String; Buffer : out String; Len : out Natural) is
      J : Integer;
   begin
      if S'Length > 0 then
         J := 1;
         for I in S'First .. S'Last loop
            Buffer(J) := S(I);
            J := J + 1;
         end loop;
         Buffer(J) := Character'Val(0);
         Len := J;
      else
         Buffer(1) := Character'Val(0);
         Len := 1;
      end if;
   end To_C_String;

   -- Initialize the three standard file handles at indices 1, 2, 3.
   procedure Init_Standard_File(Idx : Integer; Stream : SYSTEM.ADDRESS;
                                Mode : FILE_MODE) is
   begin
      FCBs(Idx).Stream := Stream;
      FCBs(Idx).Mode := Mode;
      FCBs(Idx).Is_Open := True;
      FCBs(Idx).Name_Len := 0;
      FCBs(Idx).Form_Len := 0;
      FCBs(Idx).Col := 1;
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := 1;
      FCBs(Idx).Line_Length := UNBOUNDED;
      FCBs(Idx).Page_Length := UNBOUNDED;
      FCBs(Idx).Is_Standard := True;
      FCBs(Idx).Look_Count := 0;
      FCBs(Idx).Page_Active := False;
   end Init_Standard_File;

   procedure Init_Standard_Files is
   begin
      Init_Standard_File (1, C_Stdin,  IN_FILE);
      Init_Standard_File (2, C_Stdout, OUT_FILE);
      Init_Standard_File (3, C_Stderr, OUT_FILE);
   end Init_Standard_Files;

   Initialized : Boolean := False;

   procedure Ensure_Init is
   begin
      if not Initialized then
         Init_Standard_Files;
         Initialized := True;
      end if;
   end Ensure_Init;

   -- True when Idx denotes a currently-open file (RM 14.1). Index 0 is the
   -- closed sentinel of a never-opened FILE_TYPE.
   function Is_Open_Index(Idx : Integer) return Boolean is
   begin
      return Idx >= 1 and Idx <= 99 and then FCBs(Idx).Is_Open;
   end Is_Open_Index;

   -- First control block not in use, starting past the three standard files,
   -- so slots freed by CLOSE and DELETE are reclaimed (RM 14.4 places no
   -- lifetime cap on internal files). Returns 0 (the closed sentinel) when
   -- the table is full.
   function Free_FCB_Index return Integer is
   begin
      for I in 4 .. 99 loop
         if not FCBs(I).Is_Open then return I; end if;
      end loop;
      return 0;
   end Free_FCB_Index;

   -- Raise STATUS_ERROR unless Idx denotes an open file (RM 14.4).
   procedure Require_Open(Idx : Integer) is
   begin
      if not Is_Open_Index(Idx) then raise STATUS_ERROR; end if;
   end Require_Open;

   -- Defined below; CLOSE writes the trailing terminators through it.
   procedure Raw_Put(Idx : Integer; C : Integer);

   -- Flush every open file already associated with the external file NAME, so
   -- that writes buffered against one internal file become visible to another
   -- internal file about to be opened on the same external file (RM 14.1).
   procedure Flush_External(NAME : String) is
      Dummy : Integer;
   begin
      if NAME'Length = 0 then return; end if;
      for I in 4 .. 99 loop
         if FCBs(I).Is_Open and then FCBs(I).Stream /= Null_Address
            and then FCBs(I).Name_Len = NAME'Length
            and then FCBs(I).Name(1 .. FCBs(I).Name_Len) = NAME then
            Dummy := C_Fflush(FCBs(I).Stream);
         end if;
      end loop;
   end Flush_External;

   -- File Management

   procedure CREATE(FILE : in out FILE_TYPE;
                    MODE : in FILE_MODE := OUT_FILE;
                    NAME : in STRING := "";
                    FORM : in STRING := "") is
      Idx : Integer;
      Mode_Str : String(1..3);
      Name_Buf : String(1..1025);
      Name_Len : Natural;
   begin
      Ensure_Init;

      if Is_Open_Index(Integer(FILE)) then
         raise STATUS_ERROR;
      end if;

      Idx := Free_FCB_Index;
      if Idx = 0 then
         raise USE_ERROR;
      end if;

      case MODE is
         when IN_FILE =>
            Mode_Str := ('r', Character'Val(0), Character'Val(0));
         when OUT_FILE =>
            Mode_Str := ('w', Character'Val(0), Character'Val(0));
         when APPEND_FILE =>
            Mode_Str := ('a', Character'Val(0), Character'Val(0));
      end case;

      -- A failed fopen leaves the slot unmarked, so it stays free.
      if NAME'Length > 0 then
         To_C_String(NAME, Name_Buf, Name_Len);
         FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
         if FCBs(Idx).Stream = Null_Address then
            raise NAME_ERROR;
         end if;
         FCBs(Idx).Name_Len := NAME'Length;
         FCBs(Idx).Name(1..NAME'Length) := NAME;
      else
         FCBs(Idx).Stream := C_Tmpfile;   -- anonymous temporary file
         if FCBs(Idx).Stream = Null_Address then
            raise USE_ERROR;
         end if;
         FCBs(Idx).Name_Len := 0;
      end if;

      FCBs(Idx).Mode := MODE;
      FCBs(Idx).Is_Open := True;
      FCBs(Idx).Form_Len := FORM'Length;
      if FORM'Length > 0 then
         FCBs(Idx).Form(1..FORM'Length) := FORM;
      end if;
      FCBs(Idx).Col := 1;
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := 1;
      FCBs(Idx).Line_Length := UNBOUNDED;
      FCBs(Idx).Page_Length := UNBOUNDED;
      FCBs(Idx).Is_Standard := False;
      FCBs(Idx).Look_Count := 0;
      FCBs(Idx).Page_Active := False;

      FILE := FILE_TYPE(Idx);
   end CREATE;

   procedure OPEN(FILE : in Out FILE_TYPE;
                  MODE : in FILE_MODE;
                  NAME : in STRING;
                  FORM : in STRING := "") is
      Idx : Integer;
      Mode_Str : String(1..3);
      Name_Buf : String(1..1025);
      Name_Len : Natural;
   begin
      Ensure_Init;

      if Is_Open_Index(Integer(FILE)) then
         raise STATUS_ERROR;
      end if;

      if NAME'Length = 0 then
         raise NAME_ERROR;
      end if;

      Idx := Free_FCB_Index;
      if Idx = 0 then
         raise USE_ERROR;
      end if;

      case MODE is
         when IN_FILE =>
            Mode_Str := ('r', Character'Val(0), Character'Val(0));
         when OUT_FILE =>
            Mode_Str := ('w', Character'Val(0), Character'Val(0));
         when APPEND_FILE =>
            Mode_Str := ('a', Character'Val(0), Character'Val(0));
      end case;

      -- Make any writes buffered against the same external file visible before
      -- this internal file starts reading it.
      Flush_External(NAME);

      To_C_String(NAME, Name_Buf, Name_Len);
      FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
      if FCBs(Idx).Stream = Null_Address then
         raise NAME_ERROR;   -- slot never marked open; stays free
      end if;

      FCBs(Idx).Mode := MODE;
      FCBs(Idx).Is_Open := True;
      FCBs(Idx).Name_Len := NAME'Length;
      FCBs(Idx).Name(1..NAME'Length) := NAME;
      FCBs(Idx).Form_Len := FORM'Length;
      if FORM'Length > 0 then
         FCBs(Idx).Form(1..FORM'Length) := FORM;
      end if;
      FCBs(Idx).Col := 1;
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := 1;
      FCBs(Idx).Line_Length := UNBOUNDED;
      FCBs(Idx).Page_Length := UNBOUNDED;
      FCBs(Idx).Is_Standard := False;
      FCBs(Idx).Look_Count := 0;
      FCBs(Idx).Page_Active := False;

      FILE := FILE_TYPE(Idx);
   end OPEN;

   procedure CLOSE(FILE : in Out FILE_TYPE) is
      Idx : Integer := Integer(FILE);
      Dummy : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Is_Standard then
         raise USE_ERROR;
      end if;
      -- Close the final line and page so the external file is well-formed:
      -- a non-empty page ends with a line terminator then a page terminator,
      -- before the file terminator (RM 14.3.1).
      if FCBs(Idx).Mode /= IN_FILE and FCBs(Idx).Page_Active then
         if FCBs(Idx).Col > 1 then
            Raw_Put(Idx, 10);  -- terminate the partial last line
         end if;
         Raw_Put(Idx, 12);     -- page terminator
         FCBs(Idx).Page_Active := False;
      end if;
      if FCBs(Idx).Stream /= Null_Address then
         Dummy := C_Fclose(FCBs(Idx).Stream);
      end if;
      FCBs(Idx).Is_Open := False;
      FCBs(Idx).Stream := Null_Address;
      FILE := 0;  -- the handle now denotes a closed file
   end CLOSE;

   procedure DELETE(FILE : in Out FILE_TYPE) is
      Idx : Integer := Integer(FILE);
      Name_Buf : String(1..1025);
      Name_Len : Natural;
      Dummy : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Is_Standard then
         raise USE_ERROR;
      end if;
      if FCBs(Idx).Stream /= Null_Address then
         Dummy := C_Fclose(FCBs(Idx).Stream);
      end if;
      if FCBs(Idx).Name_Len > 0 then
         To_C_String(FCBs(Idx).Name(1..FCBs(Idx).Name_Len), Name_Buf, Name_Len);
         Dummy := C_Remove(Name_Buf'Address);
      end if;
      FCBs(Idx).Is_Open := False;
      FCBs(Idx).Stream := Null_Address;
      FILE := 0;  -- the handle now denotes a closed file
   end DELETE;

   procedure RESET(FILE : in Out FILE_TYPE; MODE : in FILE_MODE) is
      Idx : Integer := Integer(FILE);
      Name_Buf : String(1..1025);
      Name_Len : Natural;
      Mode_Str : String(1..3);
      Dummy : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Is_Standard then
         raise USE_ERROR;
      end if;

      -- The mode of a file serving as a current default file may not be
      -- changed to one incompatible with that role (AI-00048): a default input
      -- must stay readable, a default output or error file writable.
      if (Idx = Current_Out_Idx or Idx = Current_Err_Idx) and MODE = IN_FILE then
         raise MODE_ERROR;
      end if;
      if Idx = Current_In_Idx and MODE /= IN_FILE then
         raise MODE_ERROR;
      end if;

      -- Finalize a partially-written output file before reopening, so its last
      -- line and page are terminated just as CLOSE would (RM 14.3.1).
      if FCBs(Idx).Mode /= IN_FILE and FCBs(Idx).Page_Active then
         if FCBs(Idx).Col > 1 then
            Raw_Put(Idx, 10);
         end if;
         Raw_Put(Idx, 12);
         FCBs(Idx).Page_Active := False;
      end if;

      if FCBs(Idx).Name_Len > 0 then
         -- Named file: close and reopen in the new mode.
         if FCBs(Idx).Stream /= Null_Address then
            Dummy := C_Fclose(FCBs(Idx).Stream);
         end if;
         case MODE is
            when IN_FILE =>
               Mode_Str := ('r', Character'Val(0), Character'Val(0));
            when OUT_FILE =>
               Mode_Str := ('w', Character'Val(0), Character'Val(0));
            when APPEND_FILE =>
               Mode_Str := ('a', Character'Val(0), Character'Val(0));
         end case;
         To_C_String(FCBs(Idx).Name(1..FCBs(Idx).Name_Len), Name_Buf, Name_Len);
         FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
      elsif FCBs(Idx).Stream /= Null_Address then
         -- Anonymous temporary file: it has no name to reopen, so flush any
         -- output and rewind to the start for the new mode.
         Dummy := C_Fflush(FCBs(Idx).Stream);
         Dummy := C_Fseek(FCBs(Idx).Stream, 0, 0);  -- SEEK_SET
      end if;

      FCBs(Idx).Mode := MODE;
      FCBs(Idx).Col := 1;
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := 1;
      FCBs(Idx).Line_Length := UNBOUNDED;  -- RM 14.3.3: RESET clears the
      FCBs(Idx).Page_Length := UNBOUNDED;  -- line and page length bounds
      FCBs(Idx).Look_Count := 0;
      FCBs(Idx).Page_Active := False;
   end RESET;

   procedure RESET(FILE : in Out FILE_TYPE) is
      Idx : Integer := Integer(FILE);
   begin
      RESET(FILE, FCBs(Idx).Mode);
   end RESET;

   function MODE(FILE : in FILE_TYPE) return FILE_MODE is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      return FCBs(Idx).Mode;
   end MODE;

   function NAME(FILE : in FILE_TYPE) return STRING is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Name_Len = 0 then
         return "";
      end if;
      return FCBs(Idx).Name(1..FCBs(Idx).Name_Len);
   end NAME;

   function FORM(FILE : in FILE_TYPE) return STRING is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Form_Len = 0 then
         return "";
      end if;
      return FCBs(Idx).Form(1..FCBs(Idx).Form_Len);
   end FORM;

   function IS_OPEN(FILE : in FILE_TYPE) return BOOLEAN is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if Idx < 0 or Idx > 99 then
         return False;
      end if;
      return FCBs(Idx).Is_Open;
   end IS_OPEN;

   -- Default Input/Output Control

   procedure SET_INPUT(FILE : in FILE_TYPE) is
   begin
      Ensure_Init;
      Require_Open(Integer(FILE));
      if FCBs(Integer(FILE)).Mode /= IN_FILE then raise MODE_ERROR; end if;
      Current_In_Idx := Integer(FILE);
   end SET_INPUT;

   procedure SET_OUTPUT(FILE : in FILE_TYPE) is
   begin
      Ensure_Init;
      Require_Open(Integer(FILE));
      if FCBs(Integer(FILE)).Mode = IN_FILE then raise MODE_ERROR; end if;
      Current_Out_Idx := Integer(FILE);
   end SET_OUTPUT;

   procedure SET_ERROR(FILE : in FILE_TYPE) is
   begin
      Ensure_Init;
      Require_Open(Integer(FILE));
      if FCBs(Integer(FILE)).Mode = IN_FILE then raise MODE_ERROR; end if;
      Current_Err_Idx := Integer(FILE);
   end SET_ERROR;

   function STANDARD_INPUT return FILE_TYPE is
   begin
      Ensure_Init;
      return 1;
   end STANDARD_INPUT;

   function STANDARD_OUTPUT return FILE_TYPE is
   begin
      Ensure_Init;
      return 2;
   end STANDARD_OUTPUT;

   function STANDARD_ERROR return FILE_TYPE is
   begin
      Ensure_Init;
      return 3;
   end STANDARD_ERROR;

   function CURRENT_INPUT return FILE_TYPE is
   begin
      Ensure_Init;
      return FILE_TYPE(Current_In_Idx);
   end CURRENT_INPUT;

   function CURRENT_OUTPUT return FILE_TYPE is
   begin
      Ensure_Init;
      return FILE_TYPE(Current_Out_Idx);
   end CURRENT_OUTPUT;

   function CURRENT_ERROR return FILE_TYPE is
   begin
      Ensure_Init;
      return FILE_TYPE(Current_Err_Idx);
   end CURRENT_ERROR;

   procedure FLUSH(FILE : in FILE_TYPE) is
      Idx : Integer := Integer(FILE);
      Dummy : Integer;
   begin
      Ensure_Init;
      if FCBs(Idx).Stream /= Null_Address then
         Dummy := C_Fflush(FCBs(Idx).Stream);
      end if;
   end FLUSH;

   procedure FLUSH is
   begin
      FLUSH(FILE_TYPE(Current_Out_Idx));
   end FLUSH;

   -- Line/Page Length Control

   procedure SET_LINE_LENGTH(FILE : in FILE_TYPE; TO : in COUNT) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      FCBs(Idx).Line_Length := TO;
   end SET_LINE_LENGTH;

   procedure SET_LINE_LENGTH(TO : in COUNT) is
   begin
      SET_LINE_LENGTH(FILE_TYPE(Current_Out_Idx), TO);
   end SET_LINE_LENGTH;

   procedure SET_PAGE_LENGTH(FILE : in FILE_TYPE; TO : in COUNT) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      FCBs(Idx).Page_Length := TO;
   end SET_PAGE_LENGTH;

   procedure SET_PAGE_LENGTH(TO : in COUNT) is
   begin
      SET_PAGE_LENGTH(FILE_TYPE(Current_Out_Idx), TO);
   end SET_PAGE_LENGTH;

   function LINE_LENGTH(FILE : in FILE_TYPE) return COUNT is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      return FCBs(Idx).Line_Length;
   end LINE_LENGTH;

   function LINE_LENGTH return COUNT is
   begin
      return LINE_LENGTH(FILE_TYPE(Current_Out_Idx));
   end LINE_LENGTH;

   function PAGE_LENGTH(FILE : in FILE_TYPE) return COUNT is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then raise MODE_ERROR; end if;
      return FCBs(Idx).Page_Length;
   end PAGE_LENGTH;

   function PAGE_LENGTH return COUNT is
   begin
      return PAGE_LENGTH(FILE_TYPE(Current_Out_Idx));
   end PAGE_LENGTH;

   -- Output character to file
   procedure Raw_Put(Idx : Integer; C : Integer) is
      Dummy : Integer;
   begin
      if FCBs(Idx).Stream /= Null_Address then
         Dummy := C_Fputc(C, FCBs(Idx).Stream);
      end if;
   end Raw_Put;

   -- Peek the character N positions ahead of the read cursor (0 = next),
   -- filling the pushback buffer from the stream as needed. End of file is
   -- reported as -1, and once reached every further position reads -1 too.
   function Peek_N(Idx : Integer; N : Integer) return Integer is
      C : Integer;
   begin
      while FCBs(Idx).Look_Count <= N loop
         if FCBs(Idx).Stream /= Null_Address then
            C := C_Fgetc(FCBs(Idx).Stream);
         else
            C := -1;
         end if;
         FCBs(Idx).Look(FCBs(Idx).Look_Count) := C;
         FCBs(Idx).Look_Count := FCBs(Idx).Look_Count + 1;
      end loop;
      return FCBs(Idx).Look(N);
   end Peek_N;

   -- Peek at the next character without consuming it.
   function Raw_Peek(Idx : Integer) return Integer is
   begin
      return Peek_N(Idx, 0);
   end Raw_Peek;

   -- Read and consume the next character from the file.
   function Raw_Get(Idx : Integer) return Integer is
      C : Integer;
   begin
      C := Peek_N(Idx, 0);
      for K in 1 .. FCBs(Idx).Look_Count - 1 loop
         FCBs(Idx).Look(K - 1) := FCBs(Idx).Look(K);
      end loop;
      if FCBs(Idx).Look_Count > 0 then
         FCBs(Idx).Look_Count := FCBs(Idx).Look_Count - 1;
      end if;
      return C;
   end Raw_Get;

   -- Line/Page Operations

   procedure NEW_LINE(FILE : in FILE_TYPE; SPACING : in POSITIVE_COUNT := 1) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      for I in 1 .. Integer(SPACING) loop
         Raw_Put(Idx, 10);  -- LF (line terminator)
         FCBs(Idx).Col := 1;
         FCBs(Idx).Line := FCBs(Idx).Line + 1;
         FCBs(Idx).Page_Active := True;  -- this page now holds a line
         if FCBs(Idx).Page_Length /= UNBOUNDED and
            FCBs(Idx).Line > FCBs(Idx).Page_Length then
            Raw_Put(Idx, 12);  -- FF (page terminator)
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
            FCBs(Idx).Page_Active := False;
         end if;
      end loop;
   end NEW_LINE;

   procedure NEW_LINE(SPACING : in POSITIVE_COUNT := 1) is
   begin
      NEW_LINE(FILE_TYPE(Current_Out_Idx), SPACING);
   end NEW_LINE;

   procedure SKIP_LINE(FILE : in FILE_TYPE; SPACING : in POSITIVE_COUNT := 1) is
      Idx : Integer := Integer(FILE);
      C : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      for I in 1 .. Integer(SPACING) loop
         -- Discard the rest of the current line up to and including its line
         -- terminator (LF). An unterminated final line ends at the file
         -- terminator; only an immediate file terminator is an error.
         declare
            Saw_Content : Boolean := False;
         begin
            loop
               C := Raw_Peek(Idx);
               if C < 0 then
                  if Saw_Content then exit; end if;
                  raise END_ERROR;
               end if;
               C := Raw_Get(Idx);
               exit when C = 10;
               Saw_Content := True;
            end loop;
         end;
         FCBs(Idx).Col := 1;
         FCBs(Idx).Line := FCBs(Idx).Line + 1;
         -- A page terminator immediately following the line terminator is
         -- skipped as well (RM 14.3.4).
         if Raw_Peek(Idx) = 12 then
            C := Raw_Get(Idx);
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
         end if;
      end loop;
   end SKIP_LINE;

   procedure SKIP_LINE(SPACING : in POSITIVE_COUNT := 1) is
   begin
      SKIP_LINE(FILE_TYPE(Current_In_Idx), SPACING);
   end SKIP_LINE;

   function END_OF_LINE(FILE : in FILE_TYPE) return BOOLEAN is
      Idx : Integer := Integer(FILE);
      C : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      C := Raw_Peek(Idx);
      return C < 0 or C = 10 or C = 12;  -- file/line/page terminator next
   end END_OF_LINE;

   function END_OF_LINE return BOOLEAN is
   begin
      return END_OF_LINE(FILE_TYPE(Current_In_Idx));
   end END_OF_LINE;

   procedure NEW_PAGE(FILE : in FILE_TYPE) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      -- A page terminator must be preceded by a line terminator, and a page
      -- holds at least one line (RM 14.3.4): terminate a partial current line,
      -- or supply an empty line if the page has none yet.
      if FCBs(Idx).Col /= 1 then
         NEW_LINE(FILE);
      elsif not FCBs(Idx).Page_Active then
         NEW_LINE(FILE);
      end if;
      Raw_Put(Idx, 12);  -- FF (page terminator)
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := FCBs(Idx).Page + 1;
      FCBs(Idx).Page_Active := False;
   end NEW_PAGE;

   procedure NEW_PAGE is
   begin
      NEW_PAGE(FILE_TYPE(Current_Out_Idx));
   end NEW_PAGE;

   procedure SKIP_PAGE(FILE : in FILE_TYPE) is
      Idx : Integer := Integer(FILE);
      C : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      -- Discard up to and including the next page terminator (FF). The last
      -- page may end at the file terminator without an explicit FF; only an
      -- immediate file terminator (no page left to skip) is END_ERROR.
      declare
         Saw_Content : Boolean := False;
      begin
         loop
            C := Raw_Peek(Idx);
            if C < 0 then
               if Saw_Content then exit; end if;
               raise END_ERROR;
            end if;
            C := Raw_Get(Idx);
            Saw_Content := True;
            exit when C = 12;
         end loop;
      end;
      FCBs(Idx).Col := 1;
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := FCBs(Idx).Page + 1;
   end SKIP_PAGE;

   procedure SKIP_PAGE is
   begin
      SKIP_PAGE(FILE_TYPE(Current_In_Idx));
   end SKIP_PAGE;

   function END_OF_PAGE(FILE : in FILE_TYPE) return BOOLEAN is
      Idx : Integer := Integer(FILE);
      C0, C1 : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      -- True when a page or file terminator is next, or a line terminator
      -- that is itself followed by a page or file terminator (RM 14.3.4).
      C0 := Peek_N(Idx, 0);
      if C0 < 0 or C0 = 12 then return True; end if;
      if C0 = 10 then
         C1 := Peek_N(Idx, 1);
         return C1 < 0 or C1 = 12;
      end if;
      return False;
   end END_OF_PAGE;

   function END_OF_PAGE return BOOLEAN is
   begin
      return END_OF_PAGE(FILE_TYPE(Current_In_Idx));
   end END_OF_PAGE;

   function END_OF_FILE(FILE : in FILE_TYPE) return BOOLEAN is
      Idx : Integer := Integer(FILE);
      C0, C1 : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      -- True at the file terminator, or when only terminators remain before
      -- it: EOF, LF EOF, FF EOF, or LF FF EOF (RM 14.3.10).
      C0 := Peek_N(Idx, 0);
      if C0 < 0 then return True; end if;
      if C0 = 12 then return Peek_N(Idx, 1) < 0; end if;
      if C0 = 10 then
         C1 := Peek_N(Idx, 1);
         if C1 < 0 then return True; end if;
         if C1 = 12 then return Peek_N(Idx, 2) < 0; end if;
      end if;
      return False;
   end END_OF_FILE;

   function END_OF_FILE return BOOLEAN is
   begin
      return END_OF_FILE(FILE_TYPE(Current_In_Idx));
   end END_OF_FILE;

   -- Column/Line/Page Position

   procedure SET_COL(FILE : in FILE_TYPE; TO : in POSITIVE_COUNT) is
      Idx : Integer := Integer(FILE);
      C : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then
         -- Input: skip characters until the column equals TO, but column TO
         -- must hold an actual character -- if the line is too short (TO falls
         -- on a line or page terminator) skip to the next line and keep
         -- looking; if already past TO, advance to the next line (RM 14.3.6).
         loop
            if FCBs(Idx).Col > TO then
               SKIP_LINE(FILE);
            elsif FCBs(Idx).Col = TO then
               C := Raw_Peek(Idx);
               exit when C /= 10 and C /= 12;  -- a real character sits at TO
               SKIP_LINE(FILE);                -- TO is a terminator: skip line
            else
               C := Raw_Peek(Idx);
               if C < 0 then raise END_ERROR; end if;
               if C = 10 or C = 12 then
                  SKIP_LINE(FILE);
               else
                  C := Raw_Get(Idx);
                  FCBs(Idx).Col := FCBs(Idx).Col + 1;
               end if;
            end if;
         end loop;
      else
         if FCBs(Idx).Line_Length /= UNBOUNDED and TO > FCBs(Idx).Line_Length then
            raise LAYOUT_ERROR;
         end if;
         if FCBs(Idx).Col > TO then
            NEW_LINE(FILE);
         end if;
         while FCBs(Idx).Col < TO loop
            Raw_Put(Idx, 32);  -- space
            FCBs(Idx).Col := FCBs(Idx).Col + 1;
         end loop;
      end if;
   end SET_COL;

   procedure SET_COL(TO : in POSITIVE_COUNT) is
   begin
      SET_COL(FILE_TYPE(Current_Out_Idx), TO);
   end SET_COL;

   procedure SET_LINE(FILE : in FILE_TYPE; TO : in POSITIVE_COUNT) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then
         -- Input: skip whole lines until the line equals TO; if already past
         -- it, advance to the next page first (RM 14.3.6).
         loop
            exit when FCBs(Idx).Line = TO;
            if FCBs(Idx).Line > TO then
               SKIP_PAGE(FILE);
            else
               SKIP_LINE(FILE);
            end if;
         end loop;
      else
         if FCBs(Idx).Page_Length /= UNBOUNDED and TO > FCBs(Idx).Page_Length then
            raise LAYOUT_ERROR;
         end if;
         if FCBs(Idx).Line > TO then
            NEW_PAGE(FILE);
         end if;
         while FCBs(Idx).Line < TO loop
            NEW_LINE(FILE);
         end loop;
      end if;
   end SET_LINE;

   procedure SET_LINE(TO : in POSITIVE_COUNT) is
   begin
      SET_LINE(FILE_TYPE(Current_Out_Idx), TO);
   end SET_LINE;

   function COL(FILE : in FILE_TYPE) return POSITIVE_COUNT is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      return FCBs(Idx).Col;
   end COL;

   function COL return POSITIVE_COUNT is
   begin
      return COL(FILE_TYPE(Current_Out_Idx));
   end COL;

   function LINE(FILE : in FILE_TYPE) return POSITIVE_COUNT is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      return FCBs(Idx).Line;
   end LINE;

   function LINE return POSITIVE_COUNT is
   begin
      return LINE(FILE_TYPE(Current_Out_Idx));
   end LINE;

   function PAGE(FILE : in FILE_TYPE) return POSITIVE_COUNT is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      return FCBs(Idx).Page;
   end PAGE;

   function PAGE return POSITIVE_COUNT is
   begin
      return PAGE(FILE_TYPE(Current_Out_Idx));
   end PAGE;

   -- Character I/O

   procedure GET(FILE : in FILE_TYPE; ITEM : out CHARACTER) is
      Idx : Integer := Integer(FILE);
      C : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      loop
         C := Raw_Get(Idx);
         if C < 0 then
            raise END_ERROR;
         end if;
         exit when C /= 10 and C /= 12 and C /= 13;
         if C = 10 then
            FCBs(Idx).Col := 1;
            FCBs(Idx).Line := FCBs(Idx).Line + 1;
         elsif C = 12 then
            FCBs(Idx).Col := 1;
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
         end if;
      end loop;
      ITEM := CHARACTER'VAL(C);
      FCBs(Idx).Col := FCBs(Idx).Col + 1;
   end GET;

   procedure GET(ITEM : out CHARACTER) is
   begin
      GET(FILE_TYPE(Current_In_Idx), ITEM);
   end GET;

   procedure PUT(FILE : in FILE_TYPE; ITEM : in CHARACTER) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      if FCBs(Idx).Line_Length /= UNBOUNDED and
         FCBs(Idx).Col > FCBs(Idx).Line_Length then
         NEW_LINE(FILE);
      end if;
      Raw_Put(Idx, CHARACTER'POS(ITEM));
      FCBs(Idx).Col := FCBs(Idx).Col + 1;
      FCBs(Idx).Page_Active := True;
   end PUT;

   procedure PUT(ITEM : in CHARACTER) is
   begin
      PUT(FILE_TYPE(Current_Out_Idx), ITEM);
   end PUT;

   -- String I/O

   procedure GET(FILE : in FILE_TYPE; ITEM : out STRING) is
   begin
      for I in ITEM'Range loop
         GET(FILE, ITEM(I));
      end loop;
   end GET;

   procedure GET(ITEM : out STRING) is
   begin
      GET(FILE_TYPE(Current_In_Idx), ITEM);
   end GET;

   procedure PUT(FILE : in FILE_TYPE; ITEM : in STRING) is
   begin
      for I in ITEM'Range loop
         PUT(FILE, ITEM(I));
      end loop;
   end PUT;

   procedure PUT(ITEM : in STRING) is
   begin
      PUT(FILE_TYPE(Current_Out_Idx), ITEM);
   end PUT;

   procedure GET_LINE(FILE : in FILE_TYPE; ITEM : out STRING; LAST : out NATURAL) is
      Idx : Integer := Integer(FILE);
      C : Integer;
      First_Idx : constant Integer := Integer(ITEM'First);
      Last_Idx  : constant Integer := Integer(ITEM'Last);
      I : Integer;
   begin
      Ensure_Init;
      Require_Open(Idx);
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      LAST := Natural(First_Idx - 1);
      I := First_Idx;
      while I <= Last_Idx loop
         C := Raw_Get(Idx);
         if C < 0 then
            if I = First_Idx then
               raise END_ERROR;
            end if;
            exit;
         end if;
         if C = 10 then
            FCBs(Idx).Col := 1;
            FCBs(Idx).Line := FCBs(Idx).Line + 1;
            exit;
         end if;
         if C = 13 then
            null;  -- skip CR
         elsif C = 12 then
            FCBs(Idx).Col := 1;
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
            exit;
         else
            ITEM(I) := CHARACTER'VAL(C);
            LAST := Natural(I);
            I := I + 1;
            FCBs(Idx).Col := FCBs(Idx).Col + 1;
         end if;
      end loop;
   end GET_LINE;

   procedure GET_LINE(ITEM : out STRING; LAST : out NATURAL) is
   begin
      GET_LINE(FILE_TYPE(Current_In_Idx), ITEM, LAST);
   end GET_LINE;

   procedure PUT_LINE(FILE : in FILE_TYPE; ITEM : in STRING) is
   begin
      PUT(FILE, ITEM);
      NEW_LINE(FILE);
   end PUT_LINE;

   procedure PUT_LINE(ITEM : in STRING) is
   begin
      PUT_LINE(FILE_TYPE(Current_Out_Idx), ITEM);
   end PUT_LINE;

   -- Shared field helpers for the numeric and enumeration IO generics.

   -- Skip the blanks, horizontal tabs, and line/page terminators that may
   -- precede a literal in an input field (RM 14.3.5).
   procedure Skip_Blanks_And_Terminators(Idx : Integer) is
      C : Integer;
   begin
      loop
         C := Raw_Peek(Idx);
         if C = 32 or C = 9 then
            C := Raw_Get(Idx);
            FCBs(Idx).Col := FCBs(Idx).Col + 1;
         elsif C = 10 then
            C := Raw_Get(Idx);
            FCBs(Idx).Col := 1;
            FCBs(Idx).Line := FCBs(Idx).Line + 1;
         elsif C = 12 then
            C := Raw_Get(Idx);
            FCBs(Idx).Col := 1;
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
         elsif C = 13 then
            C := Raw_Get(Idx);   -- ignore carriage return
         else
            exit;
         end if;
      end loop;
   end Skip_Blanks_And_Terminators;

   -- A digit in any base 2..16: 0-9 or A-F / a-f (RM 2.4.2).
   function Is_Extended_Digit(C : Integer) return Boolean is
   begin
      return (C >= 48 and C <= 57) or
             (C >= 65 and C <= 70) or
             (C >= 97 and C <= 102);
   end Is_Extended_Digit;

   -- Index of the last character of the numeric literal token beginning at
   -- First in S, following the integer (Allow_Point = False) or real
   -- (Allow_Point = True) grammar exactly as the file reader does. Returns
   -- First - 1 when no digit starts the token. The caller skips leading blanks
   -- and validates the result with NUM'VALUE.
   function Scan_String_Number(S : String; First : Integer;
                               Allow_Point : Boolean) return Integer is
      I : Integer := First;
      Hi : constant Integer := S'Last;

      function Is_Dig (Ch : Character) return Boolean is
      begin return Ch >= '0' and Ch <= '9'; end Is_Dig;

      function Is_Ext (Ch : Character) return Boolean is
      begin
         return (Ch >= '0' and Ch <= '9') or (Ch >= 'A' and Ch <= 'F')
             or (Ch >= 'a' and Ch <= 'f');
      end Is_Ext;

      procedure Skip_Digits (Got : out Boolean) is
         After : Boolean;
      begin
         if I > Hi or else not Is_Dig (S (I)) then Got := False; return; end if;
         Got := True; After := True; I := I + 1;
         while I <= Hi loop
            if Is_Dig (S (I)) then After := True; I := I + 1;
            elsif S (I) = '_' and then After then After := False; I := I + 1;
            else exit; end if;
         end loop;
      end Skip_Digits;

      procedure Skip_Extended is
         After : Boolean := False;
      begin
         while I <= Hi loop
            if Is_Ext (S (I)) then After := True; I := I + 1;
            elsif S (I) = '_' and then After then After := False; I := I + 1;
            else exit; end if;
         end loop;
      end Skip_Extended;

      procedure Match (C1, C2 : Character; Got : out Boolean) is
      begin
         if I <= Hi and then (S (I) = C1 or S (I) = C2) then
            I := I + 1; Got := True;
         else
            Got := False;
         end if;
      end Match;

      Got, Based, Dot1, Dot2, Has_E, Frac : Boolean;
   begin
      Match ('+', '-', Got);                         -- optional sign

      -- A real literal requires an integer part before the point (RM 2.4.1);
      -- the ".nnn" form is not a real literal, so a leading point ends the
      -- token immediately with no characters consumed.
      Skip_Digits (Got);
      if not Got then return First - 1; end if;

      Match ('#', ':', Based);
      if Based then
         if Allow_Point then
            Match ('.', '.', Dot1);
            if Dot1 then
               Skip_Extended; Match ('#', ':', Got);
            else
               Skip_Extended; Match ('.', '.', Dot2);
               if Dot2 then Skip_Extended; end if;
               Match ('#', ':', Got);
            end if;
         else
            Skip_Extended; Match ('#', ':', Got);
         end if;
         Match ('E', 'e', Has_E);                    -- exponent on a based literal
         if Has_E then Match ('+', '-', Got); Skip_Digits (Got); end if;
      elsif Allow_Point then
         -- Decimal real literal: the exponent belongs to the literal only after
         -- a complete "nnn.nnn" mantissa (RM 2.4.1).
         if I > First and then S (I - 1) /= '_' then
            Match ('.', '.', Dot1);
            if Dot1 then
               Skip_Digits (Frac);
               if Frac then
                  Match ('E', 'e', Has_E);
                  if Has_E then Match ('+', '-', Got); Skip_Digits (Got); end if;
               end if;
            end if;
         end if;
      else
         Match ('E', 'e', Has_E);                    -- decimal integer exponent
         if Has_E then Match ('+', '-', Got); Skip_Digits (Got); end if;
      end if;

      return I - 1;
   end Scan_String_Number;

   -- Read one numeric literal token from a file into Buf (RM 14.3.5),
   -- following the grammar of an integer literal (Allow_Point = False) or a
   -- real literal (Allow_Point = True) one character at a time, exactly as
   -- GNAT's Ada.Text_IO.Generic_Aux.Load_Integer / Load_Real do. Only the
   -- characters that can continue the literal are consumed, so the trailing
   -- delimiter is left unread; validity (consecutive/edge underscores, base
   -- range, closing '#', and -- for integers -- a forbidden point) is decided
   -- by the caller's NUM'VALUE on the returned text. When Width > 0 exactly
   -- Width characters are taken (up to a line terminator).
   procedure Read_Number_Token(Idx : Integer; Width : Integer;
                               Allow_Point : Boolean;
                               Buf : out String; Len : out Integer) is
      Local : String (1 .. 256);
      P : Integer := 0;
      Loaded, Based, Dot1, Dot2, Has_E, Frac : Boolean;
      Hash_Char : Integer;
      Cc : Integer;

      procedure Store (Ch : Integer) is
      begin
         if P < Local'Last then P := P + 1; Local (P) := Character'Val (Ch); end if;
         FCBs(Idx).Col := FCBs(Idx).Col + 1;
      end Store;

      -- Consume the next character if it is C1 or (when C2 >= 0) C2.
      procedure Load_Char (C1, C2 : Integer; Got : out Boolean) is
         Ch : Integer := Raw_Peek (Idx);
      begin
         if Ch = C1 or (C2 >= 0 and Ch = C2) then
            Cc := Raw_Get (Idx); Store (Cc); Got := True;
         else
            Got := False;
         end if;
      end Load_Char;

      -- Decimal digits with embedded single underscores (each between digits).
      procedure Load_Digits (Got : out Boolean) is
         Ch : Integer := Raw_Peek (Idx);
         After_Digit : Boolean;
      begin
         if Ch >= 48 and Ch <= 57 then
            Got := True; After_Digit := True;
            Cc := Raw_Get (Idx); Store (Cc);
            loop
               Ch := Raw_Peek (Idx);
               if Ch >= 48 and Ch <= 57 then
                  After_Digit := True; Cc := Raw_Get (Idx); Store (Cc);
               elsif Ch = 95 and After_Digit then
                  After_Digit := False; Cc := Raw_Get (Idx); Store (Cc);
               else
                  exit;
               end if;
            end loop;
         else
            Got := False;
         end if;
      end Load_Digits;

      -- Extended (base 2..16) digits with embedded single underscores.
      procedure Load_Extended is
         Ch : Integer;
         After_Digit : Boolean := False;
      begin
         loop
            Ch := Raw_Peek (Idx);
            if (Ch >= 48 and Ch <= 57) or (Ch >= 65 and Ch <= 70)
               or (Ch >= 97 and Ch <= 102) then
               After_Digit := True;
            elsif Ch = 95 and After_Digit then
               After_Digit := False;
            else
               exit;
            end if;
            Cc := Raw_Get (Idx); Store (Cc);
         end loop;
      end Load_Extended;
   begin
      if Width > 0 then
         -- A field positioned right at a line or page terminator yields no
         -- characters but is a malformed value, not end of file: Data_Error,
         -- not End_Error (RM 14.3.5, the GNAT Before_LM rule).
         Cc := Raw_Peek (Idx);
         if Cc = 10 or Cc = 12 then raise DATA_ERROR; end if;
         -- Otherwise take exactly Width characters, stopping before a line or
         -- page terminator; the field is validated by the caller's NUM'VALUE.
         for I in 1 .. Width loop
            Cc := Raw_Peek (Idx);
            exit when Cc < 0 or Cc = 10 or Cc = 12;
            Cc := Raw_Get (Idx); Store (Cc);
         end loop;
         -- The field is consumed, but blanks and horizontal tabs that bound it
         -- are not part of the value (RM 14.3.5): trim them before the caller's
         -- NUM'VALUE so a tab- or blank-padded field reads its number.
         declare
            Lo : Integer := 1;
            Hi : Integer := P;
         begin
            while Lo <= Hi and then (Local (Lo) = ' ' or Local (Lo) = Character'Val (9)) loop
               Lo := Lo + 1;
            end loop;
            while Hi >= Lo and then (Local (Hi) = ' ' or Local (Hi) = Character'Val (9)) loop
               Hi := Hi - 1;
            end loop;
            Len := Hi - Lo + 1;
            if Len > 0 then Buf (Buf'First .. Buf'First + Len - 1) := Local (Lo .. Hi); end if;
         end;
         return;
      end if;

      Skip_Blanks_And_Terminators (Idx);
      Load_Char (43, 45, Loaded);          -- optional leading sign

      -- A real literal requires an integer part before the point (RM 2.4.1);
      -- the ".nnn" form is not accepted, so a leading point stops the token at
      -- once and leaves the cursor on it (the value is then malformed).
      Load_Digits (Loaded);
      if Loaded then
         Load_Char (35, 58, Based);        -- based literal: '#' or ':'
         if Based then
            Hash_Char := Character'Pos (Local (P));
            if Allow_Point then
               Load_Char (46, -1, Dot1);   -- nnn#.xxx#
               if Dot1 then
                  Load_Extended;
                  Load_Char (35, 58, Loaded);
               else
                  Load_Extended;
                  Load_Char (46, -1, Dot2);
                  if Dot2 then Load_Extended; end if;
                  Load_Char (35, 58, Loaded);   -- mixed base char allowed (RM J.2)
               end if;
            else
               Load_Extended;
               Load_Char (Hash_Char, -1, Loaded);  -- integer: closing must match
            end if;
            -- A based literal (integer or real) may carry an exponent.
            Load_Char (69, 101, Has_E);
            if Has_E then
               Load_Char (43, 45, Loaded);
               Load_Digits (Loaded);
            end if;
         elsif Allow_Point then
            -- Decimal real literal: the exponent is part of the literal only
            -- when a complete "nnn.nnn" mantissa precedes it (RM 2.4.1). A
            -- missing point ("nnnE..") or empty fraction ("nnn.E..") stops the
            -- token there, leaving the offending character unread.
            if Local (P) /= '_' then
               Load_Char (46, -1, Dot1);
               if Dot1 then
                  Load_Digits (Frac);
                  if Frac then
                     Load_Char (69, 101, Has_E);
                     if Has_E then
                        Load_Char (43, 45, Loaded);
                        Load_Digits (Loaded);
                     end if;
                  end if;
               end if;
            end if;
         else
            -- Decimal integer literal: an exponent is permitted.
            Load_Char (69, 101, Has_E);
            if Has_E then
               Load_Char (43, 45, Loaded);
               Load_Digits (Loaded);
            end if;
         end if;
      end if;

      Len := P;
      if P > 0 then Buf (Buf'First .. Buf'First + P - 1) := Local (1 .. P); end if;
   end Read_Number_Token;

   -- The image S with IMAGE's leading sign-place blank removed.
   function Without_Leading_Blank(S : String) return String is
   begin
      if S'Length > 0 and then S(S'First) = ' ' then
         return S(S'First + 1 .. S'Last);
      end if;
      return S;
   end Without_Leading_Blank;

   -- Emit Item right-justified in a field of at least Width columns, padding
   -- on the left with blanks (numeric PUT, RM 14.3.5).
   -- Before emitting a self-contained item of the given length, enforce the
   -- bounded line length (RM 14.3.5): the item must fit on a line at all, and
   -- if it will not fit in what remains of the current line, start a new one.
   procedure Check_On_One_Line(FILE : FILE_TYPE; Length : Integer) is
      Idx : Integer := Integer(FILE);
   begin
      if FCBs(Idx).Line_Length /= UNBOUNDED then
         if COUNT(Length) > FCBs(Idx).Line_Length then
            raise LAYOUT_ERROR;
         elsif FCBs(Idx).Col + COUNT(Length) > FCBs(Idx).Line_Length + 1 then
            NEW_LINE(FILE);
         end if;
      end if;
   end Check_On_One_Line;

   -- Emit Item verbatim, without the per-character line-length wrapping that
   -- PUT applies; the caller has already validated the item fits (RM 14.3.5).
   procedure Put_Raw(FILE : FILE_TYPE; Item : String) is
      Idx : Integer := Integer(FILE);
   begin
      for I in Item'Range loop
         Raw_Put(Idx, Character'Pos(Item(I)));
         FCBs(Idx).Col := FCBs(Idx).Col + 1;
         FCBs(Idx).Page_Active := True;
      end loop;
   end Put_Raw;

   -- Emit Pad blank columns verbatim.
   procedure Put_Blanks(FILE : FILE_TYPE; Pad : Integer) is
      Idx : Integer := Integer(FILE);
   begin
      for I in 1 .. Pad loop
         Raw_Put(Idx, 32);
         FCBs(Idx).Col := FCBs(Idx).Col + 1;
         FCBs(Idx).Page_Active := True;
      end loop;
   end Put_Blanks;

   -- Emit Item right-justified in a field of at least Width columns, padding
   -- on the left with blanks (numeric PUT, RM 14.3.5).
   procedure Put_Right_Justified(FILE : FILE_TYPE; Item : String;
                                 Width : Integer) is
      Pad : Integer := Width - Item'Length;
   begin
      if Pad < 0 then Pad := 0; end if;
      Check_On_One_Line(FILE, Pad + Item'Length);
      Put_Blanks(FILE, Pad);
      Put_Raw(FILE, Item);
   end Put_Right_Justified;

   -- Emit Item left-justified in a field of at least Width columns, padding
   -- on the right with blanks (enumeration PUT, RM 14.3.9).
   procedure Put_Left_Justified(FILE : FILE_TYPE; Item : String;
                                Width : Integer) is
      Pad : Integer := Width - Item'Length;
   begin
      if Pad < 0 then Pad := 0; end if;
      Check_On_One_Line(FILE, Item'Length + Pad);
      Put_Raw(FILE, Item);
      Put_Blanks(FILE, Pad);
   end Put_Left_Justified;

   function Blanks(Count : Integer) return String is
   begin
      if Count <= 0 then return ""; end if;
      declare
         Pad : String(1 .. Count) := (others => ' ');
      begin
         return Pad;
      end;
   end Blanks;

   -- N's decimal digits, zero-padded on the left to at least Width.
   function Padded_Digits(N : Integer; Width : Integer) return String is
      Img : constant String := Without_Leading_Blank (INTEGER'IMAGE (N));
   begin
      if Img'Length >= Width then return Img; end if;
      declare
         Zeros : String (1 .. Width - Img'Length) := (others => '0');
      begin
         return Zeros & Img;
      end;
   end Padded_Digits;

   -- The integer part of a value in [0, 10): its leading decimal digit. Uses
   -- round-to-nearest INTEGER conversion then corrects down to the floor.
   -- A real value in FORE.AFT[E+/-EXP] layout (RM 14.3.8). Decimal digits are
   -- extracted one at a time after normalizing the magnitude to [1, 10), so no
   -- intermediate scaled integer can overflow regardless of AFT; the last kept
   -- digit is rounded half-up using one guard digit. FORE is the minimum width
   -- (sign + integer digits), left-padded with blanks; the exponent carries an
   -- explicit sign and at least EXP digits.
   function Render_Real(Val : FLOAT; Fore, Aft, Exp : Integer) return String is
      Max_Digits : constant Integer := 128;
      Big_Max    : constant Integer := 800;
      type Mantissa_Int is range 0 .. 2 ** 53;
      Neg  : Boolean := Val < 0.0;
      M    : FLOAT := abs (Val);
      DE   : Integer := 0;             -- value is D(0).D(1)D(2)... * 10**DE
      D    : array (0 .. Max_Digits) of Integer := (others => 0);
      Nsig : Integer;
      -- A real image always shows at least one fractional digit, so AFT 0
      -- means 1 (RM 14.3.8).
      EAft : Integer := Aft;
      Guard, Carry, J, Int_Count, Sign_Len, Idx : Integer;
      Buf  : String (1 .. 2 * Max_Digits);
      P    : Integer := 0;
      -- Exact decimal expansion. A binary float is Mant * 2**E with Mant and E
      -- integers, so its exact value is Mant * 5**(-E) scaled by 10**E when
      -- E < 0, or Mant * 2**E when E >= 0 -- always a terminating decimal. The
      -- digits are accumulated in BD (base 10, least significant first) with PP
      -- digits after the decimal point, giving every significant digit exactly
      -- rather than the ~16 a double can carry.
      Mant  : Mantissa_Int := 0;
      E     : Integer := 0;
      BD    : array (0 .. Big_Max) of Integer := (others => 0);
      BDLen : Integer := 1;
      PP    : Integer := 0;
      Two52 : constant FLOAT := 2.0 ** 52;
      Two53 : constant FLOAT := 2.0 ** 53;
   begin
      if M /= 0.0 then
         -- Decompose |Val| into Mant * 2**E with Mant in [2**52, 2**53), an
         -- exact integer; then strip trailing zero bits to minimize the work.
         while M < Two52 loop M := M * 2.0; E := E - 1; end loop;
         while M >= Two53 loop M := M / 2.0; E := E + 1; end loop;
         Mant := Mantissa_Int (M);
         while Mant mod 2 = 0 and Mant > 0 loop Mant := Mant / 2; E := E + 1; end loop;

         -- BD := Mant in base 10, least significant digit first.
         BDLen := 0;
         declare
            T : Mantissa_Int := Mant;
         begin
            while T > 0 loop
               BD (BDLen) := Integer (T mod 10); BDLen := BDLen + 1; T := T / 10;
            end loop;
         end;

         -- Multiply BD by 2**E (E >= 0) or 5**(-E) (E < 0). For E < 0 the
         -- division by 2**(-E) becomes a decimal point PP = -E places in.
         declare
            Factor : Integer := 2;
            Reps   : Integer := E;
         begin
            if E < 0 then Factor := 5; Reps := -E; PP := -E; end if;
            for R in 1 .. Reps loop
               Carry := 0;
               for I in 0 .. BDLen - 1 loop
                  declare
                     T : Integer := BD (I) * Factor + Carry;
                  begin
                     BD (I) := T mod 10; Carry := T / 10;
                  end;
               end loop;
               while Carry > 0 loop
                  BD (BDLen) := Carry mod 10; Carry := Carry / 10; BDLen := BDLen + 1;
               end loop;
            end loop;
         end;
         -- The most significant digit BD(BDLen-1) has place value 10**DE.
         DE := BDLen - 1 - PP;
      end if;

      if EAft < 1 then EAft := 1; end if;
      -- Significant digits to extract.
      if Exp > 0 then
         Nsig := EAft + 1;
      elsif DE >= 0 then
         Nsig := DE + 1 + EAft;
      else
         Nsig := EAft + DE + 1;
      end if;
      if Nsig < 0 then Nsig := 0; end if;
      if Nsig > Max_Digits then Nsig := Max_Digits; end if;

      -- Take the Nsig leading significant digits from the exact expansion,
      -- most significant first (BD is least significant first); positions past
      -- the value's last digit are exact zeros. The guard digit is the next
      -- one, which alone decides half-up rounding for a terminating decimal.
      for I in 0 .. Nsig - 1 loop
         if I < BDLen then D (I) := BD (BDLen - 1 - I); else D (I) := 0; end if;
      end loop;
      if Nsig < BDLen then Guard := BD (BDLen - 1 - Nsig); else Guard := 0; end if;

      -- Round half-up on the guard digit, propagating any carry.
      if Guard >= 5 then
         Carry := 1; J := Nsig - 1;
         while Carry = 1 and J >= 0 loop
            D (J) := D (J) + 1;
            if D (J) >= 10 then D (J) := 0; else Carry := 0; end if;
            J := J - 1;
         end loop;
         if Carry = 1 then              -- carry past the most significant digit
            for K in reverse 1 .. Nsig - 1 loop D (K) := D (K - 1); end loop;
            if Nsig > 0 then D (0) := 1; end if;
            DE := DE + 1;
         end if;
      end if;

      if Exp > 0 then
         Int_Count := 1;
      elsif DE >= 0 then
         Int_Count := DE + 1;
      else
         Int_Count := 1;                -- the single integer digit 0
      end if;
      Sign_Len := 0;
      if Neg then Sign_Len := 1; end if;

      -- FORE blanks, then sign, then the integer digits.
      for I in Int_Count + Sign_Len + 1 .. Fore loop
         P := P + 1; Buf (P) := ' ';
      end loop;
      if Neg then P := P + 1; Buf (P) := '-'; end if;
      if Exp = 0 and DE < 0 then
         P := P + 1; Buf (P) := '0';
      else
         for I in 0 .. Int_Count - 1 loop
            P := P + 1; Buf (P) := Character'Val (48 + D (I));
         end loop;
      end if;

      -- Radix point and the AFT fractional digits. Digit at fractional
      -- position p (10**-p) is D(Pos0 + p), where Pos0 indexes the units
      -- digit (0 for scientific, DE for fixed); out-of-range indices are 0.
      P := P + 1; Buf (P) := '.';
      declare
         Pos0 : Integer := 0;
      begin
         if Exp = 0 then Pos0 := DE; end if;
         for Pp in 1 .. EAft loop
            Idx := Pos0 + Pp;
            P := P + 1;
            if Idx >= 0 and Idx <= Max_Digits then
               Buf (P) := Character'Val (48 + D (Idx));
            else
               Buf (P) := '0';
            end if;
         end loop;
      end;

      if Exp > 0 then
         P := P + 1; Buf (P) := 'E';
         if DE < 0 then P := P + 1; Buf (P) := '-'; DE := -DE;
         else P := P + 1; Buf (P) := '+'; end if;
         -- Exponent magnitude, at least Exp digits, leading zeros first.
         declare
            E_Dig : array (0 .. 15) of Integer := (others => 0);
            E_N : Integer := 0;
            V : Integer := DE;
         begin
            if V = 0 then
               E_N := 1;
            else
               while V > 0 loop E_Dig (E_N) := V mod 10; E_N := E_N + 1; V := V / 10; end loop;
            end if;
            for I in E_N + 1 .. Exp loop P := P + 1; Buf (P) := '0'; end loop;
            for I in reverse 0 .. E_N - 1 loop
               P := P + 1; Buf (P) := Character'Val (48 + E_Dig (I));
            end loop;
         end;
      end if;

      return Buf (1 .. P);
   end Render_Real;

   -- Emit a real value in FORE.AFT[E+/-EXP] layout to a file (RM 14.3.8),
   -- as a single item subject to the bounded line length.
   procedure Format_Real(FILE : FILE_TYPE; Val : FLOAT;
                         Fore, Aft, Exp : Integer) is
      Img : constant String := Render_Real (Val, Fore, Aft, Exp);
   begin
      Check_On_One_Line (FILE, Img'Length);
      Put_Raw (FILE, Img);
   end Format_Real;

   -- Digit value of an extended digit (0-9, A-F, a-f), or -1.
   function Hex_Digit_Value(C : Character) return Integer is
   begin
      if C >= '0' and C <= '9' then return Character'Pos(C) - 48;
      elsif C >= 'A' and C <= 'F' then return Character'Pos(C) - 55;
      elsif C >= 'a' and C <= 'f' then return Character'Pos(C) - 87;
      else return -1; end if;
   end Hex_Digit_Value;

   -- Evaluate a based real literal base#mantissa.frac#[E exp] (RM 2.4.2), whose
   -- value is (mantissa.frac in base) * base**exp. strtod (used by FLOAT'VALUE)
   -- cannot parse these, so they are handled here. Raises CONSTRAINT_ERROR on
   -- any malformed input -- base out of 2..16, a digit outside the base, a
   -- mismatched or missing closing marker, misplaced underscores, or trailing
   -- junk -- which the caller turns into DATA_ERROR.
   function Parse_Based_Real(S : String) return FLOAT is
      I : Integer := S'First;
      Last : Integer := S'Last;
      Neg : Boolean := False;
      Base : Integer := 0;
      Base_F : FLOAT;
      Mant : FLOAT := 0.0;
      Scale : FLOAT;
      Marker : Character;
      D : Integer;
      Exp : Integer := 0;
      Exp_Neg : Boolean := False;
      After : Boolean;
   begin
      while I <= Last and then (S(I) = ' ' or S(I) = Character'Val(9)) loop I := I + 1; end loop;
      while Last >= I and then (S(Last) = ' ' or S(Last) = Character'Val(9)) loop Last := Last - 1; end loop;
      if I > Last then raise CONSTRAINT_ERROR; end if;
      if S(I) = '+' then I := I + 1; elsif S(I) = '-' then Neg := True; I := I + 1; end if;

      if I > Last or else not (S(I) >= '0' and S(I) <= '9') then raise CONSTRAINT_ERROR; end if;
      while I <= Last and then (S(I) >= '0' and S(I) <= '9') loop
         Base := Base * 10 + (Character'Pos(S(I)) - 48); I := I + 1;
      end loop;
      if Base < 2 or Base > 16 then raise CONSTRAINT_ERROR; end if;
      Base_F := FLOAT(Base);
      if I > Last then raise CONSTRAINT_ERROR; end if;
      Marker := S(I);
      if Marker /= '#' and Marker /= ':' then raise CONSTRAINT_ERROR; end if;
      I := I + 1;

      After := False;
      loop
         exit when I > Last;
         D := Hex_Digit_Value(S(I));
         if D >= 0 then
            if D >= Base then raise CONSTRAINT_ERROR; end if;
            Mant := Mant * Base_F + FLOAT(D); After := True; I := I + 1;
         elsif S(I) = '_' and After then After := False; I := I + 1;
         else exit; end if;
      end loop;
      if not After then raise CONSTRAINT_ERROR; end if;

      if I <= Last and then S(I) = '.' then
         I := I + 1; Scale := 1.0; After := False;
         loop
            exit when I > Last;
            D := Hex_Digit_Value(S(I));
            if D >= 0 then
               if D >= Base then raise CONSTRAINT_ERROR; end if;
               Scale := Scale / Base_F; Mant := Mant + FLOAT(D) * Scale;
               After := True; I := I + 1;
            elsif S(I) = '_' and After then After := False; I := I + 1;
            else exit; end if;
         end loop;
         if not After then raise CONSTRAINT_ERROR; end if;
      end if;

      if I > Last or else S(I) /= Marker then raise CONSTRAINT_ERROR; end if;  -- matching close
      I := I + 1;

      if I <= Last and then (S(I) = 'E' or S(I) = 'e') then
         I := I + 1;
         if I <= Last and then S(I) = '+' then I := I + 1;
         elsif I <= Last and then S(I) = '-' then Exp_Neg := True; I := I + 1; end if;
         After := False;
         loop
            exit when I > Last;
            if S(I) >= '0' and S(I) <= '9' then
               Exp := Exp * 10 + (Character'Pos(S(I)) - 48); After := True; I := I + 1;
            elsif S(I) = '_' and After then After := False; I := I + 1;
            else exit; end if;
         end loop;
         if not After then raise CONSTRAINT_ERROR; end if;
      end if;

      if I <= Last then raise CONSTRAINT_ERROR; end if;  -- trailing junk

      declare
         P : FLOAT := 1.0;
      begin
         for K in 1 .. Exp loop P := P * Base_F; end loop;
         if Exp_Neg then Mant := Mant / P; else Mant := Mant * P; end if;
      end;
      if Neg then Mant := -Mant; end if;
      return Mant;
   end Parse_Based_Real;

   -- A real value from its literal text: based literals are evaluated here,
   -- ordinary decimal literals by FLOAT'VALUE.
   function Real_Value(S : String) return FLOAT is
      Dot : Integer := 0;
   begin
      for I in S'Range loop
         if S(I) = '#' or S(I) = ':' then return Parse_Based_Real(S); end if;
      end loop;
      -- A decimal real literal must contain a point flanked by digits
      -- (RM 2.4.1); a value with no point, or a point not preceded and
      -- followed by a digit, is malformed input for a real GET.
      for I in S'Range loop
         if S(I) = '.' then Dot := I; exit; end if;
      end loop;
      if Dot = 0
         or else Dot = S'First or else S(Dot - 1) < '0' or else S(Dot - 1) > '9'
         or else Dot = S'Last or else S(Dot + 1) < '0' or else S(Dot + 1) > '9'
      then
         raise DATA_ERROR;
      end if;
      return FLOAT'VALUE(S);
   end Real_Value;

   package body INTEGER_IO is

      -- The decimal image without IMAGE's leading sign-place blank, or a
      -- based literal base#digits# for non-decimal BASE (RM 14.3.7). The
      -- magnitude is built digit-by-digit toward zero from the value's own
      -- sign, so NUM'FIRST never overflows through negation.
      function Image_In_Base(Item : NUM; Base : NUMBER_BASE) return String is
         Digit_Set : constant String := "0123456789ABCDEF";
         Tmp : String(1 .. 130);
         T   : Integer := 0;
         W   : NUM := Item;
         D   : Integer;
      begin
         if Base = 10 then
            return Without_Leading_Blank(NUM'IMAGE(Item));
         end if;
         if W = 0 then
            T := 1; Tmp(1) := '0';
         else
            while W /= 0 loop
               D := Integer(W rem NUM(Base));
               if D < 0 then D := -D; end if;
               T := T + 1;
               Tmp(T) := Digit_Set(D + 1);
               W := W / NUM(Base);
            end loop;
         end if;
         declare
            Base_Img : constant String :=
               Without_Leading_Blank(INTEGER'IMAGE(Integer(Base)));
            Neg : Integer := 0;
         begin
            if Item < 0 then Neg := 1; end if;
            declare
               Result : String(1 .. Neg + Base_Img'Length + 2 + T);
               P : Integer := 1;
            begin
               if Neg = 1 then Result(P) := '-'; P := P + 1; end if;
               for I in Base_Img'Range loop
                  Result(P) := Base_Img(I); P := P + 1;
               end loop;
               Result(P) := '#'; P := P + 1;
               for I in reverse 1 .. T loop
                  Result(P) := Tmp(I); P := P + 1;
               end loop;
               Result(P) := '#';
               return Result;
            end;
         end;
      end Image_In_Base;

      procedure GET(FILE : in FILE_TYPE; ITEM : out NUM; WIDTH : in FIELD := 0) is
         Idx : Integer := Integer(FILE);
         Buf : String(1 .. 256);
         Len : Integer;
      begin
         Ensure_Init;
         Require_Open(Idx);
         if FCBs(Idx).Mode /= IN_FILE then raise MODE_ERROR; end if;
         Read_Number_Token(Idx, Integer(WIDTH), False, Buf, Len);
         -- No characters: end of file if truly at the file terminator,
         -- otherwise a non-numeric lexical element, i.e. Data_Error (RM 14.3.5).
         if Len = 0 then
            if Raw_Peek(Idx) < 0 then raise END_ERROR; else raise DATA_ERROR; end if;
         end if;
         declare
            V : NUM;
         begin
            V := NUM'VALUE(Buf(1 .. Len));
            -- A value outside the subtype is a Data_Error, not the
            -- Constraint_Error the out-parameter copy-back would raise after
            -- this body returns (RM 14.3.5).
            if V < NUM'FIRST or V > NUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
      end GET;

      procedure GET(ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         GET(FILE_TYPE(Current_In_Idx), ITEM, WIDTH);
      end GET;

      procedure PUT(FILE : in FILE_TYPE; ITEM : in NUM;
                    WIDTH : in FIELD := DEFAULT_WIDTH;
                    BASE : in NUMBER_BASE := DEFAULT_BASE) is
      begin
         Ensure_Init;
         Require_Open(Integer(FILE));
         if FCBs(Integer(FILE)).Mode = IN_FILE then raise MODE_ERROR; end if;
         Put_Right_Justified(FILE, Image_In_Base(ITEM, BASE), Integer(WIDTH));
      end PUT;

      procedure PUT(ITEM : in NUM; WIDTH : in FIELD := DEFAULT_WIDTH;
                    BASE : in NUMBER_BASE := DEFAULT_BASE) is
      begin
         PUT(FILE_TYPE(Current_Out_Idx), ITEM, WIDTH, BASE);
      end PUT;

      procedure GET(FROM : in STRING; ITEM : out NUM; LAST : out POSITIVE) is
         I : Integer := FROM'First;
         Token_Start, Token_End : Integer;
      begin
         while I <= FROM'Last and then
               (FROM(I) = ' ' or FROM(I) = Character'Val(9)) loop
            I := I + 1;
         end loop;
         Token_Start := I;
         -- Only blanks (or an empty/null string): nothing to read (RM 14.3.5).
         if Token_Start > FROM'Last then raise END_ERROR; end if;
         Token_End := Scan_String_Number(FROM, Token_Start, False);
         declare
            V : NUM;
         begin
            V := NUM'VALUE(FROM(Token_Start .. Token_End));
            if V < NUM'FIRST or V > NUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
         LAST := Token_End;
      end GET;

      procedure PUT(TO : out STRING; ITEM : in NUM;
                    BASE : in NUMBER_BASE := DEFAULT_BASE) is
         Img : constant String := Image_In_Base(ITEM, BASE);
      begin
         if Img'Length > TO'Length then raise LAYOUT_ERROR; end if;
         for I in TO'Range loop TO(I) := ' '; end loop;
         TO(TO'Last - Img'Length + 1 .. TO'Last) := Img;
      end PUT;
   end INTEGER_IO;

   package body FLOAT_IO is
      procedure GET(FILE : in FILE_TYPE; ITEM : out NUM; WIDTH : in FIELD := 0) is
         Idx : Integer := Integer(FILE);
         Buf : String(1 .. 256);
         Len : Integer;
      begin
         Ensure_Init;
         Require_Open(Idx);
         if FCBs(Idx).Mode /= IN_FILE then raise MODE_ERROR; end if;
         Read_Number_Token(Idx, Integer(WIDTH), True, Buf, Len);
         -- No characters: end of file if truly at the file terminator,
         -- otherwise a non-numeric lexical element, i.e. Data_Error (RM 14.3.5).
         if Len = 0 then
            if Raw_Peek(Idx) < 0 then raise END_ERROR; else raise DATA_ERROR; end if;
         end if;
         declare
            V : NUM;
         begin
            V := NUM(Real_Value(Buf(1 .. Len)));
            if V < NUM'FIRST or V > NUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
      end GET;

      procedure GET(ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         GET(FILE_TYPE(Current_In_Idx), ITEM, WIDTH);
      end GET;

      procedure PUT(FILE : in FILE_TYPE; ITEM : in NUM;
                    FORE : in FIELD := DEFAULT_FORE; AFT : in FIELD := DEFAULT_AFT;
                    EXP : in FIELD := DEFAULT_EXP) is
      begin
         Ensure_Init;
         Require_Open(Integer(FILE));
         if FCBs(Integer(FILE)).Mode = IN_FILE then raise MODE_ERROR; end if;
         Format_Real(FILE, FLOAT(ITEM), Integer(FORE), Integer(AFT), Integer(EXP));
      end PUT;

      procedure PUT(ITEM : in NUM; FORE : in FIELD := DEFAULT_FORE;
                    AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         PUT(FILE_TYPE(Current_Out_Idx), ITEM, FORE, AFT, EXP);
      end PUT;

      procedure GET(FROM : in STRING; ITEM : out NUM; LAST : out POSITIVE) is
         I : Integer := FROM'First;
         Token_Start, Token_End : Integer;
      begin
         while I <= FROM'Last and then
               (FROM(I) = ' ' or FROM(I) = Character'Val(9)) loop
            I := I + 1;
         end loop;
         Token_Start := I;
         -- Only blanks (or an empty/null string): nothing to read (RM 14.3.5).
         if Token_Start > FROM'Last then raise END_ERROR; end if;
         Token_End := Scan_String_Number(FROM, Token_Start, True);
         declare
            V : NUM;
         begin
            V := NUM(Real_Value(FROM(Token_Start .. Token_End)));
            if V < NUM'FIRST or V > NUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
         LAST := Token_End;
      end GET;

      procedure PUT(TO : out STRING; ITEM : in NUM; AFT : in FIELD := DEFAULT_AFT;
                    EXP : in FIELD := DEFAULT_EXP) is
         Img : constant String := Render_Real(FLOAT(ITEM), 1, Integer(AFT), Integer(EXP));
      begin
         if Img'Length > TO'Length then raise LAYOUT_ERROR; end if;
         for I in TO'Range loop TO(I) := ' '; end loop;
         TO(TO'Last - Img'Length + 1 .. TO'Last) := Img;
      end PUT;
   end FLOAT_IO;

   package body FIXED_IO is
      procedure GET(FILE : in FILE_TYPE; ITEM : out NUM; WIDTH : in FIELD := 0) is
         Idx : Integer := Integer(FILE);
         Buf : String(1 .. 256);
         Len : Integer;
      begin
         Ensure_Init;
         Require_Open(Idx);
         if FCBs(Idx).Mode /= IN_FILE then raise MODE_ERROR; end if;
         Read_Number_Token(Idx, Integer(WIDTH), True, Buf, Len);
         -- No characters: end of file if truly at the file terminator,
         -- otherwise a non-numeric lexical element, i.e. Data_Error (RM 14.3.5).
         if Len = 0 then
            if Raw_Peek(Idx) < 0 then raise END_ERROR; else raise DATA_ERROR; end if;
         end if;
         declare
            V : NUM;
         begin
            -- A fixed-point value is read as a real literal and converted
            -- to the type via FLOAT (rounding to the nearest multiple of its
            -- small), since the literal syntax is the real one (RM 14.3.8).
            V := NUM (Real_Value (Buf (1 .. Len)));
            if V < NUM'FIRST or V > NUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
      end GET;

      procedure GET(ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         GET(FILE_TYPE(Current_In_Idx), ITEM, WIDTH);
      end GET;

      procedure PUT(FILE : in FILE_TYPE; ITEM : in NUM;
                    FORE : in FIELD := DEFAULT_FORE; AFT : in FIELD := DEFAULT_AFT;
                    EXP : in FIELD := DEFAULT_EXP) is
      begin
         Ensure_Init;
         Require_Open(Integer(FILE));
         if FCBs(Integer(FILE)).Mode = IN_FILE then raise MODE_ERROR; end if;
         Format_Real(FILE, FLOAT(ITEM), Integer(FORE), Integer(AFT), Integer(EXP));
      end PUT;

      procedure PUT(ITEM : in NUM; FORE : in FIELD := DEFAULT_FORE;
                    AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         PUT(FILE_TYPE(Current_Out_Idx), ITEM, FORE, AFT, EXP);
      end PUT;

      procedure GET(FROM : in STRING; ITEM : out NUM; LAST : out POSITIVE) is
         I : Integer := FROM'First;
         Token_Start, Token_End : Integer;
      begin
         while I <= FROM'Last and then
               (FROM(I) = ' ' or FROM(I) = Character'Val(9)) loop
            I := I + 1;
         end loop;
         Token_Start := I;
         -- Only blanks (or an empty/null string): nothing to read (RM 14.3.5).
         if Token_Start > FROM'Last then raise END_ERROR; end if;
         Token_End := Scan_String_Number(FROM, Token_Start, True);
         declare
            V : NUM;
         begin
            V := NUM(Real_Value(FROM(Token_Start .. Token_End)));
            if V < NUM'FIRST or V > NUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
         LAST := Token_End;
      end GET;

      procedure PUT(TO : out STRING; ITEM : in NUM; AFT : in FIELD := DEFAULT_AFT;
                    EXP : in FIELD := DEFAULT_EXP) is
         Img : constant String := Render_Real(FLOAT(ITEM), 1, Integer(AFT), Integer(EXP));
      begin
         if Img'Length > TO'Length then raise LAYOUT_ERROR; end if;
         for I in TO'Range loop TO(I) := ' '; end loop;
         TO(TO'Last - Img'Length + 1 .. TO'Last) := Img;
      end PUT;
   end FIXED_IO;

   package body ENUMERATION_IO is

      -- ENUM'IMAGE, lowered to lower case when SET so requests it (RM 14.3.9).
      function Cased_Image(Item : ENUM; Set : TYPE_SET) return String is
         Img : String := ENUM'IMAGE(Item);
      begin
         if Set = LOWER_CASE then
            for I in Img'Range loop
               if Img(I) >= 'A' and Img(I) <= 'Z' then
                  Img(I) := Character'Val(Character'Pos(Img(I)) + 32);
               end if;
            end loop;
         end if;
         return Img;
      end Cased_Image;

      procedure GET(FILE : in FILE_TYPE; ITEM : out ENUM) is
         Idx : Integer := Integer(FILE);
         Buf : String(1 .. 256);
         P   : Integer := 0;
         C   : Integer;
      begin
         Ensure_Init;
         Require_Open(Idx);
         if FCBs(Idx).Mode /= IN_FILE then raise MODE_ERROR; end if;
         Skip_Blanks_And_Terminators(Idx);

         -- Store the next raw character into Buf, advancing the column.
         declare
            procedure Take is
               Ch : Integer := Raw_Get(Idx);
            begin
               P := P + 1; Buf(P) := Character'Val(Ch);
               FCBs(Idx).Col := FCBs(Idx).Col + 1;
            end Take;
         begin
            C := Raw_Peek(Idx);
            if C = 39 then
               -- Character literal: read the quote and then up to two more
               -- characters without backtracking, so 'x' is taken whole while a
               -- bare quote or unterminated literal stops where it fails and
               -- leaves a malformed token for the VALUE check (RM 14.3.9, the
               -- GNAT Get_Enum_Lit rule exercised by CE3905L).
               Take;                                      -- opening quote
               C := Raw_Peek(Idx);
               if (C >= 32 and C <= 126) or C >= 128 then
                  Take;                                   -- the character
                  if Raw_Peek(Idx) = 39 then Take; end if;  -- closing quote
               end if;
            elsif (C >= 65 and C <= 90) or (C >= 97 and C <= 122) then
               -- Identifier: must start with a letter, then runs over letters,
               -- digits and single underscores. A trailing underscore is taken
               -- (leaving a malformed token), but a doubled underscore ends the
               -- token before the second one.
               loop
                  Take;
                  C := Raw_Peek(Idx);
                  exit when not ((C >= 65 and C <= 90) or (C >= 97 and C <= 122)
                                 or (C >= 48 and C <= 57) or C = 95);
                  exit when C = 95 and Buf(P) = '_';
               end loop;
            end if;
         end;

         -- No token: end of file only at the real terminator, otherwise a
         -- non-enumeral lexical element, i.e. Data_Error (RM 14.3.5).
         if P = 0 then
            if Raw_Peek(Idx) < 0 then raise END_ERROR; else raise DATA_ERROR; end if;
         end if;
         declare
            V : ENUM;
         begin
            V := ENUM'VALUE(Buf(1 .. P));
            -- A literal outside the instantiated (sub)type is Data_Error; a
            -- value within it but outside ITEM's subtype is left to the
            -- out-parameter check (Constraint_Error) (RM 14.3.9).
            if V < ENUM'FIRST or V > ENUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
      end GET;

      procedure GET(ITEM : out ENUM) is
      begin
         GET(FILE_TYPE(Current_In_Idx), ITEM);
      end GET;

      procedure PUT(FILE : in FILE_TYPE; ITEM : in ENUM;
                    WIDTH : in FIELD := DEFAULT_WIDTH;
                    SET : in TYPE_SET := DEFAULT_SETTING) is
      begin
         Ensure_Init;
         Require_Open(Integer(FILE));
         if FCBs(Integer(FILE)).Mode = IN_FILE then raise MODE_ERROR; end if;
         Put_Left_Justified(FILE, Cased_Image(ITEM, SET), Integer(WIDTH));
      end PUT;

      procedure PUT(ITEM : in ENUM; WIDTH : in FIELD := DEFAULT_WIDTH;
                    SET : in TYPE_SET := DEFAULT_SETTING) is
      begin
         PUT(FILE_TYPE(Current_Out_Idx), ITEM, WIDTH, SET);
      end PUT;

      procedure GET(FROM : in STRING; ITEM : out ENUM; LAST : out POSITIVE) is
         I : Integer := FROM'First;
         Token_Start, Token_End : Integer;
      begin
         while I <= FROM'Last and then
               (FROM(I) = ' ' or FROM(I) = Character'Val(9)) loop
            I := I + 1;
         end loop;
         Token_Start := I;
         -- Only blanks (or an empty/null string): nothing to read (RM 14.3.5).
         if Token_Start > FROM'Last then raise END_ERROR; end if;
         -- Mirror the file reader's enumeration-literal grammar (RM 14.3.9):
         -- a character literal taken without backtracking, or an identifier
         -- starting with a letter that absorbs at most one trailing underscore.
         declare
            function Class (J : Integer) return Integer is
               Cp : Integer := Character'Pos(FROM(J));
            begin
               if (Cp >= 65 and Cp <= 90) or (Cp >= 97 and Cp <= 122) then return 2;
               elsif Cp >= 48 and Cp <= 57 then return 1;
               elsif Cp = 95 then return 3;
               else return 0; end if;
            end Class;
         begin
            if FROM(I) = ''' then
               I := I + 1;                                  -- opening quote
               if I <= FROM'Last and then
                  ((Character'Pos(FROM(I)) >= 32 and Character'Pos(FROM(I)) <= 126)
                   or Character'Pos(FROM(I)) >= 128) then
                  I := I + 1;                               -- the character
                  if I <= FROM'Last and then FROM(I) = ''' then I := I + 1; end if;
               end if;
            elsif Class(I) = 2 then                         -- identifier (letter)
               loop
                  I := I + 1;
                  exit when I > FROM'Last or else Class(I) = 0;
                  exit when Class(I) = 3 and FROM(I - 1) = '_';
               end loop;
            end if;
         end;
         Token_End := I - 1;
         declare
            V : ENUM;
         begin
            V := ENUM'VALUE(FROM(Token_Start .. Token_End));
            if V < ENUM'FIRST or V > ENUM'LAST then raise DATA_ERROR; end if;
            ITEM := V;
         exception
            when others => raise DATA_ERROR;
         end;
         LAST := Token_End;
      end GET;

      procedure PUT(TO : out STRING; ITEM : in ENUM;
                    SET : in TYPE_SET := DEFAULT_SETTING) is
         Img : constant String := Cased_Image(ITEM, SET);
      begin
         if Img'Length > TO'Length then raise LAYOUT_ERROR; end if;
         for I in TO'Range loop TO(I) := ' '; end loop;
         TO(TO'First .. TO'First + Img'Length - 1) := Img;
      end PUT;
   end ENUMERATION_IO;

begin
   Init_Standard_Files;
   Initialized := True;
end TEXT_IO;
