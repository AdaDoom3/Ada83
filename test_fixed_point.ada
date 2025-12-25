-- Test fixed-point types
procedure Test_Fixed_Point is
   
   -- Ordinary fixed-point type (delta)
   type Money is delta 0.01 range 0.0 .. 1000.0;
   
   Balance : Money := 100.50;
   Tax : Money := 5.25;
   Total : Money;

begin
   Total := Balance + Tax;
end Test_Fixed_Point;
