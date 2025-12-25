-- Test variant record discriminants
procedure Test_Variant_Record is
   
   type Shape_Kind is (Circle, Rectangle);
   
   type Shape(Kind : Shape_Kind) is record
      case Kind is
         when Circle =>
            Radius : Integer;
         when Rectangle =>
            Width  : Integer;
            Height : Integer;
      end case;
   end record;
   
   S1 : Shape(Circle);
   S2 : Shape(Rectangle);

begin
   S1.Radius := 10;
   S2.Width := 20;
   S2.Height := 30;
end Test_Variant_Record;
