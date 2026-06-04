-- C9A003A.ADA


-- CHECK THAT ABORTING A TERMINATED TASK DOES NOT CAUSE EXCEPTIONS.


-- RM 5/21/82
-- SPS 11/21/82


WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE  C9A003A  IS

     PRAGMA PRIORITY ( PRIORITY'FIRST );  -- LEAST URGENT
     -- THE TASK WILL HAVE HIGHER PRIORITY ( PRIORITY'LAST )

BEGIN


     -------------------------------------------------------------------


     TEST ("C9A003A", "CHECK THAT  ABORTING A TERMINATED TASK" &
                      "  DOES NOT CAUSE EXCEPTIONS"   );

  
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


          IF NOT  T_OBJECT1'TERMINATED  THEN
               DELAY  2.0 ;  -- TODO: acats-delay-deviation: before was 20.0
          END IF;

          IF NOT  T_OBJECT1'TERMINATED  THEN
               COMMENT( "TASK NOT YET TERMINATED (AFTER 20 S.)" );
          END IF;


          BEGIN
               ABORT T_OBJECT1 ;
          EXCEPTION

               WHEN  OTHERS  =>
                    FAILED(  "EXCEPTION RAISED (WHEN ABORTING A" &
                             "  TERMINATED TASK)"  );

          END ;


     END ;


     -------------------------------------------------------------------



     RESULT;


END  C9A003A ;
