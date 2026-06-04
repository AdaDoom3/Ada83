-- A97106A.ADA


-- CHECK THAT A SELECTIVE_WAIT MAY HAVE MORE THAN ONE  'DELAY'  ALTER-
--    NATIVE.


-- RM 4/27/1982


WITH REPORT;
USE REPORT;
PROCEDURE  A97106A  IS


BEGIN


     TEST ( "A97106A" , "CHECK THAT A SELECTIVE_WAIT MAY HAVE" &
                        " MORE THAN ONE  'DELAY'  ALTERNATIVE" );

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
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               OR
                         ACCEPT  A ;
               OR
                         ACCEPT  A ;
               OR
                         DELAY 0.25 ;  -- MULTIPLE 'DELAY'S PERMITTED (IF  -- TODO: acats-delay-deviation: before was 2.5
               OR                     --     AND ONLY IF SINGLE 'DELAY'S
                         DELAY 0.25 ;  --     ARE PERMITTED).  -- TODO: acats-delay-deviation: before was 2.5
               OR
                         ACCEPT  A ;
               END SELECT ;

          END  TT ;

     BEGIN
          NULL ;
     END ;

     -------------------------------------------------------------------


     RESULT;


END  A97106A ;
