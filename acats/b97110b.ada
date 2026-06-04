-- B97110B.ADA


-- CHECK THAT A  'TERMINATE'  ALTERNATIVE AND A  'DELAY'  ALTERNATIVE
--     (AN ALTERNATIVE STARTING WITH A  'DELAY'  STATEMENT)
--     ARE NOT ALLOWED IN THE SAME SELECTIVE_WAIT.


-- RM 4/29/1982


PROCEDURE  B97110B  IS


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
                         ACCEPT  A ;
                         DELAY 0.1 ;-- (IN AN  'ACCEPT'  ALTERNATIVE)  -- TODO: acats-delay-deviation: before was 1.0
               OR
                         TERMINATE ;-- OK.
               END SELECT ;


               SELECT
                         ACCEPT  A ;
               OR  
                         DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
                         NULL ;
               OR
                         TERMINATE ;-- ERROR: INCOMPATIBLE WITH 
                                    --        A  'DELAY'  ALTERNATIVE.
               END SELECT ;


          END  TT ;


     BEGIN
          NULL ;
     END ;

     -------------------------------------------------------------------


END  B97110B ;
