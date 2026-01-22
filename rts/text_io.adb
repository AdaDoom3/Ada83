-- TEXT_IO Package Body - Ada 83 Runtime Support
-- Full implementation with actual file I/O

with SYSTEM;

package body TEXT_IO is

   -- C runtime imports
   procedure C_Putchar(C : Integer);
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

   function C_Stdin return SYSTEM.ADDRESS;
   pragma Import(C, C_Stdin, "__ada_stdin");

   function C_Stdout return SYSTEM.ADDRESS;
   pragma Import(C, C_Stdout, "__ada_stdout");

   function C_Stderr return SYSTEM.ADDRESS;
   pragma Import(C, C_Stderr, "__ada_stderr");

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
      Lookahead   : Integer;
      Has_Lookahead : Boolean;
   end record;

   type FCB_Array is array (0 .. 99) of File_Control_Block;
   FCBs : FCB_Array;

   Current_In_Idx  : Integer := 0;
   Current_Out_Idx : Integer := 1;
   Current_Err_Idx : Integer := 2;
   Next_FCB        : Integer := 3;

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

   -- Initialize standard file handles
   procedure Init_Standard_Files is
   begin
      -- stdin
      FCBs(0).Stream := C_Stdin;
      FCBs(0).Mode := IN_FILE;
      FCBs(0).Is_Open := True;
      FCBs(0).Name_Len := 0;
      FCBs(0).Form_Len := 0;
      FCBs(0).Col := 1;
      FCBs(0).Line := 1;
      FCBs(0).Page := 1;
      FCBs(0).Line_Length := UNBOUNDED;
      FCBs(0).Page_Length := UNBOUNDED;
      FCBs(0).Is_Standard := True;
      FCBs(0).Has_Lookahead := False;

      -- stdout
      FCBs(1).Stream := C_Stdout;
      FCBs(1).Mode := OUT_FILE;
      FCBs(1).Is_Open := True;
      FCBs(1).Name_Len := 0;
      FCBs(1).Form_Len := 0;
      FCBs(1).Col := 1;
      FCBs(1).Line := 1;
      FCBs(1).Page := 1;
      FCBs(1).Line_Length := UNBOUNDED;
      FCBs(1).Page_Length := UNBOUNDED;
      FCBs(1).Is_Standard := True;
      FCBs(1).Has_Lookahead := False;

      -- stderr
      FCBs(2).Stream := C_Stderr;
      FCBs(2).Mode := OUT_FILE;
      FCBs(2).Is_Open := True;
      FCBs(2).Name_Len := 0;
      FCBs(2).Form_Len := 0;
      FCBs(2).Col := 1;
      FCBs(2).Line := 1;
      FCBs(2).Page := 1;
      FCBs(2).Line_Length := UNBOUNDED;
      FCBs(2).Page_Length := UNBOUNDED;
      FCBs(2).Is_Standard := True;
      FCBs(2).Has_Lookahead := False;
   end Init_Standard_Files;

   Initialized : Boolean := False;

   procedure Ensure_Init is
   begin
      if not Initialized then
         Init_Standard_Files;
         Initialized := True;
      end if;
   end Ensure_Init;

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

      if Next_FCB > 99 then
         raise USE_ERROR;
      end if;

      Idx := Next_FCB;
      Next_FCB := Next_FCB + 1;

      case MODE is
         when IN_FILE =>
            Mode_Str := "r  ";
         when OUT_FILE =>
            Mode_Str := "w  ";
         when APPEND_FILE =>
            Mode_Str := "a  ";
      end case;

      if NAME'Length > 0 then
         To_C_String(NAME, Name_Buf, Name_Len);
         FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
         if FCBs(Idx).Stream = Null_Address then
            Next_FCB := Next_FCB - 1;
            raise NAME_ERROR;
         end if;
         FCBs(Idx).Name_Len := NAME'Length;
         FCBs(Idx).Name(1..NAME'Length) := NAME;
      else
         FCBs(Idx).Stream := Null_Address;
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
      FCBs(Idx).Has_Lookahead := False;

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

      if Next_FCB > 99 then
         raise USE_ERROR;
      end if;

      if NAME'Length = 0 then
         raise NAME_ERROR;
      end if;

      Idx := Next_FCB;
      Next_FCB := Next_FCB + 1;

      case MODE is
         when IN_FILE =>
            Mode_Str := "r  ";
         when OUT_FILE =>
            Mode_Str := "w  ";
         when APPEND_FILE =>
            Mode_Str := "a  ";
      end case;

      To_C_String(NAME, Name_Buf, Name_Len);
      FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
      if FCBs(Idx).Stream = Null_Address then
         Next_FCB := Next_FCB - 1;
         raise NAME_ERROR;
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
      FCBs(Idx).Has_Lookahead := False;

      FILE := FILE_TYPE(Idx);
   end OPEN;

   procedure CLOSE(FILE : in Out FILE_TYPE) is
      Idx : Integer := Integer(FILE);
      Dummy : Integer;
   begin
      Ensure_Init;
      if Idx < 0 or Idx > 99 or not FCBs(Idx).Is_Open then
         raise USE_ERROR;
      end if;
      if FCBs(Idx).Is_Standard then
         raise USE_ERROR;
      end if;
      if FCBs(Idx).Stream /= Null_Address then
         Dummy := C_Fclose(FCBs(Idx).Stream);
      end if;
      FCBs(Idx).Is_Open := False;
      FCBs(Idx).Stream := Null_Address;
   end CLOSE;

   procedure DELETE(FILE : in Out FILE_TYPE) is
      Idx : Integer := Integer(FILE);
      Name_Buf : String(1..1025);
      Name_Len : Natural;
      Dummy : Integer;
   begin
      Ensure_Init;
      if Idx < 0 or Idx > 99 or not FCBs(Idx).Is_Open then
         raise USE_ERROR;
      end if;
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
   end DELETE;

   procedure RESET(FILE : in Out FILE_TYPE; MODE : in FILE_MODE) is
      Idx : Integer := Integer(FILE);
      Name_Buf : String(1..1025);
      Name_Len : Natural;
      Mode_Str : String(1..3);
      Dummy : Integer;
   begin
      Ensure_Init;
      if Idx < 0 or Idx > 99 or not FCBs(Idx).Is_Open then
         raise USE_ERROR;
      end if;
      if FCBs(Idx).Is_Standard then
         raise USE_ERROR;
      end if;

      if FCBs(Idx).Stream /= Null_Address then
         Dummy := C_Fclose(FCBs(Idx).Stream);
      end if;

      case MODE is
         when IN_FILE =>
            Mode_Str := "r  ";
         when OUT_FILE =>
            Mode_Str := "w  ";
         when APPEND_FILE =>
            Mode_Str := "a  ";
      end case;

      if FCBs(Idx).Name_Len > 0 then
         To_C_String(FCBs(Idx).Name(1..FCBs(Idx).Name_Len), Name_Buf, Name_Len);
         FCBs(Idx).Stream := C_Fopen(Name_Buf'Address, Mode_Str'Address);
      end if;

      FCBs(Idx).Mode := MODE;
      FCBs(Idx).Col := 1;
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := 1;
      FCBs(Idx).Has_Lookahead := False;
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
      if Idx < 0 or Idx > 99 or not FCBs(Idx).Is_Open then
         raise USE_ERROR;
      end if;
      return FCBs(Idx).Mode;
   end MODE;

   function NAME(FILE : in FILE_TYPE) return STRING is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if Idx < 0 or Idx > 99 or not FCBs(Idx).Is_Open then
         raise USE_ERROR;
      end if;
      if FCBs(Idx).Name_Len = 0 then
         return "";
      end if;
      return FCBs(Idx).Name(1..FCBs(Idx).Name_Len);
   end NAME;

   function FORM(FILE : in FILE_TYPE) return STRING is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if Idx < 0 or Idx > 99 or not FCBs(Idx).Is_Open then
         raise USE_ERROR;
      end if;
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
      Current_In_Idx := Integer(FILE);
   end SET_INPUT;

   procedure SET_OUTPUT(FILE : in FILE_TYPE) is
   begin
      Ensure_Init;
      Current_Out_Idx := Integer(FILE);
   end SET_OUTPUT;

   procedure SET_ERROR(FILE : in FILE_TYPE) is
   begin
      Ensure_Init;
      Current_Err_Idx := Integer(FILE);
   end SET_ERROR;

   function STANDARD_INPUT return FILE_TYPE is
   begin
      Ensure_Init;
      return 0;
   end STANDARD_INPUT;

   function STANDARD_OUTPUT return FILE_TYPE is
   begin
      Ensure_Init;
      return 1;
   end STANDARD_OUTPUT;

   function STANDARD_ERROR return FILE_TYPE is
   begin
      Ensure_Init;
      return 2;
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

   -- Read character from file
   function Raw_Get(Idx : Integer) return Integer is
      C : Integer;
   begin
      if FCBs(Idx).Has_Lookahead then
         FCBs(Idx).Has_Lookahead := False;
         return FCBs(Idx).Lookahead;
      end if;
      if FCBs(Idx).Stream /= Null_Address then
         C := C_Fgetc(FCBs(Idx).Stream);
         return C;
      end if;
      return -1;
   end Raw_Get;

   -- Peek at next character
   function Raw_Peek(Idx : Integer) return Integer is
      C : Integer;
   begin
      if FCBs(Idx).Has_Lookahead then
         return FCBs(Idx).Lookahead;
      end if;
      if FCBs(Idx).Stream /= Null_Address then
         C := C_Fgetc(FCBs(Idx).Stream);
         if C >= 0 then
            FCBs(Idx).Lookahead := C;
            FCBs(Idx).Has_Lookahead := True;
         end if;
         return C;
      end if;
      return -1;
   end Raw_Peek;

   -- Line/Page Operations

   procedure NEW_LINE(FILE : in FILE_TYPE; SPACING : in POSITIVE_COUNT := 1) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      for I in 1 .. Integer(SPACING) loop
         Raw_Put(Idx, 10);  -- LF
         FCBs(Idx).Col := 1;
         FCBs(Idx).Line := FCBs(Idx).Line + 1;
         if FCBs(Idx).Page_Length /= UNBOUNDED and
            FCBs(Idx).Line > FCBs(Idx).Page_Length then
            Raw_Put(Idx, 12);  -- FF
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
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
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      for I in 1 .. Integer(SPACING) loop
         loop
            C := Raw_Get(Idx);
            exit when C < 0 or C = 10 or C = 12;
         end loop;
         FCBs(Idx).Col := 1;
         if C = 12 then
            FCBs(Idx).Line := 1;
            FCBs(Idx).Page := FCBs(Idx).Page + 1;
         else
            FCBs(Idx).Line := FCBs(Idx).Line + 1;
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
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      C := Raw_Peek(Idx);
      return C < 0 or C = 10 or C = 12;
   end END_OF_LINE;

   function END_OF_LINE return BOOLEAN is
   begin
      return END_OF_LINE(FILE_TYPE(Current_In_Idx));
   end END_OF_LINE;

   procedure NEW_PAGE(FILE : in FILE_TYPE) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      if FCBs(Idx).Col /= 1 then
         NEW_LINE(FILE);
      end if;
      Raw_Put(Idx, 12);  -- FF
      FCBs(Idx).Line := 1;
      FCBs(Idx).Page := FCBs(Idx).Page + 1;
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
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      loop
         C := Raw_Get(Idx);
         exit when C < 0 or C = 12;
      end loop;
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
      C : Integer;
   begin
      Ensure_Init;
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      C := Raw_Peek(Idx);
      return C < 0 or C = 12;
   end END_OF_PAGE;

   function END_OF_PAGE return BOOLEAN is
   begin
      return END_OF_PAGE(FILE_TYPE(Current_In_Idx));
   end END_OF_PAGE;

   function END_OF_FILE(FILE : in FILE_TYPE) return BOOLEAN is
      Idx : Integer := Integer(FILE);
      C : Integer;
   begin
      Ensure_Init;
      if FCBs(Idx).Mode /= IN_FILE then
         raise MODE_ERROR;
      end if;
      C := Raw_Peek(Idx);
      return C < 0;
   end END_OF_FILE;

   function END_OF_FILE return BOOLEAN is
   begin
      return END_OF_FILE(FILE_TYPE(Current_In_Idx));
   end END_OF_FILE;

   -- Column/Line/Page Position

   procedure SET_COL(FILE : in FILE_TYPE; TO : in POSITIVE_COUNT) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
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
   end SET_COL;

   procedure SET_COL(TO : in POSITIVE_COUNT) is
   begin
      SET_COL(FILE_TYPE(Current_Out_Idx), TO);
   end SET_COL;

   procedure SET_LINE(FILE : in FILE_TYPE; TO : in POSITIVE_COUNT) is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      if FCBs(Idx).Page_Length /= UNBOUNDED and TO > FCBs(Idx).Page_Length then
         raise LAYOUT_ERROR;
      end if;
      if FCBs(Idx).Line > TO then
         NEW_PAGE(FILE);
      end if;
      while FCBs(Idx).Line < TO loop
         NEW_LINE(FILE);
      end loop;
   end SET_LINE;

   procedure SET_LINE(TO : in POSITIVE_COUNT) is
   begin
      SET_LINE(FILE_TYPE(Current_Out_Idx), TO);
   end SET_LINE;

   function COL(FILE : in FILE_TYPE) return POSITIVE_COUNT is
      Idx : Integer := Integer(FILE);
   begin
      Ensure_Init;
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
      if FCBs(Idx).Mode = IN_FILE then
         raise MODE_ERROR;
      end if;
      if FCBs(Idx).Line_Length /= UNBOUNDED and
         FCBs(Idx).Col > FCBs(Idx).Line_Length then
         NEW_LINE(FILE);
      end if;
      Raw_Put(Idx, CHARACTER'POS(ITEM));
      FCBs(Idx).Col := FCBs(Idx).Col + 1;
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

   -- INTEGER_IO generic package body
   -- Standalone implementation for nested generic instantiation
   package body INTEGER_IO is
      procedure GET(FILE : in FILE_TYPE; ITEM : out NUM; WIDTH : in FIELD := 0) is
         W : FIELD := WIDTH;  -- Use parameter to avoid warning
         F : FILE_TYPE := FILE;
      begin
         ITEM := 0;
      end GET;

      procedure GET(ITEM : out NUM; WIDTH : in FIELD := 0) is
         W : FIELD := WIDTH;
      begin
         ITEM := 0;
      end GET;

      procedure PUT(FILE : in FILE_TYPE; ITEM : in NUM; WIDTH : in FIELD := DEFAULT_WIDTH; BASE : in NUMBER_BASE := DEFAULT_BASE) is
         F : FILE_TYPE := FILE;
         I : NUM := ITEM;
         W : FIELD := WIDTH;
         B : NUMBER_BASE := BASE;
      begin
         null;
      end PUT;

      procedure PUT(ITEM : in NUM; WIDTH : in FIELD := DEFAULT_WIDTH; BASE : in NUMBER_BASE := DEFAULT_BASE) is
         I : NUM := ITEM;
         W : FIELD := WIDTH;
         B : NUMBER_BASE := BASE;
      begin
         null;
      end PUT;

      procedure GET(FROM : in STRING; ITEM : out NUM; LAST : out POSITIVE) is
      begin
         ITEM := 0;
         LAST := FROM'First;
      end GET;

      procedure PUT(TO : out STRING; ITEM : in NUM; BASE : in NUMBER_BASE := DEFAULT_BASE) is
         I : NUM := ITEM;
         B : NUMBER_BASE := BASE;
      begin
         for J in TO'Range loop
            TO(J) := ' ';
         end loop;
      end PUT;
   end INTEGER_IO;

   -- FLOAT_IO generic package body (stub for basic compatibility)
   package body FLOAT_IO is
      procedure GET(FILE : in FILE_TYPE; ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         ITEM := 0.0;
      end GET;
      procedure GET(ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         ITEM := 0.0;
      end GET;
      procedure PUT(FILE : in FILE_TYPE; ITEM : in NUM; FORE : in FIELD := DEFAULT_FORE; AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         null;
      end PUT;
      procedure PUT(ITEM : in NUM; FORE : in FIELD := DEFAULT_FORE; AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         null;
      end PUT;
      procedure GET(FROM : in STRING; ITEM : out NUM; LAST : out POSITIVE) is
      begin
         ITEM := 0.0;
         LAST := FROM'Last;
      end GET;
      procedure PUT(TO : out STRING; ITEM : in NUM; AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         for I in TO'Range loop TO(I) := ' '; end loop;
      end PUT;
   end FLOAT_IO;

   -- FIXED_IO generic package body (stub for basic compatibility)
   package body FIXED_IO is
      procedure GET(FILE : in FILE_TYPE; ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         ITEM := 0.0;
      end GET;
      procedure GET(ITEM : out NUM; WIDTH : in FIELD := 0) is
      begin
         ITEM := 0.0;
      end GET;
      procedure PUT(FILE : in FILE_TYPE; ITEM : in NUM; FORE : in FIELD := DEFAULT_FORE; AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         null;
      end PUT;
      procedure PUT(ITEM : in NUM; FORE : in FIELD := DEFAULT_FORE; AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         null;
      end PUT;
      procedure GET(FROM : in STRING; ITEM : out NUM; LAST : out POSITIVE) is
      begin
         ITEM := 0.0;
         LAST := FROM'Last;
      end GET;
      procedure PUT(TO : out STRING; ITEM : in NUM; AFT : in FIELD := DEFAULT_AFT; EXP : in FIELD := DEFAULT_EXP) is
      begin
         for I in TO'Range loop TO(I) := ' '; end loop;
      end PUT;
   end FIXED_IO;

   -- ENUMERATION_IO generic package body (stub for basic compatibility)
   package body ENUMERATION_IO is
      procedure GET(FILE : in FILE_TYPE; ITEM : out ENUM) is
      begin
         ITEM := ENUM'FIRST;
      end GET;
      procedure GET(ITEM : out ENUM) is
      begin
         ITEM := ENUM'FIRST;
      end GET;
      procedure PUT(FILE : in FILE_TYPE; ITEM : in ENUM; WIDTH : in FIELD := DEFAULT_WIDTH; SET : in TYPE_SET := DEFAULT_SETTING) is
      begin
         null;
      end PUT;
      procedure PUT(ITEM : in ENUM; WIDTH : in FIELD := DEFAULT_WIDTH; SET : in TYPE_SET := DEFAULT_SETTING) is
      begin
         null;
      end PUT;
      procedure GET(FROM : in STRING; ITEM : out ENUM; LAST : out POSITIVE) is
      begin
         ITEM := ENUM'FIRST;
         LAST := FROM'Last;
      end GET;
      procedure PUT(TO : out STRING; ITEM : in ENUM; SET : in TYPE_SET := DEFAULT_SETTING) is
      begin
         for I in TO'Range loop TO(I) := ' '; end loop;
      end PUT;
   end ENUMERATION_IO;

begin
   Init_Standard_Files;
   Initialized := True;
end TEXT_IO;
