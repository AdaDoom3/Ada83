-- C97112A.ADA

-- CHECK THAT A DELAY STATEMENT IS ALLOWED IN THE SEQUENCE OF STATEMENTS
-- OF A SELECT ALTERNATIVE OF A SELECTIVE WAIT CONTAINING A TERMINATE
-- ALTERNATIVE OR AN ELSE PART.

-- WRG 7/9/86

WITH REPORT;   USE REPORT;
WITH CALENDAR; USE CALENDAR;
PROCEDURE C97112A IS

     ACCEPT_ALTERNATIVE_TAKEN : BOOLEAN := FALSE;

BEGIN

     TEST ("C97112A", "CHECK THAT A DELAY STATEMENT IS ALLOWED IN " &
                      "THE SEQUENCE OF STATEMENTS OF A SELECT " &
                      "ALTERNATIVE OF A SELECTIVE WAIT CONTAINING A " &
                      "TERMINATE ALTERNATIVE OR AN ELSE PART");

     --------------------------------------------------

  A: DECLARE

          TASK T IS
               ENTRY E;
          END T;

          TASK BODY T IS
               BEFORE, AFTER : TIME;
          BEGIN
               SELECT
                    ACCEPT E;
                    ACCEPT_ALTERNATIVE_TAKEN := TRUE;
                    BEFORE := CLOCK;
                    DELAY 1.0;  -- TODO: acats-delay-deviation: before was 10.0
                    AFTER  := CLOCK;
                    IF AFTER - BEFORE < 1.0 THEN  -- TODO: acats-delay-deviation: before was 10.0
                         FAILED ("INSUFFICIENT DELAY (A)");
                    END IF;
               OR
                    TERMINATE;
               END SELECT;
          END T;

     BEGIN

          T.E;

     END A;

     IF NOT ACCEPT_ALTERNATIVE_TAKEN THEN
          FAILED ("ACCEPT ALTERNATIVE NOT TAKEN");
     END IF;

     --------------------------------------------------

  B: DECLARE

          TASK T IS
               ENTRY E;
          END T;

          TASK BODY T IS
               BEFORE, AFTER : TIME;
          BEGIN
               --ENSURE THAT E HAS BEEN CALLED BEFORE PROCEEDING:
               WHILE E'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               SELECT
                    ACCEPT E;
                    BEFORE := CLOCK;
                    DELAY 1.0;  -- TODO: acats-delay-deviation: before was 10.0
                    AFTER  := CLOCK;
                    IF AFTER - BEFORE < 1.0 THEN  -- TODO: acats-delay-deviation: before was 10.0
                         FAILED ("INSUFFICIENT DELAY (B-1)");
                    END IF;
               ELSE
                    FAILED ("ELSE PART EXECUTED (B-1)");
               END SELECT;

               SELECT
                    ACCEPT E;
                    FAILED ("ACCEPT STATEMENT EXECUTED (B-2)");
               ELSE
                    BEFORE := CLOCK;
                    DELAY 1.0;  -- TODO: acats-delay-deviation: before was 10.0
                    AFTER  := CLOCK;
                    IF AFTER - BEFORE < 1.0 THEN  -- TODO: acats-delay-deviation: before was 10.0
                         FAILED ("INSUFFICIENT DELAY (B-2)");
                    END IF;
               END SELECT;
          EXCEPTION
               WHEN OTHERS =>
                    FAILED ("EXCEPTION RAISED");
          END T;

     BEGIN

          T.E;

     END B;

     --------------------------------------------------

     RESULT;

END C97112A;
