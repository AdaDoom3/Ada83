-- C95084A.ADA

-- CHECK THAT THE APPROPRIATE EXCEPTION IS RAISED AS REQUIRED FOR TYPE
-- CONVERSIONS ON IN OUT SCALAR VARIABLES.  IN PARTICULAR:
--   (A) CONSTRAINT_ERROR (OR NUMERIC_ERROR) IS RAISED FOR NUMERIC TYPES
--       BEFORE THE CALL WHEN THE ACTUAL VALUE IS OUTSIDE THE RANGE OF
--       THE FORMAL PARAMETER'S BASE TYPE.
--   (B) CONSTRAINT_ERROR (OR NUMERIC_ERROR) IS RAISED FOR NUMERIC TYPES
--       AFTER THE CALL WHEN THE FORMAL PARAMETER'S VALUE IS OUTSIDE THE
--       RANGE OF THE ACTUAL VARIABLE'S BASE TYPE.

-- GLH 7/30/85
-- JRK 9/13/85

WITH SYSTEM;
WITH REPORT; USE REPORT;
PROCEDURE C95084A IS

BEGIN
     TEST ("C95084A", "CHECK THAT CONSTRAINT_ERROR (OR " &
                      "NUMERIC_ERROR) IS RAISED FOR CERTAIN TYPE " &
                      "CONVERSIONS ON SCALAR PARAMETERS");

     ----------------------------------------------------

     BEGIN     -- (A)

          DECLARE
               TYPE SM_INT IS RANGE 1..10;
               TYPE LG_INT IS RANGE SYSTEM.MIN_INT .. SYSTEM.MAX_INT;
               LARGE : LG_INT := SYSTEM.MAX_INT;

               TASK T1 IS
                    ENTRY E1 (X : IN OUT SM_INT);
               END T1;

               TASK BODY T1 IS
               BEGIN
                    SELECT
                         ACCEPT E1 (X : IN OUT SM_INT) DO
                              FAILED ("EXCEPTION NOT RAISED " &
                                      "BEFORE CALL - T1 (A)");
                         END E1;
                    OR
                         TERMINATE;
                    END SELECT;
               END T1;

          BEGIN
               COMMENT ("CHECK INPUT OF INTEGER TYPES (A)");

               IF LG_INT (SM_INT'BASE'LAST) < LARGE THEN
                    T1.E1 (SM_INT (LARGE));
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE INTEGER BASE " &
                             "TYPE - T1 (A)");
               END IF;

          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - T1 (A)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - T1 (A)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - T1 (A)");
          END;

          DECLARE
               TYPE SM_FLOAT IS DIGITS 1;
               TYPE LG_FLOAT IS DIGITS SYSTEM.MAX_DIGITS;
               LARGE      : LG_FLOAT := LG_FLOAT'SAFE_LARGE;
               APPLICABLE : BOOLEAN;

               TASK T2 IS
                    ENTRY E2 (X : IN OUT SM_FLOAT);
               END T2;

               TASK BODY T2 IS
               BEGIN
                    SELECT
                         ACCEPT E2 (X : IN OUT SM_FLOAT) DO
                              FAILED ("EXCEPTION NOT RAISED " &
                                      "BEFORE CALL - T2 (A)");
                         END E2;
                    OR
                         TERMINATE;
                    END SELECT;
               END T2;

          BEGIN
               COMMENT ("CHECK INPUT OF FLOATING POINT TYPES (A)");

               BEGIN
                    APPLICABLE := LG_FLOAT (SM_FLOAT'BASE'LAST) < LARGE;
               EXCEPTION
                    WHEN OTHERS =>
                         APPLICABLE := FALSE;
               END;

               IF APPLICABLE THEN
                    T2.E2 (SM_FLOAT (LARGE));
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE FLOATING " &
                             "POINT BASE TYPE - T2 (A)");
               END IF;

          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - T2 (A)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - T2 (A)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - T2 (A)");
          END;

          DECLARE
               TYPE FINE_FIXED IS DELTA SYSTEM.FINE_DELTA
                    RANGE -1.0 .. 1.0;
               LG_C : CONSTANT := 2 * FINE_FIXED'BASE'LARGE;
               TYPE COARSE_FIXED IS DELTA 2 * SYSTEM.FINE_DELTA
                    RANGE -LG_C .. LG_C;
               LARGE : COARSE_FIXED := -LG_C;


               TASK T3 IS
                    ENTRY E3 (X : IN OUT FINE_FIXED);
               END T3;

               TASK BODY T3 IS
               BEGIN
                    SELECT
                         ACCEPT E3 (X : IN OUT FINE_FIXED) DO
                              FAILED ("EXCEPTION NOT RAISED " &
                                      "BEFORE CALL - T3 (A)");
                         END E3;
                    OR
                         TERMINATE;
                    END SELECT;
               END T3;

          BEGIN
               COMMENT ("CHECK INPUT OF FIXED POINT TYPES (A)");

               T3.E3 (FINE_FIXED (LARGE));

          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - T3 (A)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - T3 (A)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - T3 (A)");
          END;

     END;    -- (A)

     ----------------------------------------------------

     BEGIN     -- (B)

          DECLARE
               TYPE SM_INT IS RANGE 1..10;
               TYPE LG_INT IS RANGE SYSTEM.MIN_INT .. SYSTEM.MAX_INT;
               SMALL : SM_INT := 2;
               LARGE : LG_INT := SYSTEM.MAX_INT;


               TASK T4 IS
                    ENTRY E4 (X : IN OUT LG_INT);
               END T4;

               TASK BODY T4 IS
               BEGIN
                    SELECT
                         ACCEPT E4 (X : IN OUT LG_INT) DO
                              IF EQUAL (2, 2) THEN
                                   X := LARGE;
                              ELSE
                                   X := -LARGE;
                              END IF;
                         END E4;
                    OR
                         TERMINATE;
                    END SELECT;
               END T4;

          BEGIN
               COMMENT ("CHECK OUTPUT OF INTEGER TYPES (B)");

               IF LG_INT (SM_INT'BASE'LAST) < LARGE THEN
                    T4.E4 (LG_INT (SMALL));
                    IF SMALL > 0 THEN        -- TO PREVENT DEAD VARIABLE
                                             -- OPTIMIZATION.
                         FAILED ("EXCEPTION NOT RAISED AFTER CALL " &
                                 "- T4 (B)");
                    ELSE
                         FAILED ("NO EXCEPTION - T4 (B)");
                    END IF;
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE INTEGER BASE " &
                             "TYPE - T4 (B)");
               END IF;

          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - T4 (B)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - T4 (B)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - T4 (B)");
          END;

          DECLARE
               TYPE SM_FLOAT IS DIGITS 1;
               TYPE LG_FLOAT IS DIGITS SYSTEM.MAX_DIGITS;
               SMALL      : SM_FLOAT := 0.1;
               LARGE      : LG_FLOAT := LG_FLOAT'SAFE_LARGE;
               APPLICABLE : BOOLEAN;

               TASK T5 IS
                    ENTRY E5  (X : IN OUT LG_FLOAT);
               END T5;

               TASK BODY T5 IS
               BEGIN
                    SELECT
                         ACCEPT E5 (X : IN OUT LG_FLOAT) DO
                              IF EQUAL (2, 2) THEN
                                   X := LARGE;
                              ELSE
                                   X := -LARGE;
                              END IF;
                         END E5;
                    OR
                         TERMINATE;
                    END SELECT;
               END T5;

          BEGIN
               COMMENT ("CHECK OUTPUT OF FLOATING POINT TYPES (B)");

               BEGIN
                    APPLICABLE := LG_FLOAT (SM_FLOAT'BASE'LAST) < LARGE;
               EXCEPTION
                    WHEN OTHERS =>
                         APPLICABLE := FALSE;
               END;

               IF APPLICABLE THEN
                    T5.E5 (LG_FLOAT (SMALL));
                    IF SMALL > 0.0 THEN      -- TO PREVENT DEAD VARIABLE
                                             -- OPTIMIZATION.
                         FAILED ("EXCEPTION NOT RAISED AFTER CALL " &
                                 "- T5 (B)");
                    ELSE
                         FAILED ("NO EXCEPTION - T5 (B)");
                    END IF;
               ELSE
                    COMMENT ("NOT APPLICABLE: ONLY ONE FLOATING " &
                             "POINT BASE TYPE - T5 (B)");
               END IF;

          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - T5 (B)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - T5 (B)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - T5 (B)");
          END;

          DECLARE
               TYPE FINE_FIXED IS DELTA SYSTEM.FINE_DELTA
                    RANGE -1.0 .. 1.0;
               LG_C : CONSTANT := 2 * FINE_FIXED'BASE'LARGE;
               TYPE COARSE_FIXED IS DELTA 2 * SYSTEM.FINE_DELTA
                    RANGE -LG_C .. LG_C;
               SMALL : FINE_FIXED := 0.0;
               LARGE : COARSE_FIXED := -LG_C;

               TASK T6 IS
                    ENTRY E6 (X : IN OUT COARSE_FIXED);
               END T6;

               TASK BODY T6 IS
               BEGIN
                    SELECT
                         ACCEPT E6 (X : IN OUT COARSE_FIXED) DO
                              IF EQUAL (2, 2) THEN
                                   X := LARGE;
                              ELSE
                                   X := -LARGE;
                              END IF;
                         END E6;
                    OR
                         TERMINATE;
                    END SELECT;
               END T6;

          BEGIN
               COMMENT ("CHECK OUTPUT OF FIXED POINT TYPES (B)");

               T6.E6 (COARSE_FIXED (SMALL));
               IF SMALL > 0.0 THEN           -- TO PREVENT DEAD VARIABLE
                                             -- OPTIMIZATION.
                    FAILED ("EXCEPTION NOT RAISED AFTER CALL - T6 (B)");
               ELSE
                    FAILED ("NO EXCEPTION - T6 (B)");
               END IF;

          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - T6 (B)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - T6 (B)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - T6 (B)");
          END;

     END;   --(B)

     ----------------------------------------------------

     RESULT;
END C95084A;
