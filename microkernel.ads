------------------------------------------------------------------------------
-- Ada2012 ARM Cortex-A15 SMP Microkernel - Specification
------------------------------------------------------------------------------
pragma Restrictions (No_Elaboration_Code);
pragma Profile (Ravenscar);

package Microkernel with
   SPARK_Mode => On,
   Pure
is
   pragma Elaborate_Body;

   -- Main kernel entry point
   procedure Main_Loop
      with No_Return;

private
   -- Implementation in microkernel.adb

end Microkernel;
