------------------------------------------------------------------------------
-- Main Entry Point for Ada2012 Microkernel
------------------------------------------------------------------------------
pragma No_Return (Main);

with Microkernel;

procedure Main is
begin
   Microkernel.Main_Loop;
end Main;
