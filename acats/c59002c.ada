-- C59002C.ADA


-- CHECK THAT JUMPS OUT OF SELECT STATEMENTS (OTHER THAN
--    FROM INSIDE  ACCEPT  BODIES IN SELECT_ALTERNATIVES)
--    ARE POSSIBLE AND ARE CORRECTLY PERFORMED.

-- THIS TEST CONTAINS SHARED VARIABLES.


-- RM 08/15/82
-- SPS 12/13/82

WITH REPORT;
WITH SYSTEM;
USE SYSTEM;
PROCEDURE  C59002C  IS

     USE  REPORT ;

     FLOW_STRING : STRING(1..2) := "XX" ;
     INDEX       : INTEGER      :=  1 ;

     PRAGMA  PRIORITY( PRIORITY'LAST );

BEGIN

     TEST( "C59002C" , "CHECK THAT ONE CAN JUMP OUT OF SELECT STATE" &
                       "MENTS" );

     -------------------------------------------------------------------

     DECLARE

          TASK  T  IS

               PRAGMA  PRIORITY( PRIORITY'FIRST );

               ENTRY  E1 ;
               ENTRY  E2 ;
          END  T ;

          TASK BODY T IS
          BEGIN

               WHILE  E2'COUNT <= 0  LOOP
                    DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               SELECT 
                    ACCEPT  E1  DO
                         FAILED( " E1  ACCEPTED; NO ENTRY CALL (1)" );
                    END ;
               OR
                    ACCEPT  E2 ;
                    GOTO  L123 ;
                    FAILED( "'GOTO' NOT OBEYED (1)" );
               OR
                    DELAY 1.0 ;  -- TODO: acats-delay-deviation: before was 10.0
                    FAILED( "DELAY ALTERNATIVE SELECTED (1)" );
               END SELECT;

               FAILED( "WRONG DESTINATION FOR 'GOTO' (1)" );

               << L123 >>

               FLOW_STRING(INDEX) := 'A' ;
               INDEX              := INDEX + 1 ;

          END T;

     BEGIN

          T.E2 ;

     END;

     -------------------------------------------------------------------

     DECLARE

          TASK  T  IS
               ENTRY  E1 ;
               ENTRY  E2 ;
          END  T ;

          TASK BODY T IS
          BEGIN

               SELECT 
                    ACCEPT  E1  DO
                         FAILED( " E1  ACCEPTED; NO ENTRY CALL (2)" );
                    END ;
               OR
                    ACCEPT  E2  DO
                         FAILED( " E2  ACCEPTED; NO ENTRY CALL (2)" );
                    END ;
               OR
                    DELAY 1.0 ;  -- TODO: acats-delay-deviation: before was 10.0
                    GOTO  L321 ;
                    FAILED( "'GOTO' NOT OBEYED (2)" );
               END SELECT;

               FAILED( "WRONG DESTINATION FOR 'GOTO' (2)" );

               << L321 >>

               FLOW_STRING(INDEX) := 'B' ;
               INDEX              := INDEX + 1 ;

          END T;

     BEGIN

          NULL ;

     END;

     -------------------------------------------------------------------

     IF  FLOW_STRING /= "AB"  THEN
          FAILED("WRONG FLOW OF CONTROL" );
     END IF;


     RESULT ;


END  C59002C ;
