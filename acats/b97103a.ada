-- B97103A.ADA


-- CHECK THAT A  SELECTIVE_WAIT  STATEMENT CANNOT APPEAR OUTSIDE A
--     TASK BODY.  (PART A: PLAIN.)

-- RM 3/23/1982
-- JBG 6/16/83
-- JRK 6/24/86   CLARIFIED ERROR COMMENT.

PROCEDURE  B97103A  IS
BEGIN


     -------------------------------------------------------------------


     DECLARE


          PROCEDURE  B ;


          TASK TYPE  TT  IS
               ENTRY  A ;
          END  TT ;

          TASK BODY  TT  IS
          BEGIN
               NULL ;
          END  TT ;


          TASK TYPE  TT2  IS

               ENTRY  A ;
               ENTRY  B ;

          END  TT2 ;


          TASK BODY  TT2  IS
          BEGIN
               NULL ;
          END  TT2 ;


          PROCEDURE  B  IS
          BEGIN
               NULL ;
          END  B ;


     BEGIN


               SELECT
                    WHEN NOT FALSE =>  -- ERROR: SELECTIVE_WAIT OUTSIDE
                                       --   TASK BODY, OR MISSING
                                       --   ACCEPT_ALTERNATIVE.
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               OR
                         DELAY 0.0 ;
               OR
                    WHEN TRUE =>
                         DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
               END SELECT;


     END  ;

     -------------------------------------------------------------------


END  B97103A ;
