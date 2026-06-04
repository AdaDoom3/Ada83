-- C97303A.ADA


-- CHECK THAT A TIMED_ENTRY_CALL CAN APPEAR IN PLACES WHERE A
--     SELECTIVE_WAIT  CANNOT.

-- PART 1: PACKAGE BODY EMBEDDED IN TASK BODY.


-- RM 4/06/1982


WITH REPORT;
USE REPORT;
PROCEDURE  C97303A  IS


BEGIN


     TEST ( "C97303A" , "CHECK THAT A  TIMED_ENTRY_CALL  CAN" &
                        " APPEAR WHERE A  SELECTIVE_WAIT  CANNOT" );


     -------------------------------------------------------------------


     DECLARE


          TASK  TT  IS
               ENTRY  A ( AUTHORIZED : IN BOOLEAN );
          END  TT ;


          TASK BODY  TT  IS

               PACKAGE  WITHIN_TASK_BODY  IS
                    -- NOTHING HERE
               END  WITHIN_TASK_BODY ;


               PACKAGE BODY  WITHIN_TASK_BODY  IS
               BEGIN

                    SELECT  -- NOT A SELECTIVE_WAIT
                         A ( FALSE ) ;  -- CALLING (OWN) ENTRY
                    OR
                         DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
                         COMMENT( "ALTERNATIVE BRANCH TAKEN" );
                    END SELECT;
                    
               END  WITHIN_TASK_BODY ;


          BEGIN

               ACCEPT  A ( AUTHORIZED : IN BOOLEAN )  DO

                    IF  AUTHORIZED  THEN
                         COMMENT(  "AUTHORIZED ENTRY_CALL" );
                    ELSE
                         FAILED( "UNAUTHORIZED ENTRY_CALL" );
                    END IF;

               END  A ;
   
          END  TT ;


          PACKAGE  OUTSIDE_TASK_BODY  IS
               -- NOTHING HERE
          END  OUTSIDE_TASK_BODY ;


          PACKAGE BODY  OUTSIDE_TASK_BODY  IS
          BEGIN

               SELECT  -- NOT A SELECTIVE_WAIT
                    TT.A ( FALSE ) ;  -- UNBORN
               OR 
                    DELAY  0.2 ;  -- TODO: acats-delay-deviation: before was 2.0
                    COMMENT( "(OUT:) ALTERNATIVE BRANCH TAKEN" );
               END SELECT;

          END  OUTSIDE_TASK_BODY ;


     BEGIN

          TT.A ( TRUE );

     EXCEPTION

          WHEN  TASKING_ERROR  =>
               FAILED( "TASKING ERROR" );

     END  ;


     -------------------------------------------------------------------


     RESULT ;


END  C97303A ;
