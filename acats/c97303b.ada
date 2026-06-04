-- C97303B.ADA


-- CHECK THAT A TIMED_ENTRY_CALL CAN APPEAR IN PLACES WHERE A
--     SELECTIVE_WAIT  CANNOT.

-- PART 2: PROCEDURE BODY EMBEDDED IN TASK BODY.


-- RM 4/12/1982


WITH REPORT;
USE REPORT;
PROCEDURE  C97303B  IS


BEGIN


     TEST ( "C97303B" , "CHECK THAT A  TIMED_ENTRY_CALL  CAN" &
                        " APPEAR WHERE A  SELECTIVE_WAIT  CANNOT" );


     -------------------------------------------------------------------


     DECLARE


          TASK  TT  IS
               ENTRY  A ( AUTHORIZED : IN BOOLEAN );
          END  TT ;


          TASK BODY  TT  IS


               PROCEDURE  WITHIN_TASK_BODY ;


               PROCEDURE  WITHIN_TASK_BODY  IS
               BEGIN

                    SELECT  -- NOT A SELECTIVE_WAIT
                         A ( FALSE ) ;  -- CALLING (OWN) ENTRY
                    OR
                         DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
                         COMMENT( "ALTERNATIVE BRANCH TAKEN" );
                    END SELECT;
                    
               END  WITHIN_TASK_BODY ;


          BEGIN


               -- CALL THE INNER PROC. TO FORCE EXEC. OF TIMED_E_CALL
               WITHIN_TASK_BODY ;


               ACCEPT  A ( AUTHORIZED : IN BOOLEAN )  DO

                    IF  AUTHORIZED  THEN
                         COMMENT(  "AUTHORIZED ENTRY_CALL" );
                    ELSE
                         FAILED( "UNAUTHORIZED ENTRY_CALL" );
                    END IF;

               END  A ;

          END  TT ;


          PROCEDURE  OUTSIDE_TASK_BODY  IS
          BEGIN

               SELECT  -- NOT A SELECTIVE_WAIT
                    TT.A ( FALSE ) ;  -- UNBORN
               OR   
                    DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
                    COMMENT( "(OUT:) ALTERNATIVE BRANCH TAKEN" );
               END SELECT;

          END  OUTSIDE_TASK_BODY ;


          PACKAGE       CREATE_OPPORTUNITY_TO_CALL           IS END;   
          PACKAGE BODY  CREATE_OPPORTUNITY_TO_CALL  IS
          BEGIN
               -- CALL THE OTHER PROC. TO FORCE EXEC. OF TIMED_E_CALL
               OUTSIDE_TASK_BODY ;
          END  CREATE_OPPORTUNITY_TO_CALL ;


     BEGIN

          TT.A ( TRUE );

     EXCEPTION

          WHEN  TASKING_ERROR  =>
               FAILED( "TASKING ERROR" );

     END  ;

     -------------------------------------------------------------------

     RESULT ;


END  C97303B ;
