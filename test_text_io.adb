with TEXT_IO;
procedure TEST_TEXT_IO is
   X : INTEGER;
   Y : FLOAT;
begin
   X := 42;
   Y := 3.14;

   TEXT_IO.PUT(X);
   TEXT_IO.NEW_LINE;

   TEXT_IO.PUT_LINE(99);

   TEXT_IO.PUT(Y);
   TEXT_IO.NEW_LINE;
end TEST_TEXT_IO;
