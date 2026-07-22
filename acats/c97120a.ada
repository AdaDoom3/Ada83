-- C97120A.ADA

-- CHECK THAT A SELECTIVE WAIT DELAYS AT LEAST AS LONG AS IS SPECIFIED
-- IN A DELAY ALTERNATIVE.

-- WRG 7/11/86

WITH REPORT;   USE REPORT;
WITH CALENDAR; USE CALENDAR;
PROCEDURE C97120A IS

BEGIN

     TEST ("C97120A", "CHECK THAT A SELECTIVE WAIT DELAYS AT LEAST " &
                      "AS LONG AS IS SPECIFIED IN A DELAY ALTERNATIVE");

     DECLARE

          TASK T IS
               ENTRY NO_GO;
               ENTRY SYNCH;
          END T;

          TASK BODY T IS
               BEFORE, AFTER : TIME;
          BEGIN
               -- ENSURE THAT SYNCH HAS BEEN CALLED BEFORE PROCEEDING:
               WHILE SYNCH'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               BEFORE := CLOCK;
               SELECT
                    ACCEPT NO_GO;
                    FAILED ("ACCEPTED NONEXISTENT ENTRY CALL");
               OR
                    DELAY 1.0;  -- TODO: acats-delay-deviation: before was 10.0
                    AFTER := CLOCK;
                    IF AFTER - BEFORE < 1.0 THEN  -- TODO: acats-delay-deviation: before was 10.0
                         FAILED ("INSUFFICIENT DELAY");
                    END IF;
               END SELECT;

               ACCEPT SYNCH;
          EXCEPTION
               WHEN OTHERS =>
                    FAILED ("EXCEPTION RAISED");
          END T;

     BEGIN

          T.SYNCH;  -- SUSPEND MAIN TASK BEFORE READING CLOCK.

     END;

     RESULT;

END C97120A;
