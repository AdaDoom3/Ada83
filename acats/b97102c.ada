-- B97102C.ADA

-- OBJECTIVE:
--     CHECK THAT A  SELECT  STATEMENT USED IN A SELECTIVE WAIT MUST
--     CONTAIN AT LEAST ONE ALTERNATIVE STARTING WITH AN ACCEPT
--     STATEMENT.

-- HISTORY:
--     RM  03/15/82
--     JBG 06/16/83
--     DWC 09/29/87  REPLACED THE OPEN TERMINATE ALTERNATIVE WITH AN
--                   OPEN DELAY ALTERNATIVE.

PROCEDURE B97102C IS
BEGIN

     -------------------------------------------------------------------

     DECLARE

          TASK TYPE TT IS
               ENTRY A;
          END TT;


          TASK BODY TT IS
               BUSY : BOOLEAN := FALSE;
          BEGIN

               ACCEPT A;

               SELECT
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               OR
                    WHEN NOT BUSY =>
                         DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END SELECT;         -- ERROR: ACCEPT ALTERNATIVE MISSING.

          END TT;

     BEGIN
          NULL;
     END;

     -------------------------------------------------------------------

END B97102C;
