-- Ada 83 (ANSI/MIL-STD-1815A), compiled by ada83.

with Text_IO; use Text_IO;

procedure Spectrum is

   type Colour  is (Red, Amber, Green);
   type Degrees is delta 0.125 range -360.0 .. 360.0;
   type Mask    is array (Colour) of Boolean;

   subtype Small is Integer range 0 .. 2#1111_1111#;

   type Reading (Channel : Colour := Red) is
      record
         Angle : Degrees := 0.0;
         case Channel is
            when Red    => Intensity : Small;
            when others => Weight    : Degrees;
         end case;
      end record;

   Sensors : constant := 16#FF#;
   Lit     : Mask := (Red => True, Amber => False, Green => False);

   generic
      type Element is private;
   package Buffers is
      procedure Store (Item : in Element);
   end Buffers;

   package body Buffers is
      procedure Store (Item : in Element) is
      begin
         null;
      end Store;
   end Buffers;

   task Sampler is
      entry Sample (Value : in Degrees);
   end Sampler;

   task body Sampler is
   begin
      loop
         select
            accept Sample (Value : in Degrees) do
                                             Put_Line ("sampled " & Integer'Image (Integer (Value)));
            end Sample;
         or
            terminate;
         end select;
      end loop;
   end Sampler;

begin
   for Shade in Colour loop
      Put_Line (Colour'Image (Shade) & " =>" & Boolean'Image (Lit (Shade)));
   end loop;

   Sampler.Sample (180.0);

exception
   when Constraint_Error =>
      Put_Line ("out of range");
end Spectrum;
