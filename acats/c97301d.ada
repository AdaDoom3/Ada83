-- C97301D.ADA

-- CHECK THAT A TIMED_ENTRY_CALL DELAYS FOR AT LEAST THE SPECIFIED
-- AMOUNT OF TIME IF A RENDEVOUS IS NOT POSSIBLE.

-- CASE  D:  THE BODY OF THE TASK CONTAINING THE CALLED ENTRY
--           DOES NOT CONTAIN AN ACCEPT_STATEMENT FOR THAT ENTRY.

-- RJW 3/31/86

WITH REPORT; USE REPORT;
WITH CALENDAR; USE CALENDAR;
PROCEDURE C97301D IS

     OR_BRANCH_TAKEN : BOOLEAN := FALSE;

BEGIN

     TEST ("C97301D", "CHECK THAT A TIMED_ENTRY_CALL DELAYS FOR AT " &
                      "LEAST THE SPECIFIED AMOUNT OF TIME WHEN THE " &
                      "BODY OF THE TASK CONTAINING THE CALLED ENTRY " &
                      "DOES NOT CONTAIN AN ACCEPT_STATEMENT FOR " &
                      "THAT ENTRY" );

     DECLARE
          START_TIME : TIME;
          WAIT_TIME : CONSTANT DURATION := 1.0;  -- TODO: acats-delay-deviation: before was 10.0

          TASK  T  IS
               ENTRY  DO_IT_NOW_OR_WAIT;
               ENTRY  KEEP_ALIVE;
          END  T;

          TASK BODY  T  IS
          BEGIN

               -- NO ACCEPT_STATEMENT FOR THE ENTRY_CALL BEING TESTED.

               ACCEPT  KEEP_ALIVE;  -- TO PREVENT THIS SERVER TASK FROM
                                     --     TERMINATING IF
                                     --     UPON ACTIVATION
                                     --     IT GETS TO RUN
                                     --     AHEAD OF THE CALLER (WHICH
                                     --     WOULD LEAD TO A SUBSEQUENT
                                     --     TASKING_ERROR AT THE TIME
                                     --     OF THE NO-WAIT CALL).

          END;

     BEGIN
          START_TIME := CLOCK;
          SELECT
               T.DO_IT_NOW_OR_WAIT;
          OR
                            --  THIS BRANCH MUST BE CHOSEN.
               DELAY WAIT_TIME;
               IF CLOCK >= (WAIT_TIME + START_TIME) THEN
                    NULL;
               ELSE
                    FAILED ( "INSUFFICIENT WAITING TIME" );
               END IF;
               OR_BRANCH_TAKEN := TRUE;
               COMMENT( "OR_BRANCH  TAKEN" );
          END SELECT;

          T.KEEP_ALIVE;
     EXCEPTION
          WHEN TASKING_ERROR =>
               FAILED ( "TASKING ERROR RAISED" );

     END;   -- END OF BLOCK CONTAINING THE ENTRY CALL.

     -- BY NOW, THE TASK IS TERMINATED.

     IF  OR_BRANCH_TAKEN  THEN
          NULL;
     ELSE
          FAILED( "RENDEZVOUS ATTEMPTED?" );
     END IF;

     RESULT;

END  C97301D;
