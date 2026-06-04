-- C94002G.ADA

-- OBJECTIVE:
--     CHECK THAT A NON-MASTER UNIT, WHICH ALLOCATES TASKS OF A GLOBAL
--     ACCESS TYPE, MUST TERMINATE WITHOUT WAITING FOR THE ALLOCATED
--     TASKS TO TERMINATE IF AN EXCEPTION IS RAISED BUT NOT HANDLED IN
--     THE NON-MASTER UNIT.

--     SUBTESTS ARE:
--        (A)  A SIMPLE TASK ALLOCATOR, IN A BLOCK.
--        (B)  A RECORD OF TASK ALLOCATOR, IN A SUBPROGRAM.
--        (C)  A RECORD OF ARRAY OF TASK ALLOCATOR, IN A TASK BODY, NOT
--             DURING RENDEZVOUS.
--        (D)  A LIMITED PRIVATE TASK ALLOCATOR, IN A TASK BODY, DURING
--             RENDEZVOUS.

-- HISTORY:
--     TBN 01/20/86  CREATED ORIGINAL TEST.
--      JRK 05/01/86  IMPROVED ERROR RECOVERY.  FIXED EXCEPTION
--                    HANDLING.  ADDED CASE (D).
--      BCB 09/24/87  ADDED A RETURN STATEMENT TO THE HANDLER FOR OTHERS
--                    IN FUNCTION F, CASE B.

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94002G IS

     MY_EXCEPTION : EXCEPTION;

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
     TEST ("C94002G", "CHECK THAT A NON-MASTER UNIT, WHICH ALLOCATES " &
                      "TASKS OF A GLOBAL ACCESS TYPE, MUST TERMINATE " &
                      "WITHOUT WAITING FOR THE ALLOCATED TASKS TO " &
                      "TERMINATE IF AN EXCEPTION IS RAISED BUT NOT " &
                      "HANDLED IN THE NON-MASTER UNIT");

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
               RAISE MY_EXCEPTION;
               FAILED ("MY_EXCEPTION WAS NOT RAISED IN (A)");
          END;

          ABORT A1.ALL;

     EXCEPTION
          WHEN MY_EXCEPTION =>
               IF A1.ALL'TERMINATED THEN
                    FAILED ("ALLOCATED TASK PREMATURELY TERMINATED - " &
                            "(A)");
               ELSE A1.ALL.E;
               END IF;
          WHEN OTHERS =>
               FAILED ("UNEXPECTED EXCEPTION IN (A)");
               IF A1 /= NULL THEN
                    ABORT A1.ALL;
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

               PROCEDURE P IS
                    AR2 : ART;
               BEGIN
                    AR2 := NEW RT;
                    AR2.T.E;
                    AR1 := AR2;
                    RAISE MY_EXCEPTION;
                    FAILED ("MY_EXCEPTION WAS NOT RAISED IN (B)");
               END P;

          BEGIN
               P;
               ABORT AR1.T;
               RETURN 0;
          EXCEPTION
               WHEN MY_EXCEPTION =>
                    IF AR1.T'TERMINATED THEN
                         FAILED ("ALLOCATED TASK PREMATURELY " &
                                 "TERMINATED - (B)");
                    ELSE AR1.T.E;
                    END IF;
                    RETURN 0;
               WHEN OTHERS =>
                    FAILED ("UNEXPECTED EXCEPTION IN (B)");
                    IF AR1 /= NULL THEN
                         ABORT AR1.T;
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
                    ARA2 := NEW RAT;         -- INITIATE TASK ARA2.T(1).
                    ARA2.T(1).E;
                    ACCEPT ENT1 (ARA : OUT ARAT) DO
                         ARA := ARA2;
                    END ENT1;
                    RAISE MY_EXCEPTION;       -- NOT PROPOGATED.
                    FAILED ("MY_EXCEPTION WAS NOT RAISED IN (C)");
               END TSK1;

          BEGIN
               TSK1.ENT1 (ARA1);     -- ARA1.T BECOMES ALIAS FOR ARA2.T.

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

     DECLARE -- (D)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 60;                -- DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST-1);
          END TSK;

          TASK BODY TSK IS

               LOOP_COUNT1 : INTEGER := 0;
               CUT_OFF1 : CONSTANT := 60;          -- DELAY.

               PACKAGE PKG IS
                    TYPE LPT IS LIMITED PRIVATE;
                    PROCEDURE CALL (X : LPT);
                    PROCEDURE KILL (X : LPT);
                    FUNCTION TERMINATED (X : LPT) RETURN BOOLEAN;
               PRIVATE
                    TYPE LPT IS NEW TT;
               END PKG;

               USE PKG;

               TYPE ALPT IS ACCESS LPT;
               ALP1 : ALPT;

               PACKAGE BODY PKG IS
                    PROCEDURE CALL (X : LPT) IS
                    BEGIN
                         X.E;
                    END CALL;

                    PROCEDURE KILL (X : LPT) IS
                    BEGIN
                         ABORT X;
                    END KILL;

                    FUNCTION TERMINATED (X : LPT) RETURN BOOLEAN IS
                    BEGIN
                         RETURN X'TERMINATED;
                    END TERMINATED;
               END PKG;

               TASK TSK1 IS
                    ENTRY ENT1 (ALP : OUT ALPT);
                    ENTRY DIE;
                    PRAGMA PRIORITY (PRIORITY'LAST);
               END TSK1;

               TASK BODY TSK1 IS
                    ALP2 : ALPT;
               BEGIN
                    ALP2 := NEW LPT;         -- INITIATE TASK ALP2.ALL.
                    CALL (ALP2.ALL);
                    ACCEPT ENT1 (ALP : OUT ALPT) DO
                         ALP := ALP2;
                    END ENT1;
                    ACCEPT DIE DO
                         RAISE MY_EXCEPTION;       -- PROPOGATED.
                         FAILED ("MY_EXCEPTION WAS NOT RAISED IN (D)");
                    END DIE;
               END TSK1;

          BEGIN
               TSK1.ENT1 (ALP1); -- ALP1.ALL BECOMES ALIAS FOR ALP2.ALL.
               TSK1.DIE;
               FAILED ("MY_EXCEPTION WAS NOT PROPOGATED TO CALLING " &
                       "TASK - (D)");
               KILL (ALP1.ALL);
               ABORT TSK1;
          EXCEPTION
               WHEN MY_EXCEPTION =>
                    WHILE NOT TSK1'TERMINATED AND
                          LOOP_COUNT1 < CUT_OFF1 LOOP
                         DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                         LOOP_COUNT1 := LOOP_COUNT1 + 1;
                    END LOOP;

                    IF LOOP_COUNT1 >= CUT_OFF1 THEN
                         FAILED ("DEPENDENT TASK TSK1 NOT TERMINATED " &
                                 "WITHIN ONE MINUTE - (D)");
                    END IF;

                    IF TERMINATED (ALP1.ALL) THEN
                         FAILED ("ALLOCATED TASK PREMATURELY " &
                                 "TERMINATED - (D)");
                    ELSE CALL (ALP1.ALL);
                    END IF;
               WHEN OTHERS =>
                    FAILED ("UNEXPECTED EXCEPTION IN (D)");
                    IF ALP1 /= NULL THEN
                         KILL (ALP1.ALL);
                    END IF;
                    ABORT TSK1;
          END TSK;

     BEGIN -- (D)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
               DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK TSK NOT TERMINATED WITHIN " &
                       "TWO MINUTES - (D)");
          END IF;

     END; -- (D)

     --------------------------------------------------

     RESULT;
END C94002G;
