-- C97305B.ADA

-- CHECK THAT IF THE RENDEZVOUS IS IMMEDIATELY POSSIBLE (FOR A
-- TIMED ENTRY CALL), IT IS PERFORMED.

-- CASE B: ENTRY FAMILY; THE CALLED TASK IS EXECUTING A SELECTIVE WAIT.

-- WRG 7/13/86

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C97305B IS

     RENDEZVOUS_OCCURRED            : BOOLEAN  := FALSE;
     STATEMENTS_AFTER_CALL_EXECUTED : BOOLEAN  := FALSE;
     COUNT                          : POSITIVE := 1;
     ZERO                           : DURATION := 1.0;

     PRAGMA PRIORITY (PRIORITY'FIRST);           -- LOWEST PRIORITY.

BEGIN

     TEST ("C97305B", "CHECK THAT IF THE RENDEZVOUS IS IMMEDIATELY " &
                      "POSSIBLE (FOR A TIMED ENTRY CALL), IT " &
                      "IS PERFORMED");

     IF EQUAL (3, 3) THEN
          ZERO := 0.0;
     END IF;

     DECLARE

          TASK T IS
               PRAGMA PRIORITY (PRIORITY'LAST);  -- HIGHEST PRIORITY.
               ENTRY E (1..3) (B : IN OUT BOOLEAN);
          END T;

          TASK BODY T IS
          BEGIN
               SELECT
                    ACCEPT E (2) (B : IN OUT BOOLEAN) DO
                         B := IDENT_BOOL (TRUE);
                    END E;
               OR
                    ACCEPT E (3) (B : IN OUT BOOLEAN);
                    FAILED ("NONEXISTENT ENTRY CALL ACCEPTED");
               END SELECT;
          END T;

     BEGIN

          WHILE NOT STATEMENTS_AFTER_CALL_EXECUTED LOOP
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0

               SELECT
                    T.E (2) (RENDEZVOUS_OCCURRED);
                    STATEMENTS_AFTER_CALL_EXECUTED := IDENT_BOOL (TRUE);
               OR
                    DELAY ZERO;
                    IF COUNT < 60 * 60 THEN
                         COUNT := COUNT + 1;
                    ELSE
                         FAILED ("NO RENDEZVOUS AFTER AT LEAST ONE " &
                                 "HOUR ELAPSED");
                         EXIT;
                    END IF;
               END SELECT;
          END LOOP;

     END;

     IF NOT RENDEZVOUS_OCCURRED THEN
          FAILED ("RENDEZVOUS DID NOT OCCUR");
     END IF;

     IF COUNT > 1 THEN
          COMMENT ("DELAYED" & POSITIVE'IMAGE(COUNT) & " SECONDS");
     END IF;

     RESULT;

END C97305B;
