-- B97110A.ADA


-- CHECK THAT A  'TERMINATE'  ALTERNATIVE AND A  'DELAY'  ALTERNATIVE
--     (AN ALTERNATIVE STARTING WITH A  'DELAY'  STATEMENT)
--     ARE NOT ALLOWED IN THE SAME SELECTIVE_WAIT.


-- RM 4/29/1982


PROCEDURE  B97110A  IS


BEGIN


     -------------------------------------------------------------------


     DECLARE


          TASK TYPE  TT  IS
               ENTRY  A ;
          END  TT ;


          TASK BODY  TT  IS
               DUMMY : BOOLEAN := FALSE ;
          BEGIN

               SELECT
                         ACCEPT  A ;
               OR  
                         TERMINATE ;
               OR
                         ACCEPT  A ;
                         DELAY 0.1 ;-- OK (IN AN  'ACCEPT'  ALTERNATIVE)  -- TODO: acats-delay-deviation: before was 1.0
               END SELECT ;


               SELECT
                         ACCEPT  A ;
               OR  
                         TERMINATE ;
               OR
                         DELAY 0.1 ;-- ERROR: INCOMPATIBLE WITH   -- TODO: acats-delay-deviation: before was 1.0
                                    --              'TERMINATE' .
                         NULL ;
               END SELECT ;

          END  TT ;


     BEGIN
          NULL ;
     END ;

     -------------------------------------------------------------------


END  B97110A ;
