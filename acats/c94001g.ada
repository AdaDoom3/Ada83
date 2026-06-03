-- C94001G.ADA

-- CHECK THAT A COMPLETED TASK WITH DEPENDENT TASKS TERMINATES WHEN
-- A L L  DEPENDENT TASKS HAVE TERMINATED.

-- WEI  3/ 4/82
-- JBG 4/2/84
-- JWC 6/28/85   RENAMED FROM C940AIA-B.ADA

WITH REPORT;
 USE REPORT;
PROCEDURE C94001G IS

     PACKAGE SPY IS      -- PROVIDE PROTECTED ACCESS TO SPYNUMB
          SUBTYPE ARG IS NATURAL RANGE 0..9;
          FUNCTION SPYNUMB RETURN NATURAL;                      -- READ
          FUNCTION FINIT_POS (DIGT : IN ARG) RETURN NATURAL;    -- WRITE
          PROCEDURE PSPY_NUMB (DIGT : IN ARG);                  -- WRITE
     END SPY;

     USE SPY;

     PACKAGE BODY SPY IS

          TASK GUARD IS
               ENTRY READ  (NUMB : OUT NATURAL);
               ENTRY WRITE (NUMB : IN NATURAL);
          END GUARD;

          TASK BODY GUARD IS
               SPYNUMB : NATURAL := 0;
          BEGIN
               LOOP
                    SELECT
                         ACCEPT READ (NUMB : OUT NATURAL) DO
                              NUMB := SPYNUMB;
                         END READ;
                    OR   ACCEPT WRITE (NUMB : IN NATURAL) DO
                              SPYNUMB := 10*SPYNUMB+NUMB;
                         END WRITE;
                    OR   TERMINATE;
                    END SELECT;
               END LOOP;
          END GUARD;

          FUNCTION SPYNUMB RETURN NATURAL IS
               TEMP : NATURAL;
          BEGIN
               GUARD.READ (TEMP);
               RETURN TEMP;
          END SPYNUMB;

          FUNCTION FINIT_POS (DIGT: IN ARG) RETURN NATURAL IS
          BEGIN
               GUARD.WRITE (DIGT);
               RETURN DIGT;
          END FINIT_POS;

          PROCEDURE PSPY_NUMB (DIGT : IN ARG) IS
          BEGIN
               GUARD.WRITE (DIGT);
          END PSPY_NUMB;
     END SPY;

BEGIN
     TEST ("C94001G", "TERMINATION WHEN ALL DEPENDENT TASKS " &
                      "HAVE TERMINATED");

BLOCK:
     DECLARE

          TASK TYPE TT1;

          TASK BODY TT1 IS
          BEGIN
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               PSPY_NUMB (1);
          END TT1;

          TASK T1 IS
          END T1;

          TASK BODY T1 IS
               OBJ_TT1_1, OBJ_TT1_2, OBJ_TT1_3 : TT1;
          BEGIN
               NULL;
          END T1;

     BEGIN
          NULL;
     END BLOCK;               -- WAIT HERE FOR TERMINATION.

     IF SPYNUMB /= 111 THEN
          FAILED ("TASK T1 TERMINATED BEFORE " &
                  "ALL DEPENDENT TASKS HAVE TERMINATED");
          COMMENT ("ACTUAL ORDER WAS:" & INTEGER'IMAGE(SPYNUMB));
     END IF;

     RESULT;

END C94001G;
