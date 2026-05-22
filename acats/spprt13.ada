-- SPPRT13.ADA
--
-- Generated from spprt13sp.tst (ACVC 1.12) by hand-substituting the macro
-- placeholders with legal SYSTEM.ADDRESS constants. SYSTEM.ADDRESS in our
-- runtime is `TYPE ADDRESS IS NEW INTEGER`, so universal-integer literals
-- are accepted directly. The values themselves are arbitrary distinct
-- non-zero integers; tests use these only as targets for representation
-- clauses (FOR x USE AT y), they are never dereferenced.

WITH FCNDECL; USE FCNDECL;
WITH SYSTEM;
PACKAGE SPPRT13 IS

	VARIABLE_ADDRESS  : CONSTANT SYSTEM.ADDRESS := 16#1000#;
	VARIABLE_ADDRESS1 : CONSTANT SYSTEM.ADDRESS := 16#2000#;
	VARIABLE_ADDRESS2 : CONSTANT SYSTEM.ADDRESS := 16#3000#;

	ENTRY_ADDRESS  : CONSTANT SYSTEM.ADDRESS := 16#4000#;
	ENTRY_ADDRESS1 : CONSTANT SYSTEM.ADDRESS := 16#5000#;
	ENTRY_ADDRESS2 : CONSTANT SYSTEM.ADDRESS := 16#6000#;

END SPPRT13;
