-- B97102D.ADA


-- CHECK THAT A  SELECT  STATEMENT USED IN A SELECTIVE WAIT MUST CONTAIN
--    AT LEAST ONE ALTERNATIVE STARTING WITH AN  ACCEPT  STATEMENT.


-- RM 3/15/1982
-- JBG 6/16/83

PROCEDURE  B97102D  IS
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
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               OR
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               END SELECT;         -- ERROR: ACCEPT ALTERNATIVE MISSING.


          END  TT ;


     BEGIN
          NULL ;
     END  ;

     -------------------------------------------------------------------


END  B97102D ;
