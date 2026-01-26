-- Ultra-minimal TEXT_IO Package Body
with SYSTEM;

package body TEXT_IO is

   Dummy_Is_Open : Boolean := False;
   Dummy_Mode : FILE_MODE := IN_FILE;

   procedure CREATE(FILE : in Out FILE_TYPE;
                   MODE : in FILE_MODE := OUT_FILE;
                   NAME : in STRING := "";
                   FORM : in STRING := "") is
   begin
      Dummy_Is_Open := True;
      Dummy_Mode := MODE;
      FILE := 1;
   end CREATE;

   procedure OPEN(FILE : in Out FILE_TYPE;
                 MODE : in FILE_MODE;
                 NAME : in STRING;
                 FORM : in STRING := "") is
   begin
      Dummy_Is_Open := True;
      Dummy_Mode := MODE;
      FILE := 1;
   end OPEN;

   procedure CLOSE(FILE : in Out FILE_TYPE) is
   begin
      Dummy_Is_Open := False;
   end CLOSE;

   procedure DELETE(FILE : in Out FILE_TYPE) is
   begin
      Dummy_Is_Open := False;
   end DELETE;

   procedure RESET(FILE : in Out FILE_TYPE; MODE : in FILE_MODE) is
   begin
      Dummy_Mode := MODE;
   end RESET;

   procedure RESET(FILE : in Out FILE_TYPE) is
   begin
      null;
   end RESET;

   function MODE(FILE : in FILE_TYPE) return FILE_MODE is
   begin
      return Dummy_Mode;
   end MODE;

   function NAME(FILE : in FILE_TYPE) return STRING is
   begin
      return "";
   end NAME;

   function FORM(FILE : in FILE_TYPE) return STRING is
   begin
      return "";
   end FORM;

   function IS_OPEN(FILE : in FILE_TYPE) return BOOLEAN is
   begin
      return Dummy_Is_Open;
   end IS_OPEN;

   function END_OF_FILE(FILE : in FILE_TYPE) return BOOLEAN is
   begin
      return True;
   end END_OF_FILE;

   function END_OF_FILE return BOOLEAN is
   begin
      return True;
   end END_OF_FILE;

   procedure PUT(ITEM : in CHARACTER) is
   begin
      null;
   end PUT;

   procedure PUT(ITEM : in STRING) is
   begin
      null;
   end PUT;

   procedure NEW_LINE(SPACING : in POSITIVE_COUNT := 1) is
   begin
      null;
   end NEW_LINE;

end TEXT_IO;
