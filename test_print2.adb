procedure TEST_PRINT2 is
   procedure PUT(X : INTEGER);
   pragma IMPORT(C, PUT, "print_int");

   Y : INTEGER;
begin
   Y := 99;
   PUT(Y);
end TEST_PRINT2;
