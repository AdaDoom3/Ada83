-- C97204A.ADA


-- CHECK THAT THE EXCEPTION  TASKING_ERROR  WILL BE RAISED IF THE CALLED
--     TASK HAS ALREADY COMPLETED ITS EXECUTION AT THE TIME OF THE
--     CONDITIONAL_ENTRY_CALL.


-- RM 5/28/82
-- SPS 11/21/82


WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE  C97204A  IS

     PRAGMA PRIORITY ( PRIORITY'FIRST );  -- LEAST URGENT
     -- THE TASK WILL HAVE HIGHER PRIORITY ( PRIORITY'LAST )

BEGIN


     -------------------------------------------------------------------


     TEST ("C97204A", "CHECK THAT THE EXCEPTION  TASKING_ERROR  WILL" &
                      " BE RAISED IF THE CALLED TASK HAS ALREADY"     &
                      " COMPLETED ITS EXECUTION AT THE TIME OF THE"   &
                      " CONDITIONAL_ENTRY_CALL"                       );

  
     DECLARE


          TASK TYPE  T_TYPE  IS

               PRAGMA PRIORITY ( PRIORITY'LAST );  -- FIRST PRIORITY

               ENTRY  E ;

          END  T_TYPE ;


          T_OBJECT1 : T_TYPE ;


          TASK BODY  T_TYPE  IS
               BUSY : BOOLEAN := FALSE ;
          BEGIN

               NULL;

          END  T_TYPE ;


     BEGIN


          FOR  I  IN  1..5  LOOP
               EXIT WHEN  T_OBJECT1'TERMINATED ;
               DELAY 1.0 ;  -- TODO: acats-delay-deviation: before was 10.0
          END LOOP;


          IF NOT  T_OBJECT1'TERMINATED  THEN
               COMMENT( "TASK NOT YET TERMINATED (AFTER 50 S.)" );
          END IF;


          BEGIN

               SELECT
                    T_OBJECT1.E ;
                    FAILED( "CALL WAS NOT DISOBEYED" );
               ELSE
                    FAILED( "'ELSE' BRANCH TAKEN INSTEAD OF TSKG_ERR" );
               END SELECT;

               FAILED( "EXCEPTION NOT RAISED" );

          EXCEPTION

               WHEN  TASKING_ERROR  =>
                    NULL ;

               WHEN  OTHERS  =>
                    FAILED(  "WRONG EXCEPTION RAISED"  );

          END ;


     END ;


     -------------------------------------------------------------------



     RESULT;


END  C97204A ;
