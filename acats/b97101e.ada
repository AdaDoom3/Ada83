-- B97101E.ADA

-- OBJECTIVE:
--     CHECK THAT AT THE END OF A SELECT STATEMENT THE WORD 'SELECT'
--     IS REQUIRED AFTER 'END'.

-- HISTORY:
--     RM  03/17/82
--     DAS 11/20/85  SPLIT SUCH THAT THERE ARE ERRORS IN ONLY ONE PART
--                   OF THE SELECT STATEMENT.
--     DWC 09/29/87  REVISED TEST SO THAT THE SELECT STATEMENT IS
--                   PLACED WITHIN A BLOCK STATEMENT WHICH HAS A SIMPLE
--                   NAME.  LEFT OUT THE END SELECT STATEMENT.

PROCEDURE B97101E IS
BEGIN

     -------------------------------------------------------------------

     DECLARE

          TASK TYPE  TT  IS
               ENTRY  A;
          END  TT;

          TASK BODY  TT  IS
               BUSY : BOOLEAN := FALSE;
          BEGIN

               ACCEPT  A;

               SIMPLE : BEGIN

                    SELECT
                         ACCEPT  A;
                    OR
                         DELAY 0.25;  -- TODO: acats-delay-deviation: before was 2.5
               END  SIMPLE;    -- ERROR:  END SELECT MISSING.
          END TT;

     BEGIN
          NULL;
     END;

     -------------------------------------------------------------------

END  B97101E;
