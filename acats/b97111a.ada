-- B97111A.ADA


-- CHECK THAT A  'DELAY'  ALTERNATIVE  AND AN  'ELSE'  PART
--     ARE NOT ALLOWED IN THE SAME SELECTIVE_WAIT.

-- SINCE AN  'ELSE'  PART FOLLOWED BY   A N Y   ALTERNATIVE  IS ALREADY
--     RULED OUT BY A PREVIOUS TEST, THIS TEST WILL DEAL ONLY
--     WITH A (UNIQUE)  'ELSE'  PART CONCLUDING A SELECTIVE_WAIT
--     WHICH CONTAINS A  'DELAY'  ALTERNATIVE.


-- RM 4/30/1982


PROCEDURE  B97111A  IS


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
                         DELAY 0.1 ;      -- (IN AN ACCEPT ALTERNATIVE)  -- TODO: acats-delay-deviation: before was 1.0
               OR
                         ACCEPT  A ;
               ELSE                       -- OK.
                         NULL ;
               END SELECT ;


               SELECT
                         ACCEPT  A ;
               OR  
                         DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
               OR
                         ACCEPT  A ;

               ELSE   -- ERROR: INCOMPATIBLE WITH  'DELAY'  ALTERNATIVE.

                         NULL ;
               END SELECT ;

          END  TT ;


     BEGIN
          NULL ;
     END ;

     -------------------------------------------------------------------


END  B97111A ;
