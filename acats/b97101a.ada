-- B97101A.ADA

-- OBJECTIVE:
--     CHECK THAT AT THE END OF A SELECT STATEMENT THE WORD 'SELECT'
--     IS REQUIRED AFTER 'END'.

-- HISTORY:
--     RM  03/17/82
--     DAS 11/20/85  SPLIT FROM B97101E TO CONTAIN AN ERROR IN ONLY ONE
--                   PART OF THE SELECT STATEMENT.
--     DWC 09/29/87  DELETED THE OR STATEMENT OF THE SELECT STATEMENT
--                   AND INCLUDED AN ELSE STATEMENT.  DELETED
--                   END NEXT_ALTERN; AND END SELECT; STATEMENTS.

PROCEDURE B97101A IS
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
                    ACCEPT  A;
               ELSE
                    DELAY 0.25;  -- TODO: acats-delay-deviation: before was 2.5

          END TT;    -- ERROR: 'END SELECT' MISSING;

     BEGIN
          NULL;
     END;

     -------------------------------------------------------------------

END  B97101A;
