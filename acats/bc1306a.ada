-- BC1306A.ADA

-- CHECK THAT A GENERIC FORMAL SUBPROGRAM CANNOT BE USED IN A 
-- CONDITIONAL OR TIMED ENTRY CALL, EVEN IF NEVER INSTANTIATED.

-- DAT 8/27/81

PROCEDURE BC1306A IS

     TASK TYPE T IS
          ENTRY ENT;
     END T;

     TASK BODY T IS
     BEGIN
          SELECT
               ACCEPT ENT;
          OR
               TERMINATE;
          END SELECT;
     END T;

     PACKAGE Q IS
          TT : T;
     END Q;
     USE Q;
     PACKAGE BODY Q IS
     BEGIN NULL; END Q;

     GENERIC
          WITH PROCEDURE ENT;
     PACKAGE PK IS END PK;

     PACKAGE BODY PK IS
     BEGIN
          ENT;                          -- OK.
          SELECT
               ENT;                     -- ERROR: GENERIC PARM.
          ELSE 
               NULL;
          END SELECT;
          SELECT
               TT.ENT;                  -- OK.
          ELSE
               NULL;
          END SELECT;
          SELECT
               ENT;                     -- ERROR: GENERIC PARM.
          OR
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
          END SELECT;
     END PK;

BEGIN
     NULL;
END BC1306A;
