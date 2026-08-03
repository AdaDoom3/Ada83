package IO_Exceptions is
  Data_Error   : exception;
  Device_Error : exception;
  End_Error    : exception;
  Layout_Error : exception;
  Mode_Error   : exception;
  Name_Error   : exception;
  Status_Error : exception;
  Use_Error    : exception;
end;
package Low_Level_IO is
  type Device is new Integer;
  type Data is new Integer;
  procedure Send_Control (Device : Device; Data : in out Data);
  procedure Receive_Control (Device : Device; Data : in out Data);
end;
package body Low_Level_IO is
  Last_Device  : Device := 0;
  Last_Send    : Data   := 0;
  Last_Receive : Data   := 0;
  procedure Send_Control (Device : Device; Data : in out Data) is
    begin
      Last_Device := Device;
      Last_Send := Data;
      Data := Data + 1;
    end;
  procedure Receive_Control (Device : Device; Data : in out Data) is
    begin
      Last_Device := Device;
      Last_Receive := Data;
      Data := Last_Send;
    end;
end;
package Text_IO is
  type File_Type is limited private;
  type File_Mode is (In_File, Out_File, Append_File);
  type Count is range 0..32767;
  subtype Positive_Count is Count range 1..Count'Last;
  Unbounded : constant Count := 0;
  subtype Field is Integer range 0..Integer'Last;
  subtype Number_Base is Integer range 2..16;
  type Type_Set is (Lower_Case, Upper_Case);
  Status_Error : exception;
  Mode_Error   : exception;
  Name_Error   : exception;
  Use_Error    : exception;
  Device_Error : exception;
  End_Error    : exception;
  procedure Close (File : in out File_Type);
  function Col (File : File_Type) return Positive_Count;
  function Col return Positive_Count;
  procedure Create (File : in out File_Type; Mode : File_Mode := Out_File; Name : String := ""; Form : String := "");
  function Current_Error return File_Type;
  function Current_Input return File_Type;
  function Current_Output return File_Type;
  Data_Error   : exception;
  procedure Delete (File : in out File_Type);
  function End_Of_File (File : File_Type) return Boolean;
  function End_Of_File return Boolean;
  function End_Of_Line (File : File_Type) return Boolean;
  function End_Of_Line return Boolean;
  function End_Of_Page (File : File_Type) return Boolean;
  function End_Of_Page return Boolean;
  procedure Flush (File : File_Type);
  procedure Flush;
  function Form (File : File_Type) return String;
  procedure Get (File : File_Type; Item : out Character);
  procedure Get (File : File_Type; Item : out String);
  procedure Get (Item : out Character);
  procedure Get (Item : out String);
  procedure Get_Line (File : File_Type; Item : out String; Last : out Natural);
  procedure Get_Line (Item : out String; Last : out Natural);
  function Is_Open (File : File_Type) return Boolean;
  Layout_Error : exception;
  function Line (File : File_Type) return Positive_Count;
  function Line return Positive_Count;
  function Line_Length (File : File_Type) return Count;
  function Line_Length return Count;
  function Mode (File : File_Type) return File_Mode;
  function Name (File : File_Type) return String;
  procedure New_Line (File : File_Type; Spacing : Positive_Count := 1);
  procedure New_Line (Spacing : Positive_Count := 1);
  procedure New_Page (File : File_Type);
  procedure New_Page;
  procedure Open (File : in out File_Type; Mode : File_Mode; Name : String; Form : String := "");
  function Page (File : File_Type) return Positive_Count;
  function Page return Positive_Count;
  function Page_Length (File : File_Type) return Count;
  function Page_Length return Count;
  procedure Put (File : File_Type; Item : Character);
  procedure Put (File : File_Type; Item : String);
  procedure Put (Item : Character);
  procedure Put (Item : String);
  procedure Put_Line (File : File_Type; Item : String);
  procedure Put_Line (Item : String);
  procedure Reset (File : in out File_Type);
  procedure Reset (File : in out File_Type; Mode : File_Mode);
  procedure Set_Col (File : File_Type; To : Positive_Count);
  procedure Set_Col (To : Positive_Count);
  procedure Set_Error (File : File_Type);
  procedure Set_Input (File : File_Type);
  procedure Set_Line (File : File_Type; To : Positive_Count);
  procedure Set_Line (To : Positive_Count);
  procedure Set_Line_Length (File : File_Type; To : Count);
  procedure Set_Line_Length (To : Count);
  procedure Set_Output (File : File_Type);
  procedure Set_Page_Length (File : File_Type; To : Count);
  procedure Set_Page_Length (To : Count);
  procedure Skip_Line (File : File_Type; Spacing : Positive_Count := 1);
  procedure Skip_Line (Spacing : Positive_Count := 1);
  procedure Skip_Page (File : File_Type);
  procedure Skip_Page;
  function Standard_Error return File_Type;
  function Standard_Input return File_Type;
  function Standard_Output return File_Type;
  generic
    type Number is range<>;
  package Integer_IO is
    Default_Width : Field       := 11;
    Default_Base  : Number_Base := 10;
    procedure Get (File : File_Type; Item : out Number; Width : Field := 0);
    procedure Get (From : String; Item : out Number; Last : out Positive);
    procedure Get (Item : out Number; Width : Field := 0);
    procedure Put (File : File_Type; Item : Number; Width : Field := Default_Width; Base : Number_Base := Default_Base);
    procedure Put (Item : Number; Width : Field := Default_Width; Base : Number_Base := Default_Base);
    procedure Put (To : out String; Item : Number; Base : Number_Base := Default_Base);
  end;
  generic
    type Number is digits<>;
  package Float_IO is
    Default_Fore : Field := 2;
    Default_Aft  : Field := Number'Digits-1;
    Default_Exp  : Field := 2;
    procedure Get (File : File_Type; Item : out Number; Width : Field := 0);
    procedure Get (From : String; Item : out Number; Last : out Positive);
    procedure Get (Item : out Number; Width : Field := 0);
    procedure Put (File : File_Type;
                   Item : Number;
                   Fore : Field := Default_Fore;
                   Aft  : Field := Default_Aft;
                   Exp  : Field := Default_Exp);
    procedure Put (Item : Number; Fore : Field := Default_Fore; Aft : Field := Default_Aft; Exp : Field := Default_Exp);
    procedure Put (To : out String; Item : Number; Aft : Field := Default_Aft; Exp : Field := Default_Exp);
  end;
  generic
    type Number is delta<>;
  package Fixed_IO is
    Default_Fore : Field := 2;
    Default_Aft  : Field := Number'Aft;
    Default_Exp  : Field := 0;
    procedure Get (File : File_Type; Item : out Number; Width : Field := 0);
    procedure Get (From : String; Item : out Number; Last : out Positive);
    procedure Get (Item : out Number; Width : Field := 0);
    procedure Put (File : File_Type;
                   Item : Number;
                   Fore : Field := Default_Fore;
                   Aft  : Field := Default_Aft;
                   Exp  : Field := Default_Exp);
    procedure Put (Item : Number; Fore : Field := Default_Fore; Aft : Field := Default_Aft; Exp : Field := Default_Exp);
    procedure Put (To : out String; Item : Number; Aft : Field := Default_Aft; Exp : Field := Default_Exp);
  end;
  generic
    type Enum is (<>);
  package Enumeration_IO is
    Default_Width   : Field    := 0;
    Default_Setting : Type_Set := Upper_Case;
    procedure Get (File : File_Type; Item : out Enum);
    procedure Get (From : String; Item : out Enum; Last : out Positive);
    procedure Get (Item : out Enum);
    procedure Put (File : File_Type; Item : Enum; Width : Field := Default_Width; Set : Type_Set := Default_Setting);
    procedure Put (Item : Enum; Width : Field := Default_Width; Set : Type_Set := Default_Setting);
    procedure Put (To : out String; Item : Enum; Set : Type_Set := Default_Setting);
  end;
private
  type File_Type is record
      Handle : Integer := 0;
    end record;
end;
with System;
package body Text_IO is
  type Long_Real is digits 15;

  function C_Fopen (Name : System.Address; Mode : System.Address) return System.Address;
  pragma Import (C, C_Fopen, "fopen");

  function C_Fclose (Stream : System.Address) return Integer;
  pragma Import (C, C_Fclose, "fclose");

  function C_Fputc (C : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Fputc, "fputc");

  function C_Fgetc (Stream : System.Address) return Integer;
  pragma Import (C, C_Fgetc, "fgetc");

  function C_Fwrite (Pointer : System.Address; Size : Integer; Count : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Fwrite, "fwrite");

  function C_Fflush (Stream : System.Address) return Integer;
  pragma Import (C, C_Fflush, "fflush");

  function C_Remove (Name : System.Address) return Integer;
  pragma Import (C, C_Remove, "remove");

  function C_Ftell (Stream : System.Address) return Integer;
  pragma Import (C, C_Ftell, "ftell");

  function C_Fseek (Stream : System.Address; Offset : Integer; Whence : Integer) return Integer;
  pragma Import (C, C_Fseek, "fseek");

  function C_Tmpfile return System.Address;
  pragma Import (C, C_Tmpfile, "tmpfile");

  function C_Stdin return System.Address;
  pragma Import (C, C_Stdin, "__ada_stdin");

  function C_Stdout return System.Address;
  pragma Import (C, C_Stdout, "__ada_stdout");

  function C_Stderr return System.Address;
  pragma Import (C, C_Stderr, "__ada_stderr");

  type Lookahead_Buffer is array (0..7) of Integer;
  type File_Control_Block is record
      Stream      : System.Address;
      Mode        : File_Mode;
      Is_Open     : Boolean;
      Name_Length : Integer;
      Name        : String (1..1024);
      Form_Length : Integer;
      Form        : String (1..256);
      Col         : Integer;
      Line        : Integer;
      Page        : Integer;
      Line_Length : Count;
      Page_Length : Count;
      Is_Standard : Boolean;
      Look        : Lookahead_Buffer;
      Look_Count  : Integer;
      Page_Active : Boolean;
      Shared      : Boolean;
      Write_Pos   : Integer;
    end record;
  type FCB_Array is array (0..99) of File_Control_Block;
  Initialized         :          Boolean        := False;
  File_Control_Blocks :          FCB_Array;
  Current_Input_Slot  :          Integer        := 1;
  Current_Output_Slot :          Integer        := 2;
  Current_Error_Slot  :          Integer        := 3;
  Next_FCB            :          Integer        := 4;
  Null_Address        : constant System.Address := System.Null_Address;
  procedure To_C_String (S : String; Buffer : out String) is
    J : Integer := 1;
    begin
      for I in S'Range loop
        Buffer (J) := S (I);
        J := J + 1;
      end loop;
      Buffer (J) := Character'Val (0);
    end;
  function Open_Mode_String (Mode : File_Mode) return String is
    begin
      case Mode is
        when In_File => return "r" & Character'Val (0);
        when Out_File => return "w+" & Character'Val (0);
        when Append_File => return "a+" & Character'Val (0);
      end case;
    end;
  procedure Reset_Position (Table_Slot : Integer) is
    begin
      File_Control_Blocks (Table_Slot).Col := 1;
      File_Control_Blocks (Table_Slot).Line := 1;
      File_Control_Blocks (Table_Slot).Page := 1;
      File_Control_Blocks (Table_Slot).Line_Length := Unbounded;
      File_Control_Blocks (Table_Slot).Page_Length := Unbounded;
      File_Control_Blocks (Table_Slot).Look_Count := 0;
      File_Control_Blocks (Table_Slot).Page_Active := False;
    end;
  procedure Init_Standard_File (Table_Slot : Integer; Stream : System.Address; Mode : File_Mode) is
    begin
      File_Control_Blocks (Table_Slot).Stream := Stream;
      File_Control_Blocks (Table_Slot).Mode := Mode;
      File_Control_Blocks (Table_Slot).Is_Open := True;
      File_Control_Blocks (Table_Slot).Name_Length := 0;
      File_Control_Blocks (Table_Slot).Form_Length := 0;
      File_Control_Blocks (Table_Slot).Is_Standard := True;
      File_Control_Blocks (Table_Slot).Shared := False;
      Reset_Position (Table_Slot);
    end;
  procedure Init_Standard_Files is
    begin
      Init_Standard_File (1, C_Stdin, In_File);
      Init_Standard_File (2, C_Stdout, Out_File);
      Init_Standard_File (3, C_Stderr, Out_File);
    end;
  procedure Ensure_Init is
    begin
      if not Initialized then
        Init_Standard_Files;
        Initialized := True;
      end if;
    end;
  function Is_Open_Index (Table_Slot : Integer) return Boolean is
    begin
      Ensure_Init;
      return (Table_Slot >= 1 and Table_Slot <= 99) and then File_Control_Blocks (Table_Slot).Is_Open;
    end;
  procedure Require_Open (Table_Slot : Integer) is
    begin
      if not Is_Open_Index (Table_Slot) then raise Status_Error; end if;
    end;
  procedure Raw_Put (Table_Slot : Integer; C : Integer);
  function Same_External (I : Integer; Name : String) return Boolean is
    begin
      return Name'Length /= 0 and then File_Control_Blocks (I).Is_Open
        and then File_Control_Blocks (I).Stream /= Null_Address
        and then File_Control_Blocks (I).Name_Length = Name'Length
        and then File_Control_Blocks (I).Name (1..File_Control_Blocks (I).Name_Length) = Name;
    end;
  function Find_Open_By_Name (Name : String) return Integer is
    begin
      for I in 4..99 loop
        if Same_External (I, Name) then return I; end if;
      end loop;
      return 0;
    end;
  procedure Flush_External (Name : String) is
    Dummy : Integer;
    begin
      for I in 4..99 loop
        -- Only a file with pending output is flushable; see Reset below.
        if Same_External (I, Name) and then File_Control_Blocks (I).Mode /= In_File then
          Dummy := C_Fflush (File_Control_Blocks (I).Stream);
        end if;
      end loop;
    end;
  procedure Create (File : in out File_Type; Mode : File_Mode := Out_File; Name : String := ""; Form : String := "") is
    Table_Slot  :          Integer;
    Mode_Text   : constant String := Open_Mode_String (Mode);
    Name_Buffer :          String (1..1025);
    begin
      if Is_Open_Index (File.Handle) then
        raise Status_Error;
      end if;
      if Next_FCB > 99 then
        raise Use_Error;
      end if;
      Table_Slot := Next_FCB;
      Next_FCB := Next_FCB + 1;
      if Name'Length > 0 then
        To_C_String (Name, Name_Buffer);
        File_Control_Blocks (Table_Slot).Stream := C_Fopen (Name_Buffer'Address, Mode_Text'Address);
        if File_Control_Blocks (Table_Slot).Stream = Null_Address then
          Next_FCB := Next_FCB - 1;
          raise Name_Error;
        end if;
        File_Control_Blocks (Table_Slot).Name_Length := Name'Length;
        File_Control_Blocks (Table_Slot).Name (1..Name'Length) := Name;
      else
        File_Control_Blocks (Table_Slot).Stream := C_Tmpfile;
        if File_Control_Blocks (Table_Slot).Stream = Null_Address then
          Next_FCB := Next_FCB - 1;
          raise Use_Error;
        end if;
        File_Control_Blocks (Table_Slot).Name_Length := 0;
      end if;
      File_Control_Blocks (Table_Slot).Mode := Mode;
      File_Control_Blocks (Table_Slot).Is_Open := True;
      File_Control_Blocks (Table_Slot).Form_Length := Form'Length;
      if Form'Length > 0 then
        File_Control_Blocks (Table_Slot).Form (1..Form'Length) := Form;
      end if;
      File_Control_Blocks (Table_Slot).Is_Standard := False;
      Reset_Position (Table_Slot);
      File_Control_Blocks (Table_Slot).Shared := False;
      File := (Handle => Table_Slot);
    end;
  procedure Open (File : in out File_Type; Mode : File_Mode; Name : String; Form : String := "") is
    Table_Slot  :          Integer;
    Mode_Text   : constant String := Open_Mode_String (Mode);
    Name_Buffer :          String (1..1025);
    begin
      if Is_Open_Index (File.Handle) then
        raise Status_Error;
      end if;
      if File.Handle /= 0 then
        if File.Handle = Current_Output_Slot and Mode = In_File then
          raise Mode_Error;
        end if;
        if File.Handle = Current_Input_Slot and Mode /= In_File then
          raise Mode_Error;
        end if;
      end if;
      if Next_FCB > 99 then
        raise Use_Error;
      end if;
      if Name'Length = 0 then
        raise Name_Error;
      end if;
      Table_Slot := Next_FCB;
      Next_FCB := Next_FCB + 1;
      Flush_External (Name);
      declare
        Sharer : Integer := Find_Open_By_Name (Name);
        Dummy  : Integer;
        begin
          if Sharer /= 0 then
            File_Control_Blocks (Table_Slot).Stream := File_Control_Blocks (Sharer).Stream;
            if not File_Control_Blocks (Sharer).Shared then
              File_Control_Blocks (Sharer).Write_Pos := C_Ftell (File_Control_Blocks (Sharer).Stream);
              File_Control_Blocks (Sharer).Shared := True;
            end if;
            File_Control_Blocks (Table_Slot).Shared := True;
            File_Control_Blocks (Table_Slot).Write_Pos := 0;
            if Mode = Append_File then
              Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, 2);
              File_Control_Blocks (Table_Slot).Write_Pos := C_Ftell (File_Control_Blocks (Table_Slot).Stream);
            elsif Mode = In_File then
              Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, 0);
            end if;
          else
            To_C_String (Name, Name_Buffer);
            File_Control_Blocks (Table_Slot).Stream := C_Fopen (Name_Buffer'Address, Mode_Text'Address);
            File_Control_Blocks (Table_Slot).Shared := False;
            if File_Control_Blocks (Table_Slot).Stream = Null_Address then
              Next_FCB := Next_FCB - 1;
              raise Name_Error;
            end if;
          end if;
        end;
      File_Control_Blocks (Table_Slot).Mode := Mode;
      File_Control_Blocks (Table_Slot).Is_Open := True;
      File_Control_Blocks (Table_Slot).Name_Length := Name'Length;
      File_Control_Blocks (Table_Slot).Name (1..Name'Length) := Name;
      File_Control_Blocks (Table_Slot).Form_Length := Form'Length;
      if Form'Length > 0 then
        File_Control_Blocks (Table_Slot).Form (1..Form'Length) := Form;
      end if;
      File_Control_Blocks (Table_Slot).Is_Standard := False;
      Reset_Position (Table_Slot);
      if File.Handle /= 0 then
        if File.Handle = Current_Output_Slot then
          Current_Output_Slot := Table_Slot;
        end if;
        if File.Handle = Current_Input_Slot then
          Current_Input_Slot := Table_Slot;
        end if;
      end if;
      File := (Handle => Table_Slot);
    end;
  procedure Close (File : in out File_Type) is
    Table_Slot : Integer := File.Handle;
    Dummy      : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Is_Standard then
        raise Use_Error;
      end if;
      declare
        Still_Shared : Boolean := False;
        begin
          if File_Control_Blocks (Table_Slot).Shared then
            for I in 4..99 loop
              if I /= Table_Slot and then File_Control_Blocks (I).Is_Open
                  and then File_Control_Blocks (I).Stream = File_Control_Blocks (Table_Slot).Stream then
                Still_Shared := True;
              end if;
            end loop;
          end if;
          if not Still_Shared then
            if File_Control_Blocks (Table_Slot).Shared and File_Control_Blocks (Table_Slot).Stream /= Null_Address then
              Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, 2);
              File_Control_Blocks (Table_Slot).Shared := False;
            end if;
            if File_Control_Blocks (Table_Slot).Mode /= In_File and File_Control_Blocks (Table_Slot).Page_Active then
              if File_Control_Blocks (Table_Slot).Col > 1 then
                Raw_Put (Table_Slot, 10);
              end if;
              Raw_Put (Table_Slot, 12);
              File_Control_Blocks (Table_Slot).Page_Active := False;
            end if;
            if File_Control_Blocks (Table_Slot).Stream /= Null_Address then
              Dummy := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
            end if;
          end if;
        end;
      File_Control_Blocks (Table_Slot).Is_Open := False;
      File_Control_Blocks (Table_Slot).Stream := Null_Address;
    end;
  procedure Delete (File : in out File_Type) is
    Table_Slot  : Integer := File.Handle;
    Name_Buffer : String (1..1025);
    Dummy       : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Is_Standard then
        raise Use_Error;
      end if;
      if File_Control_Blocks (Table_Slot).Stream /= Null_Address then
        Dummy := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
      end if;
      if File_Control_Blocks (Table_Slot).Name_Length > 0 then
        To_C_String (File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length), Name_Buffer);
        Dummy := C_Remove (Name_Buffer'Address);
      end if;
      File_Control_Blocks (Table_Slot).Is_Open := False;
      File_Control_Blocks (Table_Slot).Stream := Null_Address;
      File.Handle := 0;
    end;
  procedure Reset (File : in out File_Type; Mode : File_Mode) is
    Table_Slot  :          Integer := File.Handle;
    Name_Buffer :          String (1..1025);
    Mode_Text   : constant String  := Open_Mode_String (Mode);
    Dummy       :          Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Is_Standard then
        raise Use_Error;
      end if;
      if (Table_Slot = Current_Output_Slot or Table_Slot = Current_Error_Slot) and Mode = In_File then
        raise Mode_Error;
      end if;
      if Table_Slot = Current_Input_Slot and Mode /= In_File then
        raise Mode_Error;
      end if;
      if File_Control_Blocks (Table_Slot).Mode /= In_File and File_Control_Blocks (Table_Slot).Page_Active then
        if File_Control_Blocks (Table_Slot).Col > 1 then
          Raw_Put (Table_Slot, 10);
        end if;
        Raw_Put (Table_Slot, 12);
        File_Control_Blocks (Table_Slot).Page_Active := False;
      end if;
      if File_Control_Blocks (Table_Slot).Shared and File_Control_Blocks (Table_Slot).Stream /= Null_Address then
        -- Flushing a stream last used for reading is undefined in C. The
        -- Windows runtime advances it to the end of what it had buffered,
        -- which drags every other internal file sharing the stream to end of
        -- file; the branches below reposition where a mode needs it.
        if File_Control_Blocks (Table_Slot).Mode /= In_File then
          Dummy := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
        end if;
        if Mode = In_File then
          null;
        elsif Mode = Out_File then
          File_Control_Blocks (Table_Slot).Write_Pos := 0;
        else
          Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, 2);
          File_Control_Blocks (Table_Slot).Write_Pos := C_Ftell (File_Control_Blocks (Table_Slot).Stream);
        end if;
      elsif File_Control_Blocks (Table_Slot).Name_Length > 0 then
        if File_Control_Blocks (Table_Slot).Stream /= Null_Address then
          Dummy := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
        end if;
        To_C_String (File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length), Name_Buffer);
        File_Control_Blocks (Table_Slot).Stream := C_Fopen (Name_Buffer'Address, Mode_Text'Address);
      elsif File_Control_Blocks (Table_Slot).Stream /= Null_Address then
        Dummy := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
        Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, 0);
      end if;
      File_Control_Blocks (Table_Slot).Mode := Mode;
      Reset_Position (Table_Slot);
    end;
  procedure Reset (File : in out File_Type) is
    begin
      Reset (File, Mode (File));
    end;
  function Mode (File : File_Type) return File_Mode is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      return File_Control_Blocks (Table_Slot).Mode;
    end;
  function Name (File : File_Type) return String is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Name_Length = 0 then
        return "";
      end if;
      return File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length);
    end;
  function Form (File : File_Type) return String is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Form_Length = 0 then
        return "";
      end if;
      return File_Control_Blocks (Table_Slot).Form (1..File_Control_Blocks (Table_Slot).Form_Length);
    end;
  function Is_Open (File : File_Type) return Boolean is
    begin
      return Is_Open_Index (File.Handle);
    end;
  procedure Set_Input (File : File_Type) is
    begin
      Require_Open (File.Handle);
      if File_Control_Blocks (File.Handle).Mode /= In_File then raise Mode_Error; end if;
      Current_Input_Slot := File.Handle;
    end;
  procedure Set_Output (File : File_Type) is
    begin
      Require_Open (File.Handle);
      if File_Control_Blocks (File.Handle).Mode = In_File then raise Mode_Error; end if;
      Current_Output_Slot := File.Handle;
    end;
  procedure Set_Error (File : File_Type) is
    begin
      Require_Open (File.Handle);
      if File_Control_Blocks (File.Handle).Mode = In_File then raise Mode_Error; end if;
      Current_Error_Slot := File.Handle;
    end;
  function Standard_Input return File_Type is
    begin
      return (Handle => 1);
    end;
  function Standard_Output return File_Type is
    begin
      return (Handle => 2);
    end;
  function Standard_Error return File_Type is
    begin
      return (Handle => 3);
    end;
  function Current_Input return File_Type is
    begin
      return (Handle => Current_Input_Slot);
    end;
  function Current_Output return File_Type is
    begin
      return (Handle => Current_Output_Slot);
    end;
  function Current_Error return File_Type is
    begin
      return (Handle => Current_Error_Slot);
    end;
  procedure Flush (File : File_Type) is
    Table_Slot : Integer := File.Handle;
    Dummy      : Integer;
    begin
      if Is_Open_Index (Table_Slot) and then File_Control_Blocks (Table_Slot).Stream /= Null_Address then
        Dummy := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
      end if;
    end;
  procedure Flush is
    begin
      Flush ((Handle => Current_Output_Slot));
    end;
  procedure Set_Line_Length (File : File_Type; To : Count) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then raise Mode_Error; end if;
      File_Control_Blocks (Table_Slot).Line_Length := To;
    end;
  procedure Set_Line_Length (To : Count) is
    begin
      Set_Line_Length ((Handle => Current_Output_Slot), To);
    end;
  procedure Set_Page_Length (File : File_Type; To : Count) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then raise Mode_Error; end if;
      File_Control_Blocks (Table_Slot).Page_Length := To;
    end;
  procedure Set_Page_Length (To : Count) is
    begin
      Set_Page_Length ((Handle => Current_Output_Slot), To);
    end;
  function Line_Length (File : File_Type) return Count is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then raise Mode_Error; end if;
      return File_Control_Blocks (Table_Slot).Line_Length;
    end;
  function Line_Length return Count is
    begin
      return Line_Length ((Handle => Current_Output_Slot));
    end;
  function Page_Length (File : File_Type) return Count is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then raise Mode_Error; end if;
      return File_Control_Blocks (Table_Slot).Page_Length;
    end;
  function Page_Length return Count is
    begin
      return Page_Length ((Handle => Current_Output_Slot));
    end;
  procedure Raw_Put (Table_Slot : Integer; C : Integer) is
    Dummy : Integer;
    Saved : Integer;
    begin
      if File_Control_Blocks (Table_Slot).Stream /= Null_Address then
        if File_Control_Blocks (Table_Slot).Shared then
          Saved := C_Ftell (File_Control_Blocks (Table_Slot).Stream);
          Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, File_Control_Blocks (Table_Slot).Write_Pos, 0);
          Dummy := C_Fputc (C, File_Control_Blocks (Table_Slot).Stream);
          File_Control_Blocks (Table_Slot).Write_Pos := File_Control_Blocks (Table_Slot).Write_Pos + 1;
          Dummy := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
          Dummy := C_Fseek (File_Control_Blocks (Table_Slot).Stream, Saved, 0);
        else
          Dummy := C_Fputc (C, File_Control_Blocks (Table_Slot).Stream);
        end if;
      end if;
    end;
  procedure Raw_Write (Table_Slot : Integer; Item : String) is
    Dummy : Integer;
    begin
      if File_Control_Blocks (Table_Slot).Stream /= Null_Address then
        Dummy := C_Fwrite (Item'Address, 1, Item'Length, File_Control_Blocks (Table_Slot).Stream);
      end if;
    end;
  function Peek_N (Table_Slot : Integer; N : Integer) return Integer is
    C : Integer;
    begin
      while File_Control_Blocks (Table_Slot).Look_Count <= N loop
        if File_Control_Blocks (Table_Slot).Stream /= Null_Address then
          C := C_Fgetc (File_Control_Blocks (Table_Slot).Stream);
        else
          C := -1;
        end if;
        File_Control_Blocks (Table_Slot).Look (File_Control_Blocks (Table_Slot).Look_Count) := C;
        File_Control_Blocks (Table_Slot).Look_Count := File_Control_Blocks (Table_Slot).Look_Count + 1;
      end loop;
      return File_Control_Blocks (Table_Slot).Look (N);
    end;
  function Raw_Peek (Table_Slot : Integer) return Integer is
    begin
      return Peek_N (Table_Slot, 0);
    end;
  function Raw_Get (Table_Slot : Integer) return Integer is
    C : Integer := Peek_N (Table_Slot, 0);
    begin
      for K in 1..File_Control_Blocks (Table_Slot).Look_Count - 1 loop
        File_Control_Blocks (Table_Slot).Look (K - 1) := File_Control_Blocks (Table_Slot).Look (K);
      end loop;
      File_Control_Blocks (Table_Slot).Look_Count := File_Control_Blocks (Table_Slot).Look_Count - 1;
      return C;
    end;
  procedure New_Line (File : File_Type; Spacing : Positive_Count := 1) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then
        raise Mode_Error;
      end if;
      for I in 1..Integer (Spacing) loop
        Raw_Put (Table_Slot, 10);
        File_Control_Blocks (Table_Slot).Col := 1;
        File_Control_Blocks (Table_Slot).Line := File_Control_Blocks (Table_Slot).Line + 1;
        File_Control_Blocks (Table_Slot).Page_Active := True;
        if File_Control_Blocks (Table_Slot).Page_Length /= Unbounded
            and File_Control_Blocks (Table_Slot).Line > Integer (File_Control_Blocks (Table_Slot).Page_Length) then
          Raw_Put (Table_Slot, 12);
          File_Control_Blocks (Table_Slot).Line := 1;
          File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
          File_Control_Blocks (Table_Slot).Page_Active := False;
        end if;
      end loop;
    end;
  procedure New_Line (Spacing : Positive_Count := 1) is
    begin
      New_Line ((Handle => Current_Output_Slot), Spacing);
    end;
  procedure Skip_Line (File : File_Type; Spacing : Positive_Count := 1) is
    Table_Slot : Integer := File.Handle;
    C          : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      for I in 1..Integer (Spacing) loop
        declare
          Saw_Content : Boolean := False;
          begin
            loop
              C := Raw_Peek (Table_Slot);
              if C < 0 then
                if Saw_Content then exit; end if;
                raise End_Error;
              end if;
              C := Raw_Get (Table_Slot);
              exit when C = 10;
              Saw_Content := True;
            end loop;
          end;
        File_Control_Blocks (Table_Slot).Col := 1;
        File_Control_Blocks (Table_Slot).Line := File_Control_Blocks (Table_Slot).Line + 1;
        if Raw_Peek (Table_Slot) = 12 then
          C := Raw_Get (Table_Slot);
          File_Control_Blocks (Table_Slot).Line := 1;
          File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
        end if;
      end loop;
    end;
  procedure Skip_Line (Spacing : Positive_Count := 1) is
    begin
      Skip_Line ((Handle => Current_Input_Slot), Spacing);
    end;
  function End_Of_Line (File : File_Type) return Boolean is
    Table_Slot : Integer := File.Handle;
    C          : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      C := Raw_Peek (Table_Slot);
      return C < 0 or C = 10 or C = 12;
    end;
  function End_Of_Line return Boolean is
    begin
      return End_Of_Line ((Handle => Current_Input_Slot));
    end;
  procedure New_Page (File : File_Type) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then
        raise Mode_Error;
      end if;
      if File_Control_Blocks (Table_Slot).Col /= 1 then
        New_Line (File);
      elsif not File_Control_Blocks (Table_Slot).Page_Active then
        New_Line (File);
      end if;
      Raw_Put (Table_Slot, 12);
      File_Control_Blocks (Table_Slot).Line := 1;
      File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
      File_Control_Blocks (Table_Slot).Page_Active := False;
    end;
  procedure New_Page is
    begin
      New_Page ((Handle => Current_Output_Slot));
    end;
  procedure Skip_Page (File : File_Type) is
    Table_Slot : Integer := File.Handle;
    C          : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      declare
        Saw_Content : Boolean := False;
        begin
          loop
            C := Raw_Peek (Table_Slot);
            if C < 0 then
              if Saw_Content then exit; end if;
              raise End_Error;
            end if;
            C := Raw_Get (Table_Slot);
            Saw_Content := True;
            exit when C = 12;
          end loop;
        end;
      File_Control_Blocks (Table_Slot).Col := 1;
      File_Control_Blocks (Table_Slot).Line := 1;
      File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
    end;
  procedure Skip_Page is
    begin
      Skip_Page ((Handle => Current_Input_Slot));
    end;
  function End_Of_Page (File : File_Type) return Boolean is
    Table_Slot : Integer := File.Handle;
    C0, C1     : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      C0 := Peek_N (Table_Slot, 0);
      if C0 < 0 or C0 = 12 then return True; end if;
      if C0 = 10 then
        C1 := Peek_N (Table_Slot, 1);
        return C1 < 0 or C1 = 12;
      end if;
      return False;
    end;
  function End_Of_Page return Boolean is
    begin
      return End_Of_Page ((Handle => Current_Input_Slot));
    end;
  function End_Of_File (File : File_Type) return Boolean is
    Table_Slot : Integer := File.Handle;
    C0, C1     : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      C0 := Peek_N (Table_Slot, 0);
      if C0 < 0 then return True; end if;
      if C0 = 12 then return Peek_N (Table_Slot, 1) < 0; end if;
      if C0 = 10 then
        C1 := Peek_N (Table_Slot, 1);
        if C1 < 0 then return True; end if;
        if C1 = 12 then return Peek_N (Table_Slot, 2) < 0; end if;
      end if;
      return False;
    end;
  function End_Of_File return Boolean is
    begin
      return End_Of_File ((Handle => Current_Input_Slot));
    end;
  procedure Set_Col (File : File_Type; To : Positive_Count) is
    Table_Slot : Integer := File.Handle;
    C          : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then
        loop
          if File_Control_Blocks (Table_Slot).Col > Integer (To) then
            Skip_Line (File);
          elsif File_Control_Blocks (Table_Slot).Col = Integer (To) then
            C := Raw_Peek (Table_Slot);
            exit when C /= 10 and C /= 12;
            Skip_Line (File);
          else
            C := Raw_Peek (Table_Slot);
            if C < 0 then raise End_Error; end if;
            if C = 10 or C = 12 then
              Skip_Line (File);
            else
              C := Raw_Get (Table_Slot);
              File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
            end if;
          end if;
        end loop;
      else
        if File_Control_Blocks (Table_Slot).Line_Length /= Unbounded and To > File_Control_Blocks (Table_Slot).Line_Length then
          raise Layout_Error;
        end if;
        if File_Control_Blocks (Table_Slot).Col > Integer (To) then
          New_Line (File);
        end if;
        while File_Control_Blocks (Table_Slot).Col < Integer (To) loop
          Raw_Put (Table_Slot, 32);
          File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
        end loop;
      end if;
    end;
  procedure Set_Col (To : Positive_Count) is
    begin
      Set_Col ((Handle => Current_Output_Slot), To);
    end;
  procedure Set_Line (File : File_Type; To : Positive_Count) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then
        loop
          exit when File_Control_Blocks (Table_Slot).Line = Integer (To);
          if File_Control_Blocks (Table_Slot).Line > Integer (To) then
            Skip_Page (File);
          else
            Skip_Line (File);
          end if;
        end loop;
      else
        if File_Control_Blocks (Table_Slot).Page_Length /= Unbounded and To > File_Control_Blocks (Table_Slot).Page_Length then
          raise Layout_Error;
        end if;
        if File_Control_Blocks (Table_Slot).Line > Integer (To) then
          New_Page (File);
        end if;
        while File_Control_Blocks (Table_Slot).Line < Integer (To) loop
          New_Line (File);
        end loop;
      end if;
    end;
  procedure Set_Line (To : Positive_Count) is
    begin
      Set_Line ((Handle => Current_Output_Slot), To);
    end;
  function Col (File : File_Type) return Positive_Count is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Col > Integer (Count'Last) then
        raise Layout_Error;
      end if;
      return Positive_Count (File_Control_Blocks (Table_Slot).Col);
    end;
  function Col return Positive_Count is
    begin
      return Col ((Handle => Current_Output_Slot));
    end;
  function Line (File : File_Type) return Positive_Count is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Line > Integer (Count'Last) then
        raise Layout_Error;
      end if;
      return Positive_Count (File_Control_Blocks (Table_Slot).Line);
    end;
  function Line return Positive_Count is
    begin
      return Line ((Handle => Current_Output_Slot));
    end;
  function Page (File : File_Type) return Positive_Count is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Page > Integer (Count'Last) then
        raise Layout_Error;
      end if;
      return Positive_Count (File_Control_Blocks (Table_Slot).Page);
    end;
  function Page return Positive_Count is
    begin
      return Page ((Handle => Current_Output_Slot));
    end;
  procedure Get (File : File_Type; Item : out Character) is
    Table_Slot : Integer := File.Handle;
    C          : Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      loop
        C := Raw_Get (Table_Slot);
        if C < 0 then
          raise End_Error;
        end if;
        exit when C /= 10 and C /= 12 and C /= 13;
        if C = 10 then
          File_Control_Blocks (Table_Slot).Col := 1;
          File_Control_Blocks (Table_Slot).Line := File_Control_Blocks (Table_Slot).Line + 1;
        elsif C = 12 then
          File_Control_Blocks (Table_Slot).Col := 1;
          File_Control_Blocks (Table_Slot).Line := 1;
          File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
        end if;
      end loop;
      Item := Character'Val (C);
      File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
    end;
  procedure Get (Item : out Character) is
    begin
      Get ((Handle => Current_Input_Slot), Item);
    end;
  procedure Put (File : File_Type; Item : Character) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then
        raise Mode_Error;
      end if;
      if File_Control_Blocks (Table_Slot).Line_Length /= Unbounded
          and File_Control_Blocks (Table_Slot).Col > Integer (File_Control_Blocks (Table_Slot).Line_Length) then
        New_Line (File);
      end if;
      Raw_Put (Table_Slot, Character'Pos (Item));
      File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
      File_Control_Blocks (Table_Slot).Page_Active := True;
    end;
  procedure Put (Item : Character) is
    begin
      Put ((Handle => Current_Output_Slot), Item);
    end;
  procedure Get (File : File_Type; Item : out String) is
    begin
      for I in Item'Range loop
        Get (File, Item (I));
      end loop;
    end;
  procedure Get (Item : out String) is
    begin
      Get ((Handle => Current_Input_Slot), Item);
    end;
  procedure Put (File : File_Type; Item : String) is
    Table_Slot : Integer := File.Handle;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode = In_File then
        raise Mode_Error;
      end if;
      if File_Control_Blocks (Table_Slot).Line_Length /= Unbounded or File_Control_Blocks (Table_Slot).Shared then
        for I in Item'Range loop
          Put (File, Item (I));
        end loop;
      elsif Item'Length > 0 then
        Raw_Write (Table_Slot, Item);
        File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + Item'Length;
        File_Control_Blocks (Table_Slot).Page_Active := True;
      end if;
    end;
  procedure Put (Item : String) is
    begin
      Put ((Handle => Current_Output_Slot), Item);
    end;
  procedure Get_Line (File : File_Type; Item : out String; Last : out Natural) is
    Table_Slot :          Integer := File.Handle;
    C          :          Integer;
    First_Slot : constant Integer := Integer (Item'First);
    Last_Slot  : constant Integer := Integer (Item'Last);
    I          :          Integer;
    begin
      Require_Open (Table_Slot);
      if File_Control_Blocks (Table_Slot).Mode /= In_File then
        raise Mode_Error;
      end if;
      Last := Natural (First_Slot - 1);
      I := First_Slot;
      while I <= Last_Slot loop
        C := Raw_Get (Table_Slot);
        if C < 0 then
          if I = First_Slot then
            raise End_Error;
          end if;
          exit;
        end if;
        if C = 10 then
          File_Control_Blocks (Table_Slot).Col := 1;
          File_Control_Blocks (Table_Slot).Line := File_Control_Blocks (Table_Slot).Line + 1;
          exit;
        end if;
        if C = 13 then
          null;
        elsif C = 12 then
          File_Control_Blocks (Table_Slot).Col := 1;
          File_Control_Blocks (Table_Slot).Line := 1;
          File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
          exit;
        else
          Item (I) := Character'Val (C);
          Last := Natural (I);
          I := I + 1;
          File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
        end if;
      end loop;
    end;
  procedure Get_Line (Item : out String; Last : out Natural) is
    begin
      Get_Line ((Handle => Current_Input_Slot), Item, Last);
    end;
  procedure Put_Line (File : File_Type; Item : String) is
    begin
      Put (File, Item);
      New_Line (File);
    end;
  procedure Put_Line (Item : String) is
    begin
      Put_Line ((Handle => Current_Output_Slot), Item);
    end;
  procedure Skip_Blanks_And_Terminators (Table_Slot : Integer) is
    C : Integer;
    begin
      loop
        C := Raw_Peek (Table_Slot);
        if C = 32 or C = 9 then
          C := Raw_Get (Table_Slot);
          File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
        elsif C = 10 then
          C := Raw_Get (Table_Slot);
          File_Control_Blocks (Table_Slot).Col := 1;
          File_Control_Blocks (Table_Slot).Line := File_Control_Blocks (Table_Slot).Line + 1;
        elsif C = 12 then
          C := Raw_Get (Table_Slot);
          File_Control_Blocks (Table_Slot).Col := 1;
          File_Control_Blocks (Table_Slot).Line := 1;
          File_Control_Blocks (Table_Slot).Page := File_Control_Blocks (Table_Slot).Page + 1;
        elsif C = 13 then
          C := Raw_Get (Table_Slot);
        else
          exit;
        end if;
      end loop;
    end;
  function Scan_String_Number (S : String; First : Integer; Allow_Point : Boolean) return Integer is
    I                                   :          Integer := First;
    Hi                                  : constant Integer := S'Last;
    Got, Based, Dot1, Dot2, Has_E, Frac :          Boolean;
    function Is_Dig (One_Character : Character) return Boolean is
      begin return One_Character >= '0' and One_Character <= '9'; end;
    function Is_Ext (One_Character : Character) return Boolean is
      begin
        return (One_Character >= '0' and One_Character <= '9') or (One_Character >= 'A' and One_Character <= 'F')
          or (One_Character >= 'a' and One_Character <= 'f');
      end;
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
      end;
    procedure Skip_Extended is
      After : Boolean := False;
      begin
        while I <= Hi loop
          if Is_Ext (S (I)) then After := True; I := I + 1;
          elsif S (I) = '_' and then After then After := False; I := I + 1;
          else exit; end if;
        end loop;
      end;
    procedure Match (C1, C2 : Character; Got : out Boolean) is
      begin
        if I <= Hi and then (S (I) = C1 or S (I) = C2) then
          I := I + 1; Got := True;
        else
          Got := False;
        end if;
      end;
    begin
      Match ('+', '-', Got);
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
        Match ('E', 'e', Has_E);
        if Has_E then Match ('+', '-', Got); Skip_Digits (Got); end if;
      elsif Allow_Point then
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
        Match ('E', 'e', Has_E);
        if Has_E then Match ('+', '-', Got); Skip_Digits (Got); end if;
      end if;
      return I - 1;
    end;
  procedure Read_Number_Token (Table_Slot  : Integer;
                               Width       : Integer;
                               Allow_Point : Boolean;
                               Scratch     : out String;
                               Text_Length : out Integer) is
    Local                                  : String (1..256);
    P                                      : Integer := 0;
    Loaded, Based, Dot1, Dot2, Has_E, Frac : Boolean;
    Hash_Char                              : Integer;
    Cc                                     : Integer;
    procedure Store (One_Character : Integer) is
      begin
        if P < Local'Last then P := P + 1; Local (P) := Character'Val (One_Character); end if;
        File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
      end;
    procedure Load_Char (C1, C2 : Integer; Got : out Boolean) is
      One_Character : Integer := Raw_Peek (Table_Slot);
      begin
        if One_Character = C1 or (C2 >= 0 and One_Character = C2) then
          Cc := Raw_Get (Table_Slot); Store (Cc); Got := True;
        else
          Got := False;
        end if;
      end;
    procedure Load_Digits (Got : out Boolean) is
      One_Character : Integer := Raw_Peek (Table_Slot);
      After_Digit   : Boolean;
      begin
        if One_Character >= 48 and One_Character <= 57 then
          Got := True; After_Digit := True;
          Cc := Raw_Get (Table_Slot); Store (Cc);
          loop
            One_Character := Raw_Peek (Table_Slot);
            if One_Character >= 48 and One_Character <= 57 then
              After_Digit := True; Cc := Raw_Get (Table_Slot); Store (Cc);
            elsif One_Character = 95 and After_Digit then
              After_Digit := False; Cc := Raw_Get (Table_Slot); Store (Cc);
            else
              exit;
            end if;
          end loop;
        else
          Got := False;
        end if;
      end;
    procedure Load_Extended is
      One_Character : Integer;
      After_Digit   : Boolean := False;
      begin
        loop
          One_Character := Raw_Peek (Table_Slot);
          if (One_Character >= 48 and One_Character <= 57) or (One_Character >= 65 and One_Character <= 70)
              or (One_Character >= 97 and One_Character <= 102) then
            After_Digit := True;
          elsif One_Character = 95 and After_Digit then
            After_Digit := False;
          else
            exit;
          end if;
          Cc := Raw_Get (Table_Slot); Store (Cc);
        end loop;
      end;
    begin
      if Width > 0 then
        Cc := Raw_Peek (Table_Slot);
        if Cc = 10 or Cc = 12 then raise Data_Error; end if;
        for I in 1..Width loop
          Cc := Raw_Peek (Table_Slot);
          exit when Cc < 0 or Cc = 10 or Cc = 12;
          Cc := Raw_Get (Table_Slot); Store (Cc);
        end loop;
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
            Text_Length := Hi - Lo + 1;
            if Hi >= Lo then Scratch (Scratch'First..Scratch'First + (Hi - Lo)) := Local (Lo..Hi); end if;
          end;
        return;
      end if;
      Skip_Blanks_And_Terminators (Table_Slot);
      Load_Char (43, 45, Loaded);
      Load_Digits (Loaded);
      if Loaded then
        Load_Char (35, 58, Based);
        if Based then
          Hash_Char := Character'Pos (Local (P));
          if Allow_Point then
            Load_Char (46, -1, Dot1);
            if Dot1 then
              Load_Extended;
              Load_Char (35, 58, Loaded);
            else
              Load_Extended;
              Load_Char (46, -1, Dot2);
              if Dot2 then Load_Extended; end if;
              Load_Char (35, 58, Loaded);
            end if;
          else
            Load_Extended;
            Load_Char (Hash_Char, -1, Loaded);
          end if;
          Load_Char (69, 101, Has_E);
          if Has_E then
            Load_Char (43, 45, Loaded);
            Load_Digits (Loaded);
          end if;
        elsif Allow_Point then
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
          Load_Char (69, 101, Has_E);
          if Has_E then
            Load_Char (43, 45, Loaded);
            Load_Digits (Loaded);
          end if;
        end if;
      end if;
      Text_Length := P;
      if P > 0 then Scratch (Scratch'First..Scratch'First + P - 1) := Local (1..P); end if;
    end;
  function First_Nonblank (From : String) return Integer is
    begin
      for I in From'Range loop
        if From (I) /= ' ' and From (I) /= Character'Val (9) then
          return I;
        end if;
      end loop;
      return From'Last + 1;
    end;
  function Without_Leading_Blank (S : String) return String is
    begin
      if S'Length > 0 and then S (S'First) = ' ' then
        return S (S'First + 1..S'Last);
      end if;
      return S;
    end;
  procedure Check_On_One_Line (File : File_Type; Length : Integer) is
    Table_Slot : Integer := File.Handle;
    begin
      if File_Control_Blocks (Table_Slot).Line_Length /= Unbounded then
        if Count (Length) > File_Control_Blocks (Table_Slot).Line_Length then
          raise Layout_Error;
        elsif File_Control_Blocks (Table_Slot).Col + Length > Integer (File_Control_Blocks (Table_Slot).Line_Length) + 1 then
          New_Line (File);
        end if;
      end if;
    end;
  procedure Put_Raw (File : File_Type; Item : String) is
    Table_Slot : Integer := File.Handle;
    begin
      if Item'Length = 0 then return; end if;
      if File_Control_Blocks (Table_Slot).Shared then
        for I in Item'Range loop
          Raw_Put (Table_Slot, Character'Pos (Item (I)));
        end loop;
      else
        Raw_Write (Table_Slot, Item);
      end if;
      File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + Item'Length;
      File_Control_Blocks (Table_Slot).Page_Active := True;
    end;
  procedure Put_Blanks (File : File_Type; Pad : Integer) is
    Table_Slot : Integer := File.Handle;
    begin
      for I in 1..Pad loop
        Raw_Put (Table_Slot, 32);
        File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
        File_Control_Blocks (Table_Slot).Page_Active := True;
      end loop;
    end;
  procedure Put_Right_Justified (File : File_Type; Item : String; Width : Integer) is
    Pad : Integer := Width - Item'Length;
    begin
      if Pad < 0 then Pad := 0; end if;
      Check_On_One_Line (File, Pad + Item'Length);
      Put_Blanks (File, Pad);
      Put_Raw (File, Item);
    end;
  procedure Put_Left_Justified (File : File_Type; Item : String; Width : Integer) is
    Pad : Integer := Width - Item'Length;
    begin
      if Pad < 0 then Pad := 0; end if;
      Check_On_One_Line (File, Item'Length + Pad);
      Put_Raw (File, Item);
      Put_Blanks (File, Pad);
    end;
  function Render_Real (Plain_Value : Long_Real; Fore, Aft, Exp : Integer) return String is
    Max_Significant_Digits : constant Integer := 128;
    Max_Exact_Digits       : constant Integer := 800;

    type Mantissa_Integer is range 0..2 ** 53;

    Is_Negative : constant Boolean   := Plain_Value < 0.0;
    Magnitude   :          Long_Real := abs Plain_Value;

    Decimal_Exponent : Integer := 0;

    Rounded_Digits : array (0..Max_Significant_Digits) of Integer := (others => 0);

    Significant_Digit_Count : Integer;
    Fraction_Digit_Count    : Integer := Integer'Max (Aft, 1);

    Rounding_Digit       : Integer;
    Carry                : Integer;
    Rounding_Index       : Integer;
    Integer_Digit_Count  : Integer;
    Sign_Character_Count : Integer;
    Source_Digit_Index   : Integer;

    Result_Buffer : String (1..2 * Max_Significant_Digits);
    Result_Length : Integer := 0;

    Mantissa        : Mantissa_Integer := 0;
    Binary_Exponent : Integer          := 0;

    -- Exact decimal digits, stored least-significant first.
    Exact_Digits : array (0..Max_Exact_Digits) of Integer := (others => 0);

    Exact_Digit_Count : Integer := 1;
    Decimal_Scale     : Integer := 0;

    Two_To_52 : constant Long_Real := 2.0 ** 52;
    Two_To_53 : constant Long_Real := 2.0 ** 53;

    begin
      if Magnitude /= 0.0 then
        while Magnitude < Two_To_52 loop
          Magnitude       := Magnitude * 2.0;
          Binary_Exponent := Binary_Exponent - 1;
        end loop;

        while Magnitude >= Two_To_53 loop
          Magnitude       := Magnitude / 2.0;
          Binary_Exponent := Binary_Exponent + 1;
        end loop;

        Mantissa := Mantissa_Integer (Magnitude);

        while Mantissa > 0 and then Mantissa mod 2 = 0 loop
          Mantissa       := Mantissa / 2;
          Binary_Exponent := Binary_Exponent + 1;
        end loop;

        Exact_Digit_Count := 0;

        declare
          Remaining_Mantissa : Mantissa_Integer := Mantissa;
          begin
            while Remaining_Mantissa > 0 loop
              Exact_Digits (Exact_Digit_Count) := Integer (Remaining_Mantissa mod 10);

              Exact_Digit_Count  := Exact_Digit_Count + 1;
              Remaining_Mantissa := Remaining_Mantissa / 10;
            end loop;
          end;

        declare
          Scale_Factor : Integer := 2;
          Scale_Count  : Integer := Binary_Exponent;
          begin
            if Binary_Exponent < 0 then
              Scale_Factor  := 5;
              Scale_Count   := -Binary_Exponent;
              Decimal_Scale := -Binary_Exponent;
            end if;

            for Scale_Step in 1..Scale_Count loop
              Carry := 0;

              for Digit_Index in 0..Exact_Digit_Count - 1 loop
                declare
                  Product : constant Integer := Exact_Digits (Digit_Index) * Scale_Factor + Carry;
                  begin
                    Exact_Digits (Digit_Index) := Product mod 10;
                    Carry                      := Product / 10;
                  end;
              end loop;

              while Carry > 0 loop
                Exact_Digits (Exact_Digit_Count) := Carry mod 10;
                Carry                            := Carry / 10;
                Exact_Digit_Count                := Exact_Digit_Count + 1;
              end loop;
            end loop;
          end;

        Decimal_Exponent := Exact_Digit_Count - 1 - Decimal_Scale;
      end if;

      if Exp > 0 then
        Significant_Digit_Count := Fraction_Digit_Count + 1;
      elsif Decimal_Exponent >= 0 then
        Significant_Digit_Count := Decimal_Exponent + 1 + Fraction_Digit_Count;
      else
        Significant_Digit_Count := Fraction_Digit_Count + Decimal_Exponent + 1;
      end if;

      Significant_Digit_Count := Integer'Max (0, Integer'Min (Significant_Digit_Count, Max_Significant_Digits));

      for Digit_Index in 0..Significant_Digit_Count - 1 loop
        if Digit_Index < Exact_Digit_Count then
          Rounded_Digits (Digit_Index) := Exact_Digits (Exact_Digit_Count - 1 - Digit_Index);
        else
          Rounded_Digits (Digit_Index) := 0;
        end if;
      end loop;

      if Significant_Digit_Count < Exact_Digit_Count then
        Rounding_Digit := Exact_Digits (Exact_Digit_Count - 1 - Significant_Digit_Count);
      else
        Rounding_Digit := 0;
      end if;

      if Rounding_Digit >= 5 then
        Carry          := 1;
        Rounding_Index := Significant_Digit_Count - 1;

        while Carry = 1 and then Rounding_Index >= 0 loop
          Rounded_Digits (Rounding_Index) := Rounded_Digits (Rounding_Index) + 1;

          if Rounded_Digits (Rounding_Index) >= 10 then
            Rounded_Digits (Rounding_Index) := 0;
          else
            Carry := 0;
          end if;

          Rounding_Index := Rounding_Index - 1;
        end loop;

        if Carry = 1 then
          for Digit_Index in reverse 1..Significant_Digit_Count - 1 loop
            Rounded_Digits (Digit_Index) := Rounded_Digits (Digit_Index - 1);
          end loop;

          if Significant_Digit_Count > 0 then
            Rounded_Digits (0) := 1;
          end if;

          Decimal_Exponent := Decimal_Exponent + 1;
        end if;
      end if;

      if Exp > 0 then
        Integer_Digit_Count := 1;
      elsif Decimal_Exponent >= 0 then
        Integer_Digit_Count := Decimal_Exponent + 1;
      else
        Integer_Digit_Count := 1;
      end if;

      Sign_Character_Count := Boolean'Pos (Is_Negative);

      for Padding_Position in Integer_Digit_Count + Sign_Character_Count + 1..Fore loop
        Result_Length                  := Result_Length + 1;
        Result_Buffer (Result_Length) := ' ';
      end loop;

      if Is_Negative then
        Result_Length                  := Result_Length + 1;
        Result_Buffer (Result_Length) := '-';
      end if;

      if Exp = 0 and then Decimal_Exponent < 0 then
        Result_Length                  := Result_Length + 1;
        Result_Buffer (Result_Length) := '0';
      else
        for Digit_Index in 0..Integer_Digit_Count - 1 loop
          Result_Length := Result_Length + 1;

          Result_Buffer (Result_Length) := Character'Val (Character'Pos ('0') + Rounded_Digits (Digit_Index));
        end loop;
      end if;

      Result_Length                  := Result_Length + 1;
      Result_Buffer (Result_Length) := '.';

      declare
        Decimal_Point_Index : Integer := 0;
        begin
          if Exp = 0 then
            Decimal_Point_Index := Decimal_Exponent;
          end if;

          for Fraction_Position in 1..Fraction_Digit_Count loop
            Source_Digit_Index := Decimal_Point_Index + Fraction_Position;

            Result_Length := Result_Length + 1;

            if Source_Digit_Index in Rounded_Digits'Range then
              Result_Buffer (Result_Length) := Character'Val (Character'Pos ('0') + Rounded_Digits (Source_Digit_Index));
            else
              Result_Buffer (Result_Length) := '0';
            end if;
          end loop;
        end;

      if Exp > 0 then
        Result_Length                  := Result_Length + 1;
        Result_Buffer (Result_Length) := 'E';

        if Decimal_Exponent < 0 then
          Result_Length                  := Result_Length + 1;
          Result_Buffer (Result_Length) := '-';
          Decimal_Exponent              := -Decimal_Exponent;
        else
          Result_Length                  := Result_Length + 1;
          Result_Buffer (Result_Length) := '+';
        end if;

        declare
          Exponent_Digits : array (0..15) of Integer := (others => 0);

          Exponent_Digit_Count : Integer := 0;
          Remaining_Exponent   : Integer := Decimal_Exponent;
          begin
            if Remaining_Exponent = 0 then
              Exponent_Digit_Count := 1;
            else
              while Remaining_Exponent > 0 loop
                Exponent_Digits (Exponent_Digit_Count) := Remaining_Exponent mod 10;

                Exponent_Digit_Count := Exponent_Digit_Count + 1;

                Remaining_Exponent := Remaining_Exponent / 10;
              end loop;
            end if;

            for Padding_Position in Exponent_Digit_Count + 1..Exp loop
              Result_Length                  := Result_Length + 1;
              Result_Buffer (Result_Length) := '0';
            end loop;

            for Digit_Index in reverse 0..Exponent_Digit_Count - 1 loop
              Result_Length := Result_Length + 1;

              Result_Buffer (Result_Length) := Character'Val (Character'Pos ('0') + Exponent_Digits (Digit_Index));
            end loop;
          end;
      end if;

      return Result_Buffer (1..Result_Length);
    end;
  procedure Format_Real (File : File_Type; Plain_Value : Long_Real; Fore, Aft, Exp : Integer) is
    Img : constant String := Render_Real (Plain_Value, Fore, Aft, Exp);
    begin
      Check_On_One_Line (File, Img'Length);
      Put_Raw (File, Img);
    end;
  function Hex_Digit_Value (C : Character) return Integer is
    begin
      if C >= '0' and C <= '9' then return Character'Pos (C) - 48;
      elsif C >= 'A' and C <= 'F' then return Character'Pos (C) - 55;
      elsif C >= 'a' and C <= 'f' then return Character'Pos (C) - 87;
      else return -1; end if;
    end;
  function Parse_Based_Real (S : String) return Long_Real is
    I       : Integer   := S'First;
    Last    : Integer   := S'Last;
    Neg     : Boolean   := False;
    Base    : Integer   := 0;
    Base_F  : Long_Real;
    Mant    : Long_Real := 0.0;
    Scale   : Long_Real;
    Marker  : Character;
    D       : Integer;
    Exp     : Integer   := 0;
    Exp_Neg : Boolean   := False;
    After   : Boolean;
    begin
      while I <= Last and then (S (I) = ' ' or S (I) = Character'Val (9)) loop I := I + 1; end loop;
      while Last >= I and then (S (Last) = ' ' or S (Last) = Character'Val (9)) loop Last := Last - 1; end loop;
      if I > Last then raise Constraint_Error; end if;
      if S (I) = '+' then I := I + 1; elsif S (I) = '-' then Neg := True; I := I + 1; end if;
      if I > Last or else not (S (I) >= '0' and S (I) <= '9') then raise Constraint_Error; end if;
      while I <= Last and then (S (I) >= '0' and S (I) <= '9') loop
        Base := Base * 10 + (Character'Pos (S (I)) - 48); I := I + 1;
      end loop;
      if Base < 2 or Base > 16 then raise Constraint_Error; end if;
      Base_F := Long_Real (Base);
      if I > Last then raise Constraint_Error; end if;
      Marker := S (I);
      if Marker /= '#' and Marker /= ':' then raise Constraint_Error; end if;
      I := I + 1;
      After := False;
      loop
        exit when I > Last;
        D := Hex_Digit_Value (S (I));
        if D >= 0 then
          if D >= Base then raise Constraint_Error; end if;
          Mant := Mant * Base_F + Long_Real (D); After := True; I := I + 1;
        elsif S (I) = '_' and After then After := False; I := I + 1;
        else exit; end if;
      end loop;
      if not After then raise Constraint_Error; end if;
      if I <= Last and then S (I) = '.' then
        I := I + 1; Scale := 1.0; After := False;
        loop
          exit when I > Last;
          D := Hex_Digit_Value (S (I));
          if D >= 0 then
            if D >= Base then raise Constraint_Error; end if;
            Scale := Scale / Base_F; Mant := Mant + Long_Real (D) * Scale;
            After := True; I := I + 1;
          elsif S (I) = '_' and After then After := False; I := I + 1;
          else exit; end if;
        end loop;
        if not After then raise Constraint_Error; end if;
      end if;
      if I > Last or else S (I) /= Marker then raise Constraint_Error; end if;
      I := I + 1;
      if I <= Last and then (S (I) = 'E' or S (I) = 'e') then
        I := I + 1;
        if I <= Last and then S (I) = '+' then I := I + 1;
        elsif I <= Last and then S (I) = '-' then Exp_Neg := True; I := I + 1; end if;
        After := False;
        loop
          exit when I > Last;
          if S (I) >= '0' and S (I) <= '9' then
            Exp := Exp * 10 + (Character'Pos (S (I)) - 48); After := True; I := I + 1;
          elsif S (I) = '_' and After then After := False; I := I + 1;
          else exit; end if;
        end loop;
        if not After then raise Constraint_Error; end if;
      end if;
      if I <= Last then raise Constraint_Error; end if;
      declare
        P : Long_Real := 1.0;
        begin
          for K in 1..Exp loop P := P * Base_F; end loop;
          if Exp_Neg then Mant := Mant / P; else Mant := Mant * P; end if;
        end;
      if Neg then Mant := -Mant; end if;
      return Mant;
    end;
  function Decimal_Real_Value (S : String) return Long_Real;
  pragma Import (C, Decimal_Real_Value, "__ada_float_value");
  function Real_Value (S : String) return Long_Real is
    Dot : Integer := 0;
    begin
      for I in S'Range loop
        if S (I) = '#' or S (I) = ':' then return Parse_Based_Real (S); end if;
      end loop;
      for I in S'Range loop
        if S (I) = '.' then Dot := I; exit; end if;
      end loop;
      if Dot = 0
          or else Dot = S'First or else S (Dot - 1) < '0' or else S (Dot - 1) > '9'
          or else Dot = S'Last or else S (Dot + 1) < '0' or else S (Dot + 1) > '9'
        then
        raise Data_Error;
      end if;
      return Decimal_Real_Value (S);
    end;
  package body Integer_IO is
    function Image_In_Base (Item : Number; Base : Number_Base) return String is
      Digit_Set : constant String  := "0123456789ABCDEF";
      Temporary :          String (1..130);
      T         :          Integer := 0;
      W         :          Number  := Item;
      D         :          Integer;
      begin
        if Base = 10 then
          return Without_Leading_Blank (Number'Image (Item));
        end if;
        if W = 0 then
          T := 1; Temporary (1) := '0';
        else
          while W /= 0 loop
            D := Integer (W rem Number (Base));
            if D < 0 then D := -D; end if;
            T := T + 1;
            Temporary (T) := Digit_Set (D + 1);
            W := W / Number (Base);
          end loop;
        end if;
        declare
          Base_Img : constant String  := Without_Leading_Blank (Integer'Image (Integer (Base)));
          Neg      :          Integer := 0;
          begin
            if Item < 0 then Neg := 1; end if;
            declare
              Result : String (1..Neg + Base_Img'Length + 2 + T);
              P      : Integer := 1;
              begin
                if Neg = 1 then Result (P) := '-'; P := P + 1; end if;
                for I in Base_Img'Range loop
                  Result (P) := Base_Img (I); P := P + 1;
                end loop;
                Result (P) := '#'; P := P + 1;
                for I in reverse 1..T loop
                  Result (P) := Temporary (I); P := P + 1;
                end loop;
                Result (P) := '#';
                return Result;
              end;
          end;
      end;
    procedure Get (File : File_Type; Item : out Number; Width : Field := 0) is
      Table_Slot  : Integer := File.Handle;
      Scratch     : String (1..256);
      Text_Length : Integer;
      begin
        Require_Open (Table_Slot);
        if File_Control_Blocks (Table_Slot).Mode /= In_File then raise Mode_Error; end if;
        Read_Number_Token (Table_Slot, Integer (Width), False, Scratch, Text_Length);
        if Text_Length = 0 then
          if Raw_Peek (Table_Slot) < 0 then raise End_Error; else raise Data_Error; end if;
        end if;
        declare
          V : Number;
          begin
            V := Number'Value (Scratch (1..Text_Length));
            if V < Number'First or V > Number'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
      end;
    procedure Get (Item : out Number; Width : Field := 0) is
      begin
        Get (File_Type'(Handle => Current_Input_Slot), Item, Width);
      end;
    procedure Put (File : File_Type; Item : Number; Width : Field := Default_Width; Base : Number_Base := Default_Base) is
      begin
        Require_Open (File.Handle);
        if File_Control_Blocks (File.Handle).Mode = In_File then raise Mode_Error; end if;
        Put_Right_Justified (File, Image_In_Base (Item, Base), Integer (Width));
      end;
    procedure Put (Item : Number; Width : Field := Default_Width; Base : Number_Base := Default_Base) is
      begin
        Put ((Handle => Current_Output_Slot), Item, Width, Base);
      end;
    procedure Get (From : String; Item : out Number; Last : out Positive) is
      Token_Start : constant Integer := First_Nonblank (From);
      Token_End   :          Integer;
      begin
        if Token_Start > From'Last then raise End_Error; end if;
        Token_End := Scan_String_Number (From, Token_Start, False);
        declare
          V : Number;
          begin
            V := Number'Value (From (Token_Start..Token_End));
            if V < Number'First or V > Number'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
        Last := Token_End;
      end;
    procedure Put (To : out String; Item : Number; Base : Number_Base := Default_Base) is
      Img : constant String := Image_In_Base (Item, Base);
      begin
        if Img'Length > To'Length then raise Layout_Error; end if;
        for I in To'Range loop To (I) := ' '; end loop;
        To (To'Last - Img'Length + 1..To'Last) := Img;
      end;
  end;
  package body Float_IO is
    procedure Get (File : File_Type; Item : out Number; Width : Field := 0) is
      Table_Slot  : Integer := File.Handle;
      Scratch     : String (1..256);
      Text_Length : Integer;
      begin
        Require_Open (Table_Slot);
        if File_Control_Blocks (Table_Slot).Mode /= In_File then raise Mode_Error; end if;
        Read_Number_Token (Table_Slot, Integer (Width), True, Scratch, Text_Length);
        if Text_Length = 0 then
          if Raw_Peek (Table_Slot) < 0 then raise End_Error; else raise Data_Error; end if;
        end if;
        declare
          V : Number;
          begin
            V := Number (Real_Value (Scratch (1..Text_Length)));
            if V < Number'First or V > Number'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
      end;
    procedure Get (Item : out Number; Width : Field := 0) is
      begin
        Get (File_Type'(Handle => Current_Input_Slot), Item, Width);
      end;
    procedure Put (File : File_Type;
                   Item : Number;
                   Fore : Field := Default_Fore;
                   Aft  : Field := Default_Aft;
                   Exp  : Field := Default_Exp) is
      begin
        Require_Open (File.Handle);
        if File_Control_Blocks (File.Handle).Mode = In_File then raise Mode_Error; end if;
        Format_Real (File, Long_Real (Item), Integer (Fore), Integer (Aft), Integer (Exp));
      end;
    procedure Put (Item : Number; Fore : Field := Default_Fore; Aft : Field := Default_Aft; Exp : Field := Default_Exp) is
      begin
        Put ((Handle => Current_Output_Slot), Item, Fore, Aft, Exp);
      end;
    procedure Get (From : String; Item : out Number; Last : out Positive) is
      Token_Start : constant Integer := First_Nonblank (From);
      Token_End   :          Integer;
      begin
        if Token_Start > From'Last then raise End_Error; end if;
        Token_End := Scan_String_Number (From, Token_Start, True);
        declare
          V : Number;
          begin
            V := Number (Real_Value (From (Token_Start..Token_End)));
            if V < Number'First or V > Number'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
        Last := Token_End;
      end;
    procedure Put (To : out String; Item : Number; Aft : Field := Default_Aft; Exp : Field := Default_Exp) is
      Img : constant String := Render_Real (Long_Real (Item), 1, Integer (Aft), Integer (Exp));
      begin
        if Img'Length > To'Length then raise Layout_Error; end if;
        for I in To'Range loop To (I) := ' '; end loop;
        To (To'Last - Img'Length + 1..To'Last) := Img;
      end;
  end;
  package body Fixed_IO is
    procedure Get (File : File_Type; Item : out Number; Width : Field := 0) is
      Table_Slot  : Integer := File.Handle;
      Scratch     : String (1..256);
      Text_Length : Integer;
      begin
        Require_Open (Table_Slot);
        if File_Control_Blocks (Table_Slot).Mode /= In_File then raise Mode_Error; end if;
        Read_Number_Token (Table_Slot, Integer (Width), True, Scratch, Text_Length);
        if Text_Length = 0 then
          if Raw_Peek (Table_Slot) < 0 then raise End_Error; else raise Data_Error; end if;
        end if;
        declare
          V : Number;
          begin
            V := Number (Real_Value (Scratch (1..Text_Length)));
            if V < Number'First or V > Number'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
      end;
    procedure Get (Item : out Number; Width : Field := 0) is
      begin
        Get (File_Type'(Handle => Current_Input_Slot), Item, Width);
      end;
    procedure Put (File : File_Type;
                   Item : Number;
                   Fore : Field := Default_Fore;
                   Aft  : Field := Default_Aft;
                   Exp  : Field := Default_Exp) is
      begin
        Require_Open (File.Handle);
        if File_Control_Blocks (File.Handle).Mode = In_File then raise Mode_Error; end if;
        Format_Real (File, Long_Real (Item), Integer (Fore), Integer (Aft), Integer (Exp));
      end;
    procedure Put (Item : Number; Fore : Field := Default_Fore; Aft : Field := Default_Aft; Exp : Field := Default_Exp) is
      begin
        Put ((Handle => Current_Output_Slot), Item, Fore, Aft, Exp);
      end;
    procedure Get (From : String; Item : out Number; Last : out Positive) is
      Token_Start : constant Integer := First_Nonblank (From);
      Token_End   :          Integer;
      begin
        if Token_Start > From'Last then raise End_Error; end if;
        Token_End := Scan_String_Number (From, Token_Start, True);
        declare
          V : Number;
          begin
            V := Number (Real_Value (From (Token_Start..Token_End)));
            if V < Number'First or V > Number'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
        Last := Token_End;
      end;
    procedure Put (To : out String; Item : Number; Aft : Field := Default_Aft; Exp : Field := Default_Exp) is
      Img : constant String := Render_Real (Long_Real (Item), 1, Integer (Aft), Integer (Exp));
      begin
        if Img'Length > To'Length then raise Layout_Error; end if;
        for I in To'Range loop To (I) := ' '; end loop;
        To (To'Last - Img'Length + 1..To'Last) := Img;
      end;
  end;
  package body Enumeration_IO is
    function Cased_Image (Item : Enum; Set : Type_Set) return String is
      Raw : constant String             := Enum'Image (Item);
      Img :          String (Raw'Range) := Raw;
      begin
        if Img'Length > 0 and then Img (Img'First) = ''' then
          return Img;
        end if;
        if Set = Lower_Case then
          for I in Img'Range loop
            if Img (I) >= 'A' and Img (I) <= 'Z' then
              Img (I) := Character'Val (Character'Pos (Img (I)) + 32);
            end if;
          end loop;
        end if;
        return Img;
      end;
    procedure Get (File : File_Type; Item : out Enum) is
      Table_Slot : Integer := File.Handle;
      Scratch    : String (1..256);
      P          : Integer := 0;
      C          : Integer;
      begin
        Require_Open (Table_Slot);
        if File_Control_Blocks (Table_Slot).Mode /= In_File then raise Mode_Error; end if;
        Skip_Blanks_And_Terminators (Table_Slot);
        declare
          procedure Take is
            One_Character : Integer := Raw_Get (Table_Slot);
            begin
              P := P + 1; Scratch (P) := Character'Val (One_Character);
              File_Control_Blocks (Table_Slot).Col := File_Control_Blocks (Table_Slot).Col + 1;
            end;
          begin
            C := Raw_Peek (Table_Slot);
            if C = 39 then
              Take;
              C := Raw_Peek (Table_Slot);
              if (C >= 32 and C <= 126) or C >= 128 then
                Take;
                if Raw_Peek (Table_Slot) = 39 then Take; end if;
              end if;
            elsif (C >= 65 and C <= 90) or (C >= 97 and C <= 122) then
              loop
                Take;
                C := Raw_Peek (Table_Slot);
                exit when not ((C >= 65 and C <= 90) or (C >= 97 and C <= 122) or (C >= 48 and C <= 57) or C = 95);
                exit when C = 95 and Scratch (P) = '_';
              end loop;
            end if;
          end;
        if P = 0 then
          if Raw_Peek (Table_Slot) < 0 then raise End_Error; else raise Data_Error; end if;
        end if;
        declare
          V : Enum;
          begin
            V := Enum'Value (Scratch (1..P));
            if V < Enum'First or V > Enum'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
      end;
    procedure Get (Item : out Enum) is
      begin
        Get ((Handle => Current_Input_Slot), Item);
      end;
    procedure Put (File : File_Type; Item : Enum; Width : Field := Default_Width; Set : Type_Set := Default_Setting) is
      begin
        Require_Open (File.Handle);
        if File_Control_Blocks (File.Handle).Mode = In_File then raise Mode_Error; end if;
        Put_Left_Justified (File, Cased_Image (Item, Set), Integer (Width));
      end;
    procedure Put (Item : Enum; Width : Field := Default_Width; Set : Type_Set := Default_Setting) is
      begin
        Put ((Handle => Current_Output_Slot), Item, Width, Set);
      end;
    procedure Get (From : String; Item : out Enum; Last : out Positive) is
      Token_Start : constant Integer := First_Nonblank (From);
      Token_End   :          Integer;
      I           :          Integer := Token_Start;
      begin
        if Token_Start > From'Last then raise End_Error; end if;
        declare
          function Class (J : Integer) return Integer is
            Cp : Integer := Character'Pos (From (J));
            begin
              if (Cp >= 65 and Cp <= 90) or (Cp >= 97 and Cp <= 122) then return 2;
              elsif Cp >= 48 and Cp <= 57 then return 1;
              elsif Cp = 95 then return 3;
              else return 0; end if;
            end;
          begin
            if From (I) = ''' then
              I := I + 1;
              if I <= From'Last
                  and then ((Character'Pos (From (I)) >= 32 and Character'Pos (From (I)) <= 126)
                            or Character'Pos (From (I)) >= 128) then
                I := I + 1;
                if I <= From'Last and then From (I) = ''' then I := I + 1; end if;
              end if;
            elsif Class (I) = 2 then
              loop
                I := I + 1;
                exit when I > From'Last or else Class (I) = 0;
                exit when Class (I) = 3 and From (I - 1) = '_';
              end loop;
            end if;
          end;
        Token_End := I - 1;
        declare
          V : Enum;
          begin
            V := Enum'Value (From (Token_Start..Token_End));
            if V < Enum'First or V > Enum'Last then raise Data_Error; end if;
            Item := V;
          exception when others => raise Data_Error;
          end;
        Last := Token_End;
      end;
    procedure Put (To : out String; Item : Enum; Set : Type_Set := Default_Setting) is
      Img : constant String := Cased_Image (Item, Set);
      begin
        if Img'Length > To'Length then raise Layout_Error; end if;
        for I in To'Range loop To (I) := ' '; end loop;
        To (To'First..To'First + Img'Length - 1) := Img;
      end;
  end;
begin
  Init_Standard_Files;
  Initialized := True;
end;
package Calendar is
  type Time is private;
  subtype Year_Number is Integer range 1901..2099;
  subtype Month_Number is Integer range 1..12;
  subtype Day_Number is Integer range 1..31;
  subtype Day_Duration is Duration range 0.0..86_400.0;
  function "+" (Left : Duration; Right : Time) return Time;
  function "+" (Left : Time; Right : Duration) return Time;
  function "-" (Left : Time; Right : Duration) return Time;
  function "-" (Left : Time; Right : Time) return Duration;
  function "<" (Left, Right : Time) return Boolean;
  function "<=" (Left, Right : Time) return Boolean;
  function ">" (Left, Right : Time) return Boolean;
  function ">=" (Left, Right : Time) return Boolean;
  function Clock return Time;
  function Day (Date : Time) return Day_Number;
  function Month (Date : Time) return Month_Number;
  function Seconds (Date : Time) return Day_Duration;
  procedure Split (Date    : Time;
                   Year    : out Year_Number;
                   Month   : out Month_Number;
                   Day     : out Day_Number;
                   Seconds : out Day_Duration);
  Time_Error : exception;
  function Time_Of (Year : Year_Number; Month : Month_Number; Day : Day_Number; Seconds : Day_Duration := 0.0) return Time;
  function Year (Date : Time) return Year_Number;
private
  type Time is new Duration;
end;
package body Calendar is
  function Rt_Time return Duration;
  pragma Import (Llvm, Rt_Time, "__ada_clock");
  Epoch       : constant := 2440588;
  Day_Seconds : constant Duration := 86_400.0;
  Time_First  :          Duration;
  Time_Last   :          Duration;
  function Julian_Day (Y : Year_Number; M : Month_Number; D : Day_Number) return Integer is
    A  : Integer := (14 - M) / 12;
    Yy : Integer := Y + 4800 - A;
    Mm : Integer := M + 12 * A - 3;
    begin
      return D + (153 * Mm + 2) / 5 + 365 * Yy + Yy / 4 - Yy / 100 + Yy / 400 - 32045;
    end;
  function Day_Count (Date : Time) return Integer is
    D : Duration := Duration (Date);
    N : Integer  := Integer (D / Day_Seconds);
    begin
      while N * Day_Seconds > D loop N := N - 1; end loop;
      return N;
    end;
  function Clock return Time is begin return Time (Rt_Time); end;
  procedure Split (Date    : Time;
                   Year  : out Year_Number;
                   Month : out Month_Number;
                   Day   : out Day_Number;
                   Seconds : out Day_Duration) is
    Days : Integer := Day_Count (Date);
    Jd   : Integer := Epoch + Days;
    A    : Integer := Jd + 32044;
    B    : Integer := (4 * A + 3) / 146097;
    C    : Integer := A - 146097 * B / 4;
    Dd   : Integer := (4 * C + 3) / 1461;
    E    : Integer := C - 1461 * Dd / 4;
    M    : Integer := (5 * E + 2) / 153;
    begin
      Day := E - (153 * M + 2) / 5 + 1;
      Month := M + 3 - 12 * (M / 10);
      Year := 100 * B + Dd - 4800 + M / 10;
      Seconds := Duration (Date) - Days * Day_Seconds;
    end;
  function Year (Date : Time) return Year_Number is
    Y : Year_Number; M : Month_Number; D : Day_Number; S : Day_Duration;
    begin
      Split (Date, Y, M, D, S);
      return Y;
    end;
  function Month (Date : Time) return Month_Number is
    Y : Year_Number; M : Month_Number; D : Day_Number; S : Day_Duration;
    begin
      Split (Date, Y, M, D, S);
      return M;
    end;
  function Day (Date : Time) return Day_Number is
    Y : Year_Number; M : Month_Number; D : Day_Number; S : Day_Duration;
    begin
      Split (Date, Y, M, D, S);
      return D;
    end;
  function Seconds (Date : Time) return Day_Duration is
    Y : Year_Number; M : Month_Number; D : Day_Number; S : Day_Duration;
    begin
      Split (Date, Y, M, D, S);
      return S;
    end;
  function Time_Of (Year : Year_Number; Month : Month_Number; Day : Day_Number; Seconds : Day_Duration := 0.0) return Time is
    Days_In_Month : Integer;
    begin
      case Month is
        when 4 | 6 | 9 | 11 => Days_In_Month := 30;
        when 2 =>
          if Year mod 4 = 0 and then (Year mod 100 /= 0 or else Year mod 400 = 0) then
            Days_In_Month := 29;
          else
            Days_In_Month := 28;
          end if;
        when others => Days_In_Month := 31;
      end case;
      if Day > Days_In_Month then raise Time_Error; end if;
      return Time ((Julian_Day (Year, Month, Day) - Epoch) * Day_Seconds + Seconds);
    end;
  function "+" (Left : Time; Right : Duration) return Time is
    L : Duration := Duration (Left);
    begin
      if Right >= 0.0 then
        if L > Time_Last - Right then raise Time_Error; end if;
      else
        if L < Time_First - Right then raise Time_Error; end if;
      end if;
      return Time (L + Right);
    end;
  function "+" (Left : Duration; Right : Time) return Time is
    begin
      return Right + Left;
    end;
  function "-" (Left : Time; Right : Duration) return Time is
    L : Duration := Duration (Left);
    begin
      if Right >= 0.0 then
        if L < Time_First + Right then raise Time_Error; end if;
      else
        if L > Time_Last + Right then raise Time_Error; end if;
      end if;
      return Time (L - Right);
    end;
  function "-" (Left : Time; Right : Time) return Duration is
    begin
      return Duration (Left) - Duration (Right);
    end;
  function "<" (Left, Right : Time) return Boolean is begin return Duration (Left) < Duration (Right); end;
  function "<=" (Left, Right : Time) return Boolean is begin return Duration (Left) <= Duration (Right); end;
  function ">" (Left, Right : Time) return Boolean is begin return Duration (Left) > Duration (Right); end;
  function ">=" (Left, Right : Time) return Boolean is begin return Duration (Left) >= Duration (Right); end;
begin
  Time_First := (Julian_Day (1901, 1, 1) - Epoch) * Day_Seconds;
  Time_Last := (Julian_Day (2099, 12, 31) - Epoch) * Day_Seconds + Day_Seconds;
end;
generic
  type Element_Type is private;
package Direct_IO is
  type File_Type is limited private;
  type File_Mode is (In_File, Inout_File, Out_File);
  type Count is range 0..Integer'Last;
  subtype Positive_Count is Count range 1..Count'Last;
  Status_Error : exception;
  Mode_Error   : exception;
  Name_Error   : exception;
  Use_Error    : exception;
  Device_Error : exception;
  End_Error    : exception;
  Data_Error   : exception;
  procedure Create (File : in out File_Type; Mode : File_Mode := Inout_File; Name : String := ""; Form : String := "");
  procedure Open (File : in out File_Type; Mode : File_Mode; Name : String; Form : String := "");
  procedure Close (File : in out File_Type);
  procedure Delete (File : in out File_Type);
  procedure Reset (File : in out File_Type; Mode : File_Mode);
  procedure Reset (File : in out File_Type);
  function Mode (File : File_Type) return File_Mode;
  function Name (File : File_Type) return String;
  function Form (File : File_Type) return String;
  function Is_Open (File : File_Type) return Boolean;
  procedure Read (File : File_Type; Item : out Element_Type; From : Positive_Count);
  procedure Read (File : File_Type; Item : out Element_Type);
  procedure Write (File : File_Type; Item : Element_Type; To : Positive_Count);
  procedure Write (File : File_Type; Item : Element_Type);
  procedure Set_Index (File : File_Type; To : Positive_Count);
  function Index (File : File_Type) return Positive_Count;
  function Size (File : File_Type) return Count;
  function End_Of_File (File : File_Type) return Boolean;
private
  type File_Type is record
      Handle : Integer := 0;
    end record;
end;
with System;
package body Direct_IO is
  function C_Fopen (Name : System.Address; Mode : System.Address) return System.Address;
  pragma Import (C, C_Fopen, "fopen");
  function C_Fclose (Stream : System.Address) return Integer;
  pragma Import (C, C_Fclose, "fclose");
  function C_Fread (Pointer : System.Address; Size : Integer; Count : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Fread, "fread");
  function C_Fwrite (Pointer : System.Address; Size : Integer; Count : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Fwrite, "fwrite");
  function C_Remove (Name : System.Address) return Integer;
  pragma Import (C, C_Remove, "remove");
  function C_Fseek (Stream : System.Address; Offset : Integer; Whence : Integer) return Integer;
  pragma Import (C, C_Fseek, "fseek");
  function C_Ftell (Stream : System.Address) return Integer;
  pragma Import (C, C_Ftell, "ftell");
  function C_Fflush (Stream : System.Address) return Integer;
  pragma Import (C, C_Fflush, "fflush");
  function C_Tmpfile return System.Address;
  pragma Import (C, C_Tmpfile, "tmpfile");
  Null_Address : constant System.Address := System.Null_Address;
  Seek_Set     : constant Integer        := 0;
  Seek_End     : constant Integer        := 2;
  Header_Bytes : constant Integer        := 4;
  type Control_Block is record
      Stream      : System.Address := Null_Address;
      Mode        : File_Mode      := Inout_File;
      Is_Open     : Boolean        := False;
      Index       : Positive_Count := 1;
      Slot        : Integer        := 0;
      Name_Length : Natural        := 0;
      Name        : String (1..1024);
    end record;
  File_Control_Blocks : array (1..99) of Control_Block;
  function Is_Open_Index (Table_Slot : Integer) return Boolean is
    begin
      return Table_Slot >= 1 and then Table_Slot <= 99 and then File_Control_Blocks (Table_Slot).Is_Open;
    end;
  function Require_Open (File : File_Type) return Integer is
    Table_Slot : Integer := File.Handle;
    begin
      if not Is_Open_Index (Table_Slot) then raise Status_Error; end if;
      return Table_Slot;
    end;
  procedure To_C_String (S : String; Buffer : out String) is
    J : Integer := 1;
    begin
      for I in S'First..S'Last loop
        Buffer (J) := S (I);
        J := J + 1;
      end loop;
      Buffer (J) := Character'Val (0);
    end;
  procedure Seek_To (Table_Slot : Integer; Element_Index : Positive_Count) is
    Offset : Integer := Header_Bytes + Integer (Element_Index - 1) * (4 + File_Control_Blocks (Table_Slot).Slot);
    Ignore : Integer;
    begin
      Ignore := C_Fseek (File_Control_Blocks (Table_Slot).Stream, Offset, Seek_Set);
    end;
  function Element_Count (Table_Slot : Integer) return Count is
    Ignore : Integer;
    Bytes  : Integer;
    begin
      if File_Control_Blocks (Table_Slot).Slot = 0 then return 0; end if;
      Ignore := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
      Ignore := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, Seek_End);
      Bytes := C_Ftell (File_Control_Blocks (Table_Slot).Stream);
      if Bytes <= Header_Bytes then return 0; end if;
      return Count ((Bytes - Header_Bytes) / (4 + File_Control_Blocks (Table_Slot).Slot));
    end;
  procedure Establish_Slot (Table_Slot : Integer; Item_Bytes : Integer) is
    Header : Integer := Item_Bytes;
    Ignore : Integer;
    begin
      if File_Control_Blocks (Table_Slot).Slot = 0 then
        File_Control_Blocks (Table_Slot).Slot := Item_Bytes;
        Ignore := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, Seek_Set);
        if C_Fwrite (Header'Address, Header_Bytes, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
          raise Device_Error;
        end if;
      elsif File_Control_Blocks (Table_Slot).Slot /= Item_Bytes then
        raise Use_Error;
      end if;
    end;
  procedure Recover_Slot (Table_Slot : Integer) is
    Header : Integer := 0;
    Ignore : Integer;
    begin
      Ignore := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, Seek_Set);
      if C_Fread (Header'Address, Header_Bytes, 1, File_Control_Blocks (Table_Slot).Stream) = 1 then
        File_Control_Blocks (Table_Slot).Slot := Header;
      end if;
    end;
  procedure Attach (File : in out File_Type; Mode : File_Mode; Name : String; Creating : Boolean) is
    Table_Slot  : Integer;
    Name_Buffer : String (1..1026);
    Mode_Text   : String (1..4);
    begin
      if Is_Open_Index (File.Handle) then raise Status_Error; end if;
      Table_Slot := 0;
      for J in File_Control_Blocks'Range loop
        if not File_Control_Blocks (J).Is_Open then Table_Slot := J; exit; end if;
      end loop;
      if Table_Slot = 0 then raise Use_Error; end if;
      if not Creating and then Name'Length = 0 then raise Name_Error; end if;
      if Creating then
        Mode_Text := ('w', '+', 'b', Character'Val (0));
      elsif Mode = In_File then
        Mode_Text := ('r', 'b', Character'Val (0), Character'Val (0));
      else
        Mode_Text := ('r', '+', 'b', Character'Val (0));
      end if;
      if Name'Length > 0 then
        To_C_String (Name, Name_Buffer);
        File_Control_Blocks (Table_Slot).Stream := C_Fopen (Name_Buffer'Address, Mode_Text'Address);
        if File_Control_Blocks (Table_Slot).Stream = Null_Address then
          if Creating then raise Use_Error; else raise Name_Error; end if;
        end if;
        File_Control_Blocks (Table_Slot).Name_Length := Name'Length;
        File_Control_Blocks (Table_Slot).Name (1..Name'Length) := Name;
      else
        File_Control_Blocks (Table_Slot).Stream := C_Tmpfile;
        if File_Control_Blocks (Table_Slot).Stream = Null_Address then raise Use_Error; end if;
        File_Control_Blocks (Table_Slot).Name_Length := 0;
      end if;
      File_Control_Blocks (Table_Slot).Mode := Mode;
      File_Control_Blocks (Table_Slot).Is_Open := True;
      File_Control_Blocks (Table_Slot).Index := 1;
      File_Control_Blocks (Table_Slot).Slot := 0;
      if not Creating then
        Recover_Slot (Table_Slot);
      end if;
      File := (Handle => Table_Slot);
    end;
  procedure Create (File : in out File_Type; Mode : File_Mode := Inout_File; Name : String := ""; Form : String := "") is
    begin
      Attach (File, Mode, Name, Creating => True);
    end;
  procedure Open (File : in out File_Type; Mode : File_Mode; Name : String; Form : String := "") is
    begin
      Attach (File, Mode, Name, Creating => False);
    end;
  procedure Close (File : in out File_Type) is
    Table_Slot : Integer := Require_Open (File);
    Ignore     : Integer;
    begin
      Ignore := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
      File_Control_Blocks (Table_Slot).Stream := Null_Address;
      File_Control_Blocks (Table_Slot).Is_Open := False;
      File := (Handle => 0);
    end;
  procedure Delete (File : in out File_Type) is
    Table_Slot  : Integer := Require_Open (File);
    Name_Buffer : String (1..1026);
    Ignore      : Integer;
    begin
      Ignore := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
      if File_Control_Blocks (Table_Slot).Name_Length > 0 then
        To_C_String (File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length), Name_Buffer);
        Ignore := C_Remove (Name_Buffer'Address);
      end if;
      File_Control_Blocks (Table_Slot).Stream := Null_Address;
      File_Control_Blocks (Table_Slot).Is_Open := False;
      File := (Handle => 0);
    end;
  procedure Reset (File : in out File_Type; Mode : File_Mode) is
    Table_Slot : Integer := Require_Open (File);
    Ignore     : Integer;
    begin
      Ignore := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
      Ignore := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, Seek_Set);
      File_Control_Blocks (Table_Slot).Mode := Mode;
      File_Control_Blocks (Table_Slot).Index := 1;
    end;
  procedure Reset (File : in out File_Type) is
    begin
      Reset (File, Mode (File));
    end;
  function Mode (File : File_Type) return File_Mode is
    Table_Slot : Integer := Require_Open (File);
    begin
      return File_Control_Blocks (Table_Slot).Mode;
    end;
  function Name (File : File_Type) return String is
    Table_Slot : Integer := Require_Open (File);
    begin
      if File_Control_Blocks (Table_Slot).Name_Length = 0 then raise Use_Error; end if;
      return File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length);
    end;
  function Form (File : File_Type) return String is
    Table_Slot : Integer := Require_Open (File);
    begin
      return "";
    end;
  function Is_Open (File : File_Type) return Boolean is
    begin
      return Is_Open_Index (File.Handle);
    end;
  procedure Read_At (Table_Slot : Integer; Item : out Element_Type; From : Positive_Count) is
    Stored_Bytes : Integer := 0;
    Item_Bytes   : Integer := (Item'Size + 7) / 8;
    begin
      if File_Control_Blocks (Table_Slot).Mode = Out_File then raise Mode_Error; end if;
      Seek_To (Table_Slot, From);
      if C_Fread (Stored_Bytes'Address, 4, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise End_Error;
      end if;
      if Stored_Bytes /= Item_Bytes then
        File_Control_Blocks (Table_Slot).Index := From + 1;
        raise Data_Error;
      end if;
      if C_Fread (Item'Address, Stored_Bytes, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise End_Error;
      end if;
      File_Control_Blocks (Table_Slot).Index := From + 1;
    end;
  procedure Read (File : File_Type; Item : out Element_Type; From : Positive_Count) is
    Table_Slot : Integer := Require_Open (File);
    begin
      Read_At (Table_Slot, Item, From);
    end;
  procedure Read (File : File_Type; Item : out Element_Type) is
    Table_Slot : Integer := Require_Open (File);
    begin
      Read_At (Table_Slot, Item, File_Control_Blocks (Table_Slot).Index);
    end;
  procedure Write_At (Table_Slot : Integer; Item : Element_Type; To : Positive_Count) is
    Item_Bytes : Integer := (Item'Size + 7) / 8;
    Ignore     : Integer;
    begin
      if File_Control_Blocks (Table_Slot).Mode = In_File then raise Mode_Error; end if;
      Establish_Slot (Table_Slot, Item_Bytes);
      Seek_To (Table_Slot, To);
      if C_Fwrite (Item_Bytes'Address, 4, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise Device_Error;
      end if;
      if C_Fwrite (Item'Address, Item_Bytes, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise Device_Error;
      end if;
      Ignore := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
      File_Control_Blocks (Table_Slot).Index := To + 1;
    end;
  procedure Write (File : File_Type; Item : Element_Type; To : Positive_Count) is
    Table_Slot : Integer := Require_Open (File);
    begin
      Write_At (Table_Slot, Item, To);
    end;
  procedure Write (File : File_Type; Item : Element_Type) is
    Table_Slot : Integer := Require_Open (File);
    begin
      Write_At (Table_Slot, Item, File_Control_Blocks (Table_Slot).Index);
    end;
  procedure Set_Index (File : File_Type; To : Positive_Count) is
    Table_Slot : Integer := Require_Open (File);
    begin
      File_Control_Blocks (Table_Slot).Index := To;
    end;
  function Index (File : File_Type) return Positive_Count is
    Table_Slot : Integer := Require_Open (File);
    begin
      return File_Control_Blocks (Table_Slot).Index;
    end;
  function Size (File : File_Type) return Count is
    Table_Slot : Integer := Require_Open (File);
    begin
      return Element_Count (Table_Slot);
    end;
  function End_Of_File (File : File_Type) return Boolean is
    Table_Slot : Integer := Require_Open (File);
    begin
      if File_Control_Blocks (Table_Slot).Mode = Out_File then raise Mode_Error; end if;
      return Count (File_Control_Blocks (Table_Slot).Index) > Element_Count (Table_Slot);
    end;
end;
generic
  type Element_Type is private;
package Sequential_IO is
  type File_Type is limited private;
  type File_Mode is (In_File, Out_File);
  Status_Error : exception;
  Mode_Error   : exception;
  Name_Error   : exception;
  Use_Error    : exception;
  Device_Error : exception;
  End_Error    : exception;
  Data_Error   : exception;
  procedure Create (File : in out File_Type; Mode : File_Mode := Out_File; Name : String := ""; Form : String := "");
  procedure Open (File : in out File_Type; Mode : File_Mode; Name : String; Form : String := "");
  procedure Close (File : in out File_Type);
  procedure Delete (File : in out File_Type);
  procedure Reset (File : in out File_Type; Mode : File_Mode);
  procedure Reset (File : in out File_Type);
  function Mode (File : File_Type) return File_Mode;
  function Name (File : File_Type) return String;
  function Form (File : File_Type) return String;
  function Is_Open (File : File_Type) return Boolean;
  procedure Read (File : File_Type; Item : out Element_Type);
  procedure Write (File : File_Type; Item : Element_Type);
  function End_Of_File (File : File_Type) return Boolean;
private
  type File_Type is record
      Handle : Integer := 0;
    end record;
end;
with System;
package body Sequential_IO is
  function C_Fopen (Name : System.Address; Mode : System.Address) return System.Address;
  pragma Import (C, C_Fopen, "fopen");
  function C_Fclose (Stream : System.Address) return Integer;
  pragma Import (C, C_Fclose, "fclose");
  function C_Fread (Pointer : System.Address; Size : Integer; Count : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Fread, "fread");
  function C_Fwrite (Pointer : System.Address; Size : Integer; Count : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Fwrite, "fwrite");
  function C_Remove (Name : System.Address) return Integer;
  pragma Import (C, C_Remove, "remove");
  function C_Fseek (Stream : System.Address; Offset : Integer; Whence : Integer) return Integer;
  pragma Import (C, C_Fseek, "fseek");
  function C_Fflush (Stream : System.Address) return Integer;
  pragma Import (C, C_Fflush, "fflush");
  function C_Fgetc (Stream : System.Address) return Integer;
  pragma Import (C, C_Fgetc, "fgetc");
  function C_Ungetc (C : Integer; Stream : System.Address) return Integer;
  pragma Import (C, C_Ungetc, "ungetc");
  function C_Tmpfile return System.Address;
  pragma Import (C, C_Tmpfile, "tmpfile");
  Null_Address : constant System.Address := System.Null_Address;
  Seek_Set     : constant Integer        := 0;
  type Control_Block is record
      Stream      : System.Address := Null_Address;
      Mode        : File_Mode      := In_File;
      Is_Open     : Boolean        := False;
      Name_Length : Natural        := 0;
      Name        : String (1..1024);
    end record;
  File_Control_Blocks : array (1..99) of Control_Block;
  function Is_Open_Index (Table_Slot : Integer) return Boolean is
    begin
      return Table_Slot >= 1 and then Table_Slot <= 99 and then File_Control_Blocks (Table_Slot).Is_Open;
    end;
  function Require_Open (File : File_Type) return Integer is
    Table_Slot : Integer := File.Handle;
    begin
      if not Is_Open_Index (Table_Slot) then raise Status_Error; end if;
      return Table_Slot;
    end;
  procedure To_C_String (S : String; Buffer : out String) is
    J : Integer := 1;
    begin
      for I in S'First..S'Last loop
        Buffer (J) := S (I);
        J := J + 1;
      end loop;
      Buffer (J) := Character'Val (0);
    end;
  procedure Attach (File : in out File_Type; Mode : File_Mode; Name : String; Creating : Boolean) is
    Table_Slot  : Integer;
    Name_Buffer : String (1..1026);
    Mode_Text   : String (1..4);
    begin
      if Is_Open_Index (File.Handle) then raise Status_Error; end if;
      Table_Slot := 0;
      for J in File_Control_Blocks'Range loop
        if not File_Control_Blocks (J).Is_Open then Table_Slot := J; exit; end if;
      end loop;
      if Table_Slot = 0 then raise Use_Error; end if;
      if not Creating and then Name'Length = 0 then raise Name_Error; end if;
      if Creating then
        Mode_Text := ('w', '+', 'b', Character'Val (0));
      elsif Mode = In_File then
        Mode_Text := ('r', 'b', Character'Val (0), Character'Val (0));
      else
        Mode_Text := ('r', '+', 'b', Character'Val (0));
      end if;
      if Name'Length > 0 then
        To_C_String (Name, Name_Buffer);
        File_Control_Blocks (Table_Slot).Stream := C_Fopen (Name_Buffer'Address, Mode_Text'Address);
        if File_Control_Blocks (Table_Slot).Stream = Null_Address then
          if Creating then raise Use_Error; else raise Name_Error; end if;
        end if;
        File_Control_Blocks (Table_Slot).Name_Length := Name'Length;
        File_Control_Blocks (Table_Slot).Name (1..Name'Length) := Name;
      else
        File_Control_Blocks (Table_Slot).Stream := C_Tmpfile;
        if File_Control_Blocks (Table_Slot).Stream = Null_Address then raise Use_Error; end if;
        File_Control_Blocks (Table_Slot).Name_Length := 0;
      end if;
      File_Control_Blocks (Table_Slot).Mode := Mode;
      File_Control_Blocks (Table_Slot).Is_Open := True;
      File := (Handle => Table_Slot);
    end;
  procedure Create (File : in out File_Type; Mode : File_Mode := Out_File; Name : String := ""; Form : String := "") is
    begin
      Attach (File, Mode, Name, Creating => True);
    end;
  procedure Open (File : in out File_Type; Mode : File_Mode; Name : String; Form : String := "") is
    begin
      Attach (File, Mode, Name, Creating => False);
    end;
  procedure Close (File : in out File_Type) is
    Table_Slot : Integer := Require_Open (File);
    Ignore     : Integer;
    begin
      Ignore := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
      File_Control_Blocks (Table_Slot).Stream := Null_Address;
      File_Control_Blocks (Table_Slot).Is_Open := False;
      File := (Handle => 0);
    end;
  procedure Delete (File : in out File_Type) is
    Table_Slot  : Integer := Require_Open (File);
    Name_Buffer : String (1..1026);
    Ignore      : Integer;
    begin
      Ignore := C_Fclose (File_Control_Blocks (Table_Slot).Stream);
      if File_Control_Blocks (Table_Slot).Name_Length > 0 then
        To_C_String (File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length), Name_Buffer);
        Ignore := C_Remove (Name_Buffer'Address);
      end if;
      File_Control_Blocks (Table_Slot).Stream := Null_Address;
      File_Control_Blocks (Table_Slot).Is_Open := False;
      File := (Handle => 0);
    end;
  procedure Reset (File : in out File_Type; Mode : File_Mode) is
    Table_Slot : Integer := Require_Open (File);
    Ignore     : Integer;
    begin
      Ignore := C_Fflush (File_Control_Blocks (Table_Slot).Stream);
      Ignore := C_Fseek (File_Control_Blocks (Table_Slot).Stream, 0, Seek_Set);
      File_Control_Blocks (Table_Slot).Mode := Mode;
    end;
  procedure Reset (File : in out File_Type) is
    begin
      Reset (File, Mode (File));
    end;
  function Mode (File : File_Type) return File_Mode is
    Table_Slot : Integer := Require_Open (File);
    begin
      return File_Control_Blocks (Table_Slot).Mode;
    end;
  function Name (File : File_Type) return String is
    Table_Slot : Integer := Require_Open (File);
    begin
      if File_Control_Blocks (Table_Slot).Name_Length = 0 then raise Use_Error; end if;
      return File_Control_Blocks (Table_Slot).Name (1..File_Control_Blocks (Table_Slot).Name_Length);
    end;
  function Form (File : File_Type) return String is
    Table_Slot : Integer := Require_Open (File);
    begin
      return "";
    end;
  function Is_Open (File : File_Type) return Boolean is
    begin
      return Is_Open_Index (File.Handle);
    end;
  procedure Read (File : File_Type; Item : out Element_Type) is
    Table_Slot   :          Integer := Require_Open (File);
    Stored_Bytes :          Integer;
    Item_Bytes   : constant Integer := (Item'Size + 7) / 8;
    begin
      if File_Control_Blocks (Table_Slot).Mode = Out_File then raise Mode_Error; end if;
      if C_Fread (Stored_Bytes'Address, 4, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise End_Error;
      end if;
      if Stored_Bytes /= Item_Bytes then
        raise Data_Error;
      end if;
      if C_Fread (Item'Address, Stored_Bytes, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise End_Error;
      end if;
    end;
  procedure Write (File : File_Type; Item : Element_Type) is
    Table_Slot : Integer := Require_Open (File);
    Item_Bytes : Integer := (Item'Size + 7) / 8;
    begin
      if File_Control_Blocks (Table_Slot).Mode = In_File then raise Mode_Error; end if;
      if C_Fwrite (Item_Bytes'Address, 4, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise Device_Error;
      end if;
      if C_Fwrite (Item'Address, Item_Bytes, 1, File_Control_Blocks (Table_Slot).Stream) /= 1 then
        raise Device_Error;
      end if;
    end;
  function End_Of_File (File : File_Type) return Boolean is
    Table_Slot : Integer := Require_Open (File);
    C          : Integer;
    Ignore     : Integer;
    begin
      if File_Control_Blocks (Table_Slot).Mode = Out_File then raise Mode_Error; end if;
      C := C_Fgetc (File_Control_Blocks (Table_Slot).Stream);
      if C = -1 then
        return True;
      else
        Ignore := C_Ungetc (C, File_Control_Blocks (Table_Slot).Stream);
        return False;
      end if;
    end;
end;
generic
  type Source is limited private;
  type Target is limited private;
function Unchecked_Conversion (S : Source) return Target;
pragma Convention (Intrinsic, Unchecked_Conversion);
pragma Pure (Unchecked_Conversion);
generic
  type Object is limited private;
  type Name is access Object;
procedure Unchecked_Deallocation (X : in out Name);
pragma Convention (Intrinsic, Unchecked_Deallocation);
pragma Pure (Unchecked_Deallocation);
package Machine_Code is
  type Asm_Input_Operand is private;
  type Asm_Output_Operand is private;
  No_Input_Operands  : constant Asm_Input_Operand;
  No_Output_Operands : constant Asm_Output_Operand;
  type Asm_Input_Operand_List is
  array (Integer range <>) of Asm_Input_Operand;
  type Asm_Output_Operand_List is
  array (Integer range <>) of Asm_Output_Operand;
  type Asm_Insn is private;
  procedure Asm (Template : String;
                 Outputs  : Asm_Output_Operand_List;
                 Inputs   : Asm_Input_Operand_List;
                 Clobber  : String  := "";
                 Volatile : Boolean := False);
  procedure Asm (Template : String;
                 Outputs  : Asm_Output_Operand := No_Output_Operands;
                 Inputs   : Asm_Input_Operand_List;
                 Clobber  : String             := "";
                 Volatile : Boolean            := False);
  procedure Asm (Template : String;
                 Outputs  : Asm_Output_Operand_List;
                 Inputs   : Asm_Input_Operand := No_Input_Operands;
                 Clobber  : String            := "";
                 Volatile : Boolean           := False);
  procedure Asm (Template : String;
                 Outputs  : Asm_Output_Operand := No_Output_Operands;
                 Inputs   : Asm_Input_Operand  := No_Input_Operands;
                 Clobber  : String             := "";
                 Volatile : Boolean            := False);
  function Asm (Template : String;
                Outputs : Asm_Output_Operand_List;
                Inputs  : Asm_Input_Operand_List;
                Clobber : String := "";
                Volatile : Boolean := False) return Asm_Insn;
  function Asm (Template : String;
                Outputs : Asm_Output_Operand := No_Output_Operands;
                Inputs  : Asm_Input_Operand_List;
                Clobber : String             := "";
                Volatile : Boolean            := False) return Asm_Insn;
  function Asm (Template : String;
                Outputs : Asm_Output_Operand_List;
                Inputs  : Asm_Input_Operand := No_Input_Operands;
                Clobber : String            := "";
                Volatile : Boolean           := False) return Asm_Insn;
  function Asm (Template : String;
                Outputs : Asm_Output_Operand := No_Output_Operands;
                Inputs  : Asm_Input_Operand  := No_Input_Operands;
                Clobber : String             := "";
                Volatile : Boolean            := False) return Asm_Insn;
  pragma Import (Intrinsic, Asm);
private
  type Asm_Input_Operand is new Integer;
  type Asm_Output_Operand is new Integer;
  type Asm_Insn is new Integer;
  No_Input_Operands  : constant Asm_Input_Operand  := 0;
  No_Output_Operands : constant Asm_Output_Operand := 0;
end;
