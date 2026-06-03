-- C97301C.ADA

-- CHECK THAT A TIMED_ENTRY_CALL DELAYS FOR AT LEAST THE SPECIFIED 
-- AMOUNT OF TIME IF A RENDEVOUS IS NOT POSSIBLE.

-- CASE  C:  AN ACCEPT STATEMENT FOR THE CALLED ENTRY HAS NOT BEEN
--           REACHED.

-- RJW 3/31/86

WITH REPORT; USE REPORT;
WITH CALENDAR; USE CALENDAR;
PROCEDURE C97301C IS
     
     OR_BRANCH_TAKEN : BOOLEAN := FALSE;
     
BEGIN

     TEST ("C97301C", "CHECK THAT A TIMED_ENTRY_CALL DELAYS FOR AT " &
                      "LEAST THE SPECIFIED AMOUNT OF TIME WHEN AN " &
                      "ACCEPT STATEMENT FOR THE CALLED ENTRY HAS " &
                      "NOT BEEN REACHED" );


     DECLARE
          START_TIME : TIME;
          STOP_TIME : TIME;
          WAIT_TIME  : DURATION :=  0.3;  -- TODO: acats-delay-deviation: before was 3.0

          TASK  T  IS
               ENTRY NO_SPIN;
               ENTRY DO_IT_NOW_OR_WAIT;
          END  T;

          TASK BODY  T  IS
          BEGIN
               ACCEPT NO_SPIN;
               ACCEPT DO_IT_NOW_OR_WAIT;
          END  T;

     BEGIN
          START_TIME := CLOCK;
          SELECT
               T.DO_IT_NOW_OR_WAIT;
               FAILED("RENDEZVOUS OCCURRED");
               ABORT T;
          OR
                                      -- THIS BRANCH MUST BE CHOSEN.
               DELAY WAIT_TIME;
               STOP_TIME := CLOCK;
               IF STOP_TIME >= (WAIT_TIME + START_TIME) THEN
                    NULL;
               ELSE
                    FAILED ( "INSUFFICIENT DELAY" );
               END IF;
               T.NO_SPIN;
               OR_BRANCH_TAKEN := TRUE;
               COMMENT( "OR_BRANCH TAKEN" );
               T.DO_IT_NOW_OR_WAIT;
          END SELECT;
     EXCEPTION
          WHEN TASKING_ERROR =>
               FAILED ( "TASKING ERROR" );
     END;
                                     -- END OF BLOCK CONTAINING TIMED 
                                     -- ENTRY CALL.

     -- BY NOW, TASK T IS TERMINATED (AND THE NONLOCALS UPDATED).
          
     IF OR_BRANCH_TAKEN THEN
          NULL;
     ELSE
          FAILED( "RENDEZVOUS ATTEMPTED" );
     END IF;

     RESULT;

END C97301C; 
