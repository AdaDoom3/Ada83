-- B97102H.ADA


-- CHECK THAT A  SELECT  STATEMENT USED IN A SELECTIVE WAIT MUST CONTAIN
--    AT LEAST ONE ALTERNATIVE STARTING WITH AN  ACCEPT  STATEMENT.


-- RM 3/15/1982


PROCEDURE  B97102H  IS
BEGIN


     -------------------------------------------------------------------


     DECLARE


          TASK TYPE  TT  IS
               ENTRY  A ;
          END  TT ;


          TASK BODY  TT  IS
               BUSY : BOOLEAN := FALSE ;
          BEGIN

               ACCEPT  A ;


               SELECT
                    WHEN NOT FALSE =>
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               OR
                    WHEN  BUSY =>
                         NULL ;    -- ERROR: NOT AN ALTERNATIVE.
               OR
                    WHEN TRUE =>
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               END SELECT;         


          END  TT ;


     BEGIN
          NULL ;
     END  ;

     -------------------------------------------------------------------


END  B97102H ;
