-- SPPRT13.ADA
--
-- Generated from spprt13sp.tst (ACVC 1.12) by hand-substituting the macro
-- placeholders with calls to FCNDECL functions. Each constant is the
-- address of a real backing variable in FCNDECL's body, so tests that
-- place objects at these addresses (FOR x USE AT y) overlay storage that
-- actually exists and may read and write through the overlaid objects.

WITH FCNDECL; USE FCNDECL;
WITH SYSTEM;
PACKAGE SPPRT13 IS

	VARIABLE_ADDRESS  : CONSTANT SYSTEM.ADDRESS := FCNDECL.VAR_ADDRESS;
	VARIABLE_ADDRESS1 : CONSTANT SYSTEM.ADDRESS := FCNDECL.VAR_ADDRESS1;
	VARIABLE_ADDRESS2 : CONSTANT SYSTEM.ADDRESS := FCNDECL.VAR_ADDRESS2;

	ENTRY_ADDRESS  : CONSTANT SYSTEM.ADDRESS := FCNDECL.ENT_ADDRESS;
	ENTRY_ADDRESS1 : CONSTANT SYSTEM.ADDRESS := FCNDECL.ENT_ADDRESS1;
	ENTRY_ADDRESS2 : CONSTANT SYSTEM.ADDRESS := FCNDECL.ENT_ADDRESS2;

END SPPRT13;
