-- C97120B.ADA

-- CHECK THAT IF A SPECIFIED DELAY IS ZERO OR NEGATIVE AND AN ENTRY CALL
-- IS WAITING AT AN OPEN ALTERNATIVE WHEN THE SELECTIVE WAIT IS
-- EXECUTED, THE CALL IS ACCEPTED.

-- WRG 7/11/86

WITH REPORT; USE REPORT;
PROCEDURE C97120B IS

     ZERO, NEG : DURATION := 1.0;

BEGIN

     TEST ("C97120B", "CHECK THAT IF A SPECIFIED DELAY IS ZERO OR " &
                      "NEGATIVE AND AN ENTRY CALL IS WAITING AT AN " &
                      "OPEN ALTERNATIVE WHEN THE SELECTIVE WAIT IS " &
                      "EXECUTED, THE CALL IS ACCEPTED");

     IF EQUAL (3, 3) THEN
          ZERO :=  0.0;
          NEG  := -1.0;
     END IF;

     DECLARE

          TASK T IS
               ENTRY E;
          END T;

          TASK BODY T IS
          BEGIN
               WHILE E'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

            A: BEGIN
                    SELECT
                         WHEN IDENT_BOOL (TRUE) =>
                              ACCEPT E;
                    OR
                         DELAY ZERO;
                         FAILED ("ZERO DELAY ALTERNATIVE TAKEN");
                         ACCEPT E;
                    END SELECT;
               EXCEPTION
                    WHEN OTHERS =>
                         FAILED ("EXCEPTION RAISED (A)");
               END A;

               WHILE E'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

            B: BEGIN
                    SELECT
                         ACCEPT E;
                    OR
                         DELAY NEG;
                         FAILED ("NEGATIVE DELAY ALTERNATIVE TAKEN");
                         ACCEPT E;
                    END SELECT;
               EXCEPTION
                    WHEN OTHERS =>
                         FAILED ("EXCEPTION RAISED (B)");
               END B;

          END T;

     BEGIN

          T.E;
          T.E;

     END;

     RESULT;

END C97120B;
