-- C94002E.ADA

-- CHECK THAT A NON-MASTER UNIT, WHICH ALLOCATES TASKS OF A GLOBAL
--   ACCESS TYPE, MUST TERMINATE WITHOUT WAITING FOR THE ALLOCATED TASKS
--   TO TERMINATE.

-- SUBTESTS ARE:
--   (A)  A SIMPLE TASK ALLOCATOR, IN A BLOCK.
--   (B)  A RECORD OF TASK ALLOCATOR, IN A SUBPROGRAM.
--   (C)  A RECORD OF ARRAY OF TASK ALLOCATOR, IN A TASK BODY.

-- JRK 10/8/81
-- SPS 11/2/82
-- SPS 11/21/82
-- JRK 11/29/82
-- TBN 1/20/86     RENAMED FROM C94006A-B.ADA.  LOWERED THE DELAY VALUES
--                 AND MODIFIED THE COMMENTS.
-- JRK 5/1/86      IMPROVED ERROR RECOVERY LOGIC.

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94002E IS

     TASK TYPE TT IS
          ENTRY E;
          PRAGMA PRIORITY (PRIORITY'FIRST);
     END TT;

     TASK BODY TT IS
     BEGIN
          ACCEPT E;
          ACCEPT E;
     END TT;

     PRAGMA PRIORITY (PRIORITY'LAST-2);

BEGIN
     TEST ("C94002E", "CHECK THAT A NON-MASTER UNIT, WHICH ALLOCATES " &
                      "TASKS OF A GLOBAL ACCESS TYPE, MUST TERMINATE " &
                      "WITHOUT WAITING FOR THE ALLOCATED TASKS TO " &
                      "TERMINATE");

     --------------------------------------------------

     DECLARE -- (A)

          TYPE A_T IS ACCESS TT;
          A1 : A_T;

     BEGIN -- (A)

          DECLARE
               A2 : A_T;
          BEGIN
               A2 := NEW TT;
               A2.ALL.E;
               A1 := A2;
          END;

          IF A1.ALL'TERMINATED THEN
               FAILED ("ALLOCATED TASK PREMATURELY TERMINATED - (A)");
          ELSE A1.ALL.E;
          END IF;

     END; -- (A)

     --------------------------------------------------

     DECLARE -- (B)

          I : INTEGER;

          FUNCTION F RETURN INTEGER IS

               TYPE RT IS
                    RECORD
                         T : TT;
                    END RECORD;
               TYPE ART IS ACCESS RT;
               AR1 : ART;

               PROCEDURE P (AR : OUT ART) IS
                    AR2 : ART;
               BEGIN
                    AR2 := NEW RT;
                    AR2.T.E;
                    AR := AR2;
               END P;

          BEGIN
               P (AR1);

               IF AR1.T'TERMINATED THEN
                    FAILED ("ALLOCATED TASK PREMATURELY TERMINATED " &
                            "- (B)");
               ELSE AR1.T.E;
               END IF;

               RETURN 0;
          END F;

     BEGIN -- (B)

          I := F;

     END; -- (B)

     --------------------------------------------------

     DECLARE -- (C)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 60;                -- DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST-1);
          END TSK;

          TASK BODY TSK IS

               LOOP_COUNT1 : INTEGER := 0;
               CUT_OFF1 : CONSTANT := 60;          -- DELAY.

               TYPE RAT;
               TYPE ARAT IS ACCESS RAT;
               TYPE ARR IS ARRAY (1..1) OF TT;
               TYPE RAT IS
                    RECORD
                         A : ARAT;
                         T : ARR;
                    END RECORD;
               ARA1 : ARAT;

               TASK TSK1 IS
                    ENTRY ENT1 (ARA : OUT ARAT);
                    PRAGMA PRIORITY (PRIORITY'LAST);
               END TSK1;

               TASK BODY TSK1 IS
                    ARA2 : ARAT;
               BEGIN
                    ARA2 := NEW RAT;
                    ARA2.T(1).E;
                    ACCEPT ENT1 (ARA : OUT ARAT) DO
                         ARA := ARA2;
                    END ENT1;
               END TSK1;

          BEGIN
               TSK1.ENT1 (ARA1);

               WHILE NOT TSK1'TERMINATED AND LOOP_COUNT1 < CUT_OFF1 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    LOOP_COUNT1 := LOOP_COUNT1 + 1;
               END LOOP;

               IF LOOP_COUNT1 >= CUT_OFF1 THEN
                    FAILED ("DEPENDENT TASK TSK1 NOT TERMINATED " &
                            "WITHIN ONE MINUTE - (C)");
               END IF;

               IF ARA1.T(1)'TERMINATED THEN
                    FAILED ("ALLOCATED TASK PREMATURELY TERMINATED " &
                            "- (C)");
               ELSE ARA1.T(1).E;
               END IF;
          END TSK;

     BEGIN -- (C)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
               DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK TSK NOT TERMINATED WITHIN " &
                       "TWO MINUTES - (C)");
          END IF;

     END; -- (C)

     --------------------------------------------------

     RESULT;
END C94002E;
