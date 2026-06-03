-- C9A005A.ADA


-- CHECK THAT AFTER THE ABORT STATEMENT, THE NAMED TASK AND
--     ALL DEPENDENT TASKS  ARE NOT CALLABLE.


-- RM 5/21/82
-- RM 7/02/82
-- SPS 11/21/82
-- JBG 2/27/84
-- JBG 3/8/84

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE  C9A005A  IS

      TASK_NOT_ABORTED : BOOLEAN := FALSE; 
      TEST_VALID       : BOOLEAN := TRUE ; 

BEGIN


     -------------------------------------------------------------------


     TEST ("C9A005A", "CHECK THAT AFTER THE ABORT STATEMENT, THE NAMED"&
                      " TASK AND ALL DEPENDENT TASKS ARE NOT CALLABLE");

  
     DECLARE


          TASK  REGISTER  IS

               PRAGMA PRIORITY ( PRIORITY'FIRST );  -- LEAST URGENT
               -- OTHER TASKS HAVE HIGHEST PRIORITY ( PRIORITY'LAST )

               ENTRY  BIRTHS_AND_DEATHS; 

          END  REGISTER; 


          TASK BODY  REGISTER  IS


               TASK TYPE  SECONDARY  IS

                    PRAGMA PRIORITY ( PRIORITY'LAST ); -- MOST URGENT
 
                    ENTRY  WAIT_INDEFINITELY; 

               END  SECONDARY; 


               TASK TYPE  T_TYPE  IS

                    PRAGMA PRIORITY ( PRIORITY'LAST ); -- MOST URGENT

                    ENTRY  E; 

               END  T_TYPE; 


               TYPE  T_ARRAY_TYPE  IS  ARRAY(1..3) OF T_TYPE; 


               T_OBJECT : T_ARRAY_TYPE; 


               TASK BODY  SECONDARY  IS
               BEGIN
                    BIRTHS_AND_DEATHS; 
                    TASK_NOT_ABORTED  :=  TRUE; 
               END  SECONDARY; 


               TASK BODY  T_TYPE  IS

                    BUSY : BOOLEAN := FALSE; 

                    TYPE  ACCESS_TO_TASK  IS  ACCESS SECONDARY; 


                    TASK  INNER_TASK  IS

                         PRAGMA PRIORITY (PRIORITY'LAST); -- MOST URGENT
                         ENTRY  WAIT_INDEFINITELY; 

                    END  INNER_TASK; 


                    TASK BODY  INNER_TASK  IS
                    BEGIN
                         BIRTHS_AND_DEATHS; 
                         TASK_NOT_ABORTED  :=  TRUE; 
                    END  INNER_TASK; 


               BEGIN


                    DECLARE
                         DEPENDENT_BY_ACCESS   :  ACCESS_TO_TASK  :=
                                                  NEW  SECONDARY ; 
                    BEGIN
                         NULL;
                    END; 


                    BIRTHS_AND_DEATHS; 
                                     -- DURING THIS SUSPENSION
                                     --     MOST OF THE TASKS
                                     --     ARE ABORTED   (FIRST
                                     --     TASK #1    -- T_OBJECT(1) --
                                     --     THEN  #2  AND  #3 ).

                    TASK_NOT_ABORTED := TRUE; 

               END  T_TYPE; 


          BEGIN

               DECLARE
                    OLD_COUNT : INTEGER := 0; 
               BEGIN

                    FOR  I  IN  1..9  LOOP
                         EXIT WHEN  BIRTHS_AND_DEATHS'COUNT = 9; 
                         DELAY 1.0;   -- TODO: acats-delay-deviation: before was 10.0
                    END LOOP;

                    OLD_COUNT := BIRTHS_AND_DEATHS'COUNT; 

                    IF  OLD_COUNT = 9  THEN

                         ABORT T_OBJECT( IDENT_INT(1) ); -- (NON-STATIC)

                         -- CHECK THAT  #1  WAS ABORTED  -  2 WAYS:

                         IF T_OBJECT(1)'CALLABLE  THEN
                              FAILED( "T_OBJECT(1)'CALLABLE = TRUE" );
                         END IF;

                         BEGIN
                              T_OBJECT(1).E; 
                              FAILED( "T_OBJECT(1).E  DID NOT RAISE" &
                                                   "  TASKING_ERROR" );
                         EXCEPTION

                              WHEN TASKING_ERROR  =>
                                   NULL;

                              WHEN OTHERS  =>
                                   FAILED( "OTHER EXCEPTION RAISED" );

                         END; 

                         -- CHECK THAT  #1  AND ITS DEPENDENTS  WERE
                         --     ABORTED:

                         IF  OLD_COUNT - BIRTHS_AND_DEATHS'COUNT /= 3
                         THEN
                              FAILED( "BROOD#1 NOT REMOVED FROM QUEUE");
                         END IF;

                         ABORT  T_OBJECT(2) , T_OBJECT(3); 

                         -- CHECK THAT  #2  AND ITS DEPENDENTS  WERE
                         --     ABORTED
                         -- CHECK THAT  #3  AND ITS DEPENDENTS  WERE
                         --     ABORTED:

                         IF  BIRTHS_AND_DEATHS'COUNT /= 0  THEN
                              FAILED( "SOME TASKS STILL QUEUED" );
                         END IF;

                    ELSE

                         COMMENT( "LINEUP NOT COMPLETE (AFTER 50 S.)" );
                         TEST_VALID  :=  FALSE; 

                    END IF;

               END; 


               WHILE  BIRTHS_AND_DEATHS'COUNT > 0  LOOP
                    ACCEPT  BIRTHS_AND_DEATHS; 
               END LOOP;


          END  REGISTER; 


     BEGIN

          NULL;

     END; 


     -------------------------------------------------------------------


     IF  TEST_VALID  AND  TASK_NOT_ABORTED  THEN
          FAILED( "SOME TASKS NOT ABORTED" );
     END IF;


     RESULT;


END  C9A005A; 
