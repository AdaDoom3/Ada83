procedure TEST_UNCONSTRAINED is
   type INT_ARRAY is array (INTEGER range <>) of INTEGER;

   procedure PROCESS(A : INT_ARRAY) is
      SUM : INTEGER;
   begin
      SUM := 0;
      for I in A'FIRST .. A'LAST loop
         SUM := SUM + A(I);
      end loop;
   end PROCESS;

   ARR1 : INT_ARRAY(1 .. 5);
   ARR2 : INT_ARRAY(10 .. 12);
begin
   ARR1(1) := 1;
   ARR1(2) := 2;
   ARR1(3) := 3;
   ARR1(4) := 4;
   ARR1(5) := 5;

   ARR2(10) := 10;
   ARR2(11) := 20;
   ARR2(12) := 30;

   PROCESS(ARR1);
   PROCESS(ARR2);
end TEST_UNCONSTRAINED;
