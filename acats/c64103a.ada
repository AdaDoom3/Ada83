-- C64103A.ADA

-- OBJECTIVE:
--     CHECK THAT THE APPROPRIATE EXCEPTION IS RAISED AS REQUIRED FOR
--     TYPE CONVERSIONS ON IN OUT SCALAR VARIABLES.  IN PARTICULAR:
--          (A) NUMERIC_ERROR/CONSTRAINT_ERROR IS RAISED FOR NUMERIC
--              TYPES BEFORE THE CALL WHEN THE ACTUAL VALUE IS
--              OUTSIDE THE RANGE OF THE FORMAL PARAMETER'S BASE TYPE.
--          (B) NUMERIC_ERROR/CONSTRAINT_ERROR IS RAISED FOR NUMERIC
--              TYPES AFTER THE CALL WHEN THE FORMAL PARAMETER'S
--              VALUE IS OUTSIDE THE RANGE OF THE ACTUAL VARIABLE'S
--              BASE TYPE.

-- HISTORY:
--     CPP 07/02/84
--     JBG 04/19/85
--     EG  10/29/85  FIX NUMERIC_ERROR/CONSTRAINT_ERROR ACCORDING TO
--                   AI-00387.
--     JRK 06/24/86  FIXED COMMENTS ABOUT NUMERIC_ERROR/
--                   CONSTRAINT_ERROR.
--     RJW 02/12/88  ADDED CHECK FOR EQUALITY OF EXPONENTS IN
--                   PROCEDURE 'P2' OF SECOND SUB-BLOCK OF PART (A).
--                   REVISED HEADER.

WITH SYSTEM;
WITH REPORT;  USE REPORT;
PROCEDURE C64103A IS

BEGIN
     TEST ("C64103A", "CHECK THAT NUMERIC_ERROR/CONSTRAINT_ERROR IS " &
                      "RAISED FOR CERTAIN TYPE CONVERSIONS ON SCALAR " &
                      "PARAMETERS");

     ----------------------------------------------------

     DECLARE   -- (A)
     BEGIN     -- (A)

          DECLARE
               TYPE SM_INT IS RANGE 1..10;
               TYPE LG_INT IS RANGE SYSTEM.MIN_INT..SYSTEM.MAX_INT;
               LARGE : LG_INT := SYSTEM.MAX_INT;

               PROCEDURE P1 (X : IN OUT SM_INT) IS
               BEGIN
                    FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P1 (A)");
               END P1;
          BEGIN
               COMMENT ("CHECK INPUT OF INTEGER TYPES (A)");
               IF LG_INT (SM_INT'BASE'LAST) < LARGE THEN
                    P1 (SM_INT (LARGE));
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE INTEGER BASE " &
                             "TYPE - P1 (A)");
               END IF;
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P1 (A)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P1 (A)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (A)");
          END;

          DECLARE
               TYPE SM_FLOAT IS DIGITS 1;
               TYPE LG_FLOAT IS DIGITS SYSTEM.MAX_DIGITS;
               LARGE      : LG_FLOAT := LG_FLOAT'SAFE_LARGE;
               APPLICABLE : BOOLEAN;

               PROCEDURE P2 (X : IN OUT SM_FLOAT) IS
               BEGIN
                    IF SM_FLOAT'SAFE_EMAX = LG_FLOAT'SAFE_EMAX THEN
                         NULL;
                    ELSE
                         FAILED ("EXCEPTION NOT RAISED BEFORE CALL " &
                                 "-P2 (A)");
                    END IF;
               END P2;
          BEGIN
               COMMENT ("CHECK INPUT OF FLOATING POINT TYPES (A)");

               BEGIN
                    APPLICABLE := LG_FLOAT (SM_FLOAT'BASE'LAST) < LARGE;
               EXCEPTION
                    WHEN OTHERS =>
                         APPLICABLE := FALSE;
               END;

               IF APPLICABLE THEN
                    P2 (SM_FLOAT (LARGE));
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE FLOATING " &
                             "POINT BASE TYPE - P2 (A)");
               END IF;
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P2 (A)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P2 (A)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P2 (A)");
          END;

          DECLARE
               TYPE FINE_FIXED IS DELTA SYSTEM.FINE_DELTA
                    RANGE -1.0 .. 1.0;
               LG_C : CONSTANT := 2 * FINE_FIXED'BASE'LARGE;
               TYPE COARSE_FIXED IS DELTA 2 * SYSTEM.FINE_DELTA
                    RANGE -LG_C .. LG_C;
               LARGE : COARSE_FIXED := -LG_C;

               PROCEDURE P3 (X : IN OUT FINE_FIXED) IS
               BEGIN
                    FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P3 (A)");
               END P3;
          BEGIN
               COMMENT ("CHECK INPUT OF FIXED POINT TYPES (A)");
               P3 (FINE_FIXED (LARGE));
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P3 (A)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P3 (A)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P3 (A)");
          END;

     END; -- (A)

     ----------------------------------------------------

     DECLARE   -- (B)
     BEGIN     -- (B)

          DECLARE
               TYPE SM_INT IS RANGE 1..10;
               TYPE LG_INT IS RANGE SYSTEM.MIN_INT..SYSTEM.MAX_INT;
               SMALL : SM_INT := 2;
               LARGE : LG_INT := SYSTEM.MAX_INT;

               PROCEDURE P1 (X : IN OUT LG_INT) IS
               BEGIN
                    IF EQUAL(2, 2) THEN
                         X := LARGE;
                    ELSE
                         X := -LARGE;
                    END IF;
               END P1;
          BEGIN
               COMMENT ("CHECK OUTPUT OF INTEGER TYPES (B)");
               IF LG_INT (SM_INT'BASE'LAST) < LARGE THEN
                    P1 (LG_INT (SMALL));
                    IF SMALL > 0 THEN
                         FAILED ("EXCEPTION NOT RAISED AFTER CALL"&
                                 " -P1 (B)");
                    ELSE
                         FAILED ("NO EXCEPTION - P1(B)");
                    END IF;
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE INTEGER BASE " &
                             "TYPE - P1 (B)");
               END IF;
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P1 (B)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P1 (B)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (B)");
          END;

          DECLARE
               TYPE SM_FLOAT IS DIGITS 1;
               TYPE LG_FLOAT IS DIGITS SYSTEM.MAX_DIGITS;
               SMALL      : SM_FLOAT := 0.1;
               LARGE      : LG_FLOAT := LG_FLOAT'SAFE_LARGE;
               APPLICABLE : BOOLEAN;

               PROCEDURE P2 (X : IN OUT LG_FLOAT) IS
               BEGIN
                    IF EQUAL(2, 2) THEN
                         X := LARGE;
                    ELSE
                         X := -LARGE;
                    END IF;
               END P2;
          BEGIN
               COMMENT ("CHECK OUTPUT OF FLOATING POINT TYPES (B)");
               BEGIN
                    APPLICABLE := LG_FLOAT(SM_FLOAT'BASE'LAST) < LARGE;
               EXCEPTION
                    WHEN OTHERS =>
                         APPLICABLE := FALSE;
               END;

               IF APPLICABLE THEN
                    P2 (LG_FLOAT (SMALL));
                    IF SMALL > 0.0 THEN
                         FAILED ("EXCEPTION NOT RAISED AFTER CALL"&
                                 " -P2 (B)");
                    ELSE
                         FAILED ("NO EXCEPTION - P2 (B)");
                    END IF;
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE FLOATING " &
                             "POINT BASE TYPE - P2 (B)");
               END IF;
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P2 (B)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P2 (B)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - P2 (B)");
          END;

          DECLARE
               TYPE FINE_FIXED IS DELTA SYSTEM.FINE_DELTA
                    RANGE -1.0 .. 1.0;
               LG_C : CONSTANT := 2 * FINE_FIXED'BASE'LARGE;
               TYPE COARSE_FIXED IS DELTA 2 * SYSTEM.FINE_DELTA
                    RANGE -LG_C .. LG_C;
               SMALL : FINE_FIXED := 0.0;
               LARGE : COARSE_FIXED := -LG_C;

               PROCEDURE P3 (X : IN OUT COARSE_FIXED) IS
               BEGIN
                    IF EQUAL(2, 2) THEN
                         X := LARGE;
                    ELSE
                         X := -LARGE;
                    END IF;
               END P3;
          BEGIN
               COMMENT ("CHECK OUTPUT OF FIXED POINT TYPES (B)");
               P3 (COARSE_FIXED (SMALL));
               IF SMALL > 0.0 THEN
                    FAILED ("NO EXCEPTION - P3 (B)");
               ELSE
                    FAILED ("STILL NO EXCEPTION - P3 (B)");
               END IF;
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P3 (B)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P3 (B)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - P3 (B)");
          END;

     END; -- (B)

     ----------------------------------------------------

     RESULT;

END C64103A;
