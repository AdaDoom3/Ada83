procedure TEST_SLICE is
   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   procedure PROCESS_SLICE(A : INT_ARRAY) is
      SUM : INTEGER;
   begin
      SUM := 0;
      for I in A'FIRST .. A'LAST loop
         SUM := SUM + A(I);
      end loop;
   end PROCESS_SLICE;

   ARR : INT_ARRAY(1 .. 10);
begin
   ARR(1) := 1;
   ARR(2) := 2;
   ARR(3) := 3;
   ARR(4) := 4;
   ARR(5) := 5;
   ARR(6) := 6;
   ARR(7) := 7;
   ARR(8) := 8;
   ARR(9) := 9;
   ARR(10) := 10;

   PROCESS_SLICE(ARR(3 .. 7));
   PROCESS_SLICE(ARR(1 .. 5));
   PROCESS_SLICE(ARR(6 .. 10));
end TEST_SLICE;
