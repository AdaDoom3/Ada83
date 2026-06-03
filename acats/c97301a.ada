-- C97301A.ADA

-- CHECK THAT A TIMED_ENTRY_CALL DELAYS FOR AT LEAST THE SPECIFIED
-- AMOUNT OF TIME IF A RENDEVOUS IS NOT POSSIBLE.

-- CASE  A:  THE TASK TO BE CALLED HAS NOT YET BEEN ACTIVATED AS OF THE
--           MOMENT OF CALL.

-- RJW 3/31/86

WITH REPORT; USE REPORT;
WITH CALENDAR; USE CALENDAR;
PROCEDURE C97301A IS

     WAIT_TIME : CONSTANT DURATION := 1.0;  -- TODO: acats-delay-deviation: before was 10.0
     OR_BRANCH_TAKEN : INTEGER := 3;

BEGIN

     TEST ("C97301A", "CHECK THAT A TIMED_ENTRY_CALL DELAYS FOR AT " &
                      "LEAST THE SPECIFIED AMOUNT OF TIME WHEN THE " &
                      "CALLED TASK IS NOT ACTIVE" );

     ------------------------------------------------------------------

     DECLARE

          TASK  T  IS
               ENTRY  DO_IT_NOW_OR_WAIT ( AUTHORIZED : IN BOOLEAN );
          END  T;

          TASK BODY  T  IS

               PACKAGE       SECOND_ATTEMPT  IS    END  SECOND_ATTEMPT;
               PACKAGE BODY  SECOND_ATTEMPT  IS
                    START_TIME : TIME;
               BEGIN
                    START_TIME := CLOCK;
                    SELECT
                         DO_IT_NOW_OR_WAIT (FALSE); --CALLING OWN ENTRY.
                    OR
                                      -- THEREFORE THIS BRANCH
                                      --    MUST BE CHOSEN.
                         DELAY WAIT_TIME;
                         IF CLOCK >= (WAIT_TIME + START_TIME) THEN
                              NULL;
                         ELSE
                              FAILED ( "INSUFFICIENT DELAY (#2)" );
                         END IF;
                         OR_BRANCH_TAKEN := 2 * OR_BRANCH_TAKEN;
                         COMMENT( "OR_BRANCH  TAKEN  (#2)" );
                    END SELECT;
               END  SECOND_ATTEMPT;

          BEGIN

               ACCEPT DO_IT_NOW_OR_WAIT ( AUTHORIZED : IN BOOLEAN )  DO

                    IF  AUTHORIZED  THEN
                         COMMENT( "AUTHORIZED ENTRY_CALL" );
                    ELSE
                         FAILED( "UNAUTHORIZED ENTRY_CALL" );
                    END IF;

               END DO_IT_NOW_OR_WAIT;


          END  T;


          PACKAGE       FIRST_ATTEMPT  IS      END  FIRST_ATTEMPT;
          PACKAGE BODY  FIRST_ATTEMPT  IS
               START_TIME : TIME;
          BEGIN
               START_TIME := CLOCK;
               SELECT
                    T.DO_IT_NOW_OR_WAIT (FALSE);
               OR
                                      -- THIS BRANCH MUST BE CHOSEN.
                    DELAY WAIT_TIME;
                    IF CLOCK >= (WAIT_TIME + START_TIME) THEN
                         NULL;
                    ELSE
                         FAILED ( "INSUFFICIENT DELAY (#1)" );
                    END IF;
                    OR_BRANCH_TAKEN := 1 + OR_BRANCH_TAKEN;
                    COMMENT( "OR_BRANCH  TAKEN  (#1)" );
               END SELECT;

          END  FIRST_ATTEMPT;

     BEGIN

          T.DO_IT_NOW_OR_WAIT ( TRUE );   -- TO SATISFY THE SERVER'S
                                          --     WAIT FOR SUCH A CALL.

     EXCEPTION

          WHEN  TASKING_ERROR  =>
               FAILED( "TASKING ERROR" );

     END ;


     ------------------------------------------------------------------


     -- BY NOW, THE TASK IS TERMINATED  (AND THE NONLOCALS UPDATED).


     CASE  OR_BRANCH_TAKEN  IS

          WHEN  3  =>
               FAILED( "NO 'OR'; BOTH (?) RENDEZVOUS ATTEMPTED?" );

          WHEN  4  =>
               FAILED( "'OR' #1 ONLY; RENDEZVOUS (#2) ATTEMPTED?" );

          WHEN  6  =>
               FAILED( "'OR' #2 ONLY; RENDEZVOUS (#1) ATTEMPTED?" );

          WHEN  7  =>
               FAILED( "WRONG ORDER FOR 'OR':  #2,#1" );

          WHEN  8  =>
               NULL;

          WHEN  OTHERS  =>
               FAILED( "WRONG CASE_VALUE" );

     END CASE;

     RESULT;

END  C97301A;
