-- C97302A.ADA

-- CHECK THAT WHENEVER AN INDEX IS PRESENT IN A TIMED_ENTRY_CALL, IT
-- IS EVALUATED BEFORE ANY PARAMETER ASSOCIATIONS ARE EVALUATED, AND 
-- PARAMETER ASSOCIATIONS ARE EVALUATED BEFORE THE DELAY EXPRESSION.
-- THEN A RENDEZVOUS IS ATTEMPTED.

-- RJW 3/31/86

WITH REPORT; USE REPORT;
WITH CALENDAR; USE CALENDAR;
PROCEDURE C97302A IS
     
     INDEX_COMPUTED  :  BOOLEAN  :=  FALSE;
     PARAM_COMPUTED  :  BOOLEAN  :=  FALSE;     
     DELAY_COMPUTED  :  BOOLEAN  :=  FALSE;
BEGIN

     TEST ("C97302A", "CHECK THAT WHENEVER AN INDEX IS PRESENT IN " &
                      "A TIMED_ENTRY_CALL, IT IS EVALUATED BEFORE " &
                      "ANY PARAMETER ASSOCIATIONS ARE EVALUATED, " &
                      "AND PARAMETER ASSOCIATIONS ARE EVALUATED " & 
                      "BEFORE THE DELAY EXPRESSION" );
     DECLARE

          WAIT_TIME : DURATION := 0.3;  -- TODO: acats-delay-deviation: before was 3.0
          
          TYPE SHORT IS RANGE 10 .. 20;

          TASK  T  IS
               ENTRY  DO_IT_NOW_OR_WAIT
                                      ( SHORT )
                                      ( DID_YOU_DO_IT : IN BOOLEAN );
               ENTRY  KEEP_ALIVE;
          END  T;

          TASK BODY  T  IS
          BEGIN
               ACCEPT  KEEP_ALIVE;
          END  T;

          FUNCTION  F1 (X : SHORT) RETURN SHORT IS
          BEGIN
               INDEX_COMPUTED  :=  TRUE;
               RETURN (15);
          END  F1;

          FUNCTION  F2 RETURN BOOLEAN  IS
          BEGIN
               IF INDEX_COMPUTED THEN
                    NULL;
               ELSE
                    FAILED ( "INDEX NOT EVALUATED FIRST" );
               END IF;
               PARAM_COMPUTED  :=  TRUE;
               RETURN (FALSE);
          END  F2;

          FUNCTION F3 RETURN DURATION IS
          BEGIN
               IF PARAM_COMPUTED THEN
                    NULL;
               ELSE
                    FAILED ( "PARAMETERS NOT EVALUATED BEFORE DELAY " &
                             "EXPRESSION" );
               END IF;
               DELAY_COMPUTED := TRUE;
               RETURN (WAIT_TIME);
          END;
     BEGIN

          SELECT
               T.DO_IT_NOW_OR_WAIT
                                      ( F1 (15) )
                                      ( NOT F2 );
               FAILED ("RENDEZVOUS OCCURRED");
          OR
               DELAY F3;          
          END SELECT;

          T.KEEP_ALIVE;

     END;   -- END OF BLOCK CONTAINING THE ENTRY CALLS.

     IF  DELAY_COMPUTED  THEN
          NULL;
     ELSE
          FAILED( "DELAY EXPRESSION NOT EVALUATED" );
     END IF;

     RESULT;

END  C97302A; 
