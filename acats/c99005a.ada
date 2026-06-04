-- C99005A.ADA

-- OBJECTIVE:
--     CHECK THAT THE ATTRIBUTE 'COUNT RETURNS THE CORRECT VALUE.

-- HISTORY:
--     DHH 03/24/88 CREATED ORIGINAL TEST.

WITH SYSTEM; USE SYSTEM;
WITH REPORT; USE REPORT;
PROCEDURE C99005A IS

BEGIN

     TEST("C99005A", "CHECK THAT THE ATTRIBUTE 'COUNT RETURNS THE " &
                     "CORRECT VALUE");

     DECLARE
          TASK A IS
          END A;

          TASK B IS
          END B;

          TASK C IS
          END C;

          TASK D IS
          END D;

          TASK E IS
          END E;

          TASK F IS
          END F;

          TASK G IS
          END G;

          TASK H IS
          END H;

          TASK I IS
          END I;

          TASK J IS
          END J;

          TASK T IS
               ENTRY WAIT;
          END T;

          TASK CHOICE IS
               ENTRY RETURN_CALL;
               ENTRY E2;
               ENTRY E1;
          END CHOICE;

          TASK BODY A IS
          BEGIN
               CHOICE.E1;
          END A;

          TASK BODY B IS
          BEGIN
               CHOICE.E1;
          END B;

          TASK BODY C IS
          BEGIN
               CHOICE.E1;
          END C;

          TASK BODY D IS
          BEGIN
               CHOICE.E1;
          END D;

          TASK BODY E IS
          BEGIN
               CHOICE.E1;
          END E;

          TASK BODY F IS
          BEGIN
               CHOICE.E2;
          END F;

          TASK BODY G IS
          BEGIN
               CHOICE.E2;
          END G;

          TASK BODY H IS
          BEGIN
               CHOICE.E2;
          END H;

          TASK BODY I IS
          BEGIN
               CHOICE.E2;
          END I;

          TASK BODY J IS
          BEGIN
               CHOICE.E2;
          END J;

          TASK BODY T IS
          BEGIN
               LOOP
                    SELECT
                         ACCEPT WAIT DO
                              DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                         END WAIT;
                         CHOICE.RETURN_CALL;
                    OR
                         TERMINATE;
                    END SELECT;
               END LOOP;
          END T;

          TASK BODY CHOICE IS
          BEGIN
               WHILE E1'COUNT + E2'COUNT < 10 LOOP
                    T.WAIT;
                    ACCEPT RETURN_CALL;
               END LOOP;

               FOR I IN REVERSE 1 ..10 LOOP
                    SELECT
                         ACCEPT E2 DO
                              IF (E2'COUNT + E1'COUNT + 1) /= I THEN
                                   FAILED("'COUNT NOT RETURNING " &
                                          "CORRECT VALUE FOR LOOP" &
                                           INTEGER'IMAGE(I) & "VALUE " &
                                           INTEGER'IMAGE((E2'COUNT
                                           + E1'COUNT + 1)));
                              END IF;
                         END E2;
                    OR
                         ACCEPT E1 DO
                              IF (E2'COUNT + E1'COUNT + 1) /= I THEN
                                   FAILED("'COUNT NOT RETURNING " &
                                          "CORRECT VALUE FOR LOOP" &
                                           INTEGER'IMAGE(I) & "VALUE " &
                                           INTEGER'IMAGE((E2'COUNT
                                           + E1'COUNT + 1)));
                              END IF;
                         END E1;
                    END SELECT;
               END LOOP;
           END CHOICE;

     BEGIN
          NULL;
     END;

     RESULT;
END C99005A;
