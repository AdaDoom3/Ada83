-- B85015A.ADA

-- CHECK THAT A RENAMED ENTRY CANNOT BE USED:

--   A) IN A CONDITIONAL ENTRY CALL.

--   B) IN A TIMED ENTRY CALL.

--   C) AS THE PREFIX TO THE 'COUNT ATTRIBUTE.

-- EG  02/22/84

PROCEDURE B85015A IS

     TASK TYPE T IS
          ENTRY E;
     END T;

     TSK : T;

     FUNCTION FUN RETURN T;

     PROCEDURE PROC1 RENAMES FUN.E;
     PROCEDURE PROC2 RENAMES PROC1;

     TASK BODY T IS
          I   : INTEGER;
     BEGIN
          I := PROC1'COUNT;                             -- ERROR: C.
          I := PROC2'COUNT;                             -- ERROR: C.
     END T;

     FUNCTION FUN RETURN T IS
     BEGIN
          RETURN TSK;
     END FUN;

BEGIN

     SELECT
          PROC1;                                        -- ERROR: A.
     ELSE
          NULL;
     END SELECT;
     SELECT
          PROC2;                                        -- ERROR: A.
     ELSE
          NULL;
     END SELECT;

     SELECT
          PROC1;                                        -- ERROR: B.
     OR
          DELAY 3.0;  -- TODO: acats-delay-deviation: before was 30.0
     END SELECT;
     SELECT
          PROC2;                                        -- ERROR: B.
     OR
          DELAY 3.0;  -- TODO: acats-delay-deviation: before was 30.0
     END SELECT;

END B85015A;
