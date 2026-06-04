-- B97101H.ADA

-- OBJECTIVE:
--     CHECK THAT AT THE END OF A SELECT STATEMENT THE WORD 'TCELES'
--     RAISES AND ERROR.

-- HISTORY:
--     DWC 09/29/87  CREATED ORIGINAL TEST.

PROCEDURE B97101H IS
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

               TCELES;
          END TT;        -- ERROR: 'END SELECT' MISSING.

     BEGIN
          NULL;
     END;

     -------------------------------------------------------------------

END  B97101H;
