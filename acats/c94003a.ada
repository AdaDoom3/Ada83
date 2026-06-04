-- C94003A.ADA

-- CHECK THAT A UNIT TERMINATES PROPERLY IF IT DECLARES AN ACCESS
--   TYPE DESIGNATING TASK OBJECTS BUT NEVER ACTUALLY CREATES A
--   TASK.
-- SUBTESTS ARE:
--   (A)  A SIMPLE ACCESS TO TASK TYPE, IN A BLOCK.
--   (B)  AN ACCESS TO RECORD OF TASK TYPE, IN A FUNCTION.
--   (C)  AN ACCESS TO RECORD OF ARRAY OF TASK TYPE, IN A TASK BODY.

-- THE PRIMARY FAILURE SYMPTOM OF THIS TEST OCCURS IF THE PROGRAM
--   WAITS FOREVER AT ONE OF THE INDICATED POINTS.

-- THIS TEST CONTAINS SHARED VARIABLES.

-- JRK 10/7/81
-- SPS 11/2/82
-- SPS 11/21/82
-- JRK 11/29/82

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94003A IS

     GLOBAL : INTEGER;

     FUNCTION SIDE_EFFECT RETURN INTEGER IS
     BEGIN
          GLOBAL := IDENT_INT (2);
          RETURN 0;
     END SIDE_EFFECT;

     TASK TYPE TT IS
          ENTRY E;
          PRAGMA PRIORITY (PRIORITY'LAST);
     END TT;

     TASK BODY TT IS
          I : INTEGER := SIDE_EFFECT;
     BEGIN
          NULL;
     END TT;

     PRAGMA PRIORITY (PRIORITY'FIRST);

BEGIN
     TEST ("C94003A", "CHECK THAT A UNIT TERMINATES PROPERLY IF " &
                      "IT DECLARES AN ACCESS TYPE DESIGNATING " &
                      "TASK OBJECTS BUT NEVER ACTUALLY CREATES " &
                      "A TASK");

     --------------------------------------------------

     GLOBAL := IDENT_INT (1);

     DECLARE -- (A)

          TYPE A_T IS ACCESS TT;
          A : A_T;

     BEGIN -- (A)

          DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0

     END; -- (A)    -- FAILS IF INFINITE WAIT.

     COMMENT ("BLOCK TERMINATED - (A)");

     IF GLOBAL /= 1 THEN
          FAILED ("UNALLOCATED TASK BODY EXECUTED - (A)");
     END IF;

     --------------------------------------------------

     GLOBAL := IDENT_INT (1);

     DECLARE -- (B)

          I : INTEGER;

          FUNCTION F RETURN INTEGER IS
               TYPE RT IS
                    RECORD
                         I : INTEGER;
                         T : TT;
                    END RECORD;
               TYPE ART IS ACCESS RT;
               AR : ART;
          BEGIN
               DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0
               RETURN 0;
          END F;    -- FAILS IF INFINITE WAIT.

     BEGIN -- (B)

          I := F;   -- FAILS IF F NEVER RETURNS.

          COMMENT ("FUNCTION TERMINATED - (B)");

          IF GLOBAL /= 1 THEN
               FAILED ("UNALLOCATED TASK BODY EXECUTED - (B)");
          END IF;

     END; -- (B)

     --------------------------------------------------

     GLOBAL := IDENT_INT (1);

     DECLARE -- (C)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 60 * 60 / 4;      -- ONE HOUR DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'FIRST+1);
          END TSK;

          TASK BODY TSK IS
               TYPE RAT;
               TYPE ARAT IS ACCESS RAT;
               TYPE ARR IS ARRAY (1..1) OF TT;
               TYPE RAT IS
                    RECORD
                         A : ARAT;
                         T : ARR;
                    END RECORD;
               LIST : ARAT;
          BEGIN
               DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0
          END TSK;  -- FAILS IF INFINITE WAIT.

     BEGIN -- (C)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
                                   -- FAILS IF "INFINITE" LOOP.
               DELAY 0.4;  -- TODO: acats-delay-deviation: before was 4.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED WITHIN ONE " &
                       "HOUR - (C)");
          ELSE COMMENT ("TASK TERMINATED - (C)");
          END IF;

          IF GLOBAL /= 1 THEN
               FAILED ("UNALLOCATED TASK BODY EXECUTED - (C)");
          END IF;

     END; -- (C)    -- FAILS IF INFINITE WAIT (FOR TSK TO TERMINATE).

     --------------------------------------------------

     RESULT;
END C94003A;
 
