-- C93008A.ADA

-- CHECK THAT FOR A TASK CREATED BY AN OBJECT DECLARATION, EXECUTION 
-- DOES NOT PROCEED IN PARALLEL WITH ACTIVATION.

-- R.WILLIAMS 8/20/86

WITH REPORT; USE REPORT;
PROCEDURE C93008A IS

     SUBTYPE ARG IS NATURAL RANGE 0..9;
     SPYNUMB : NATURAL := 0;

     TASK T IS
          ENTRY FINIT_POS (DIGT : IN ARG);
     END T;

     TASK BODY T IS
     BEGIN
          LOOP
               SELECT
                    ACCEPT FINIT_POS (DIGT : IN ARG) DO
                         SPYNUMB := 10*SPYNUMB+DIGT;
                    END FINIT_POS;
               OR
                    TERMINATE;
               END SELECT;          
          END LOOP;
     END T;

BEGIN

     TEST ("C93008A", "CHECK THAT EXECUTION DOES NOT PROCEED IN  " &
                      "PARALLEL WITH ACTIVATION OF A TASK CREATED " &
                      "BY AN OBJECT DECLARATION");

BLOCK:
     DECLARE

          TASK TYPE TT1;

          TASK TT2;

          T1 : TT1;

          TASK BODY TT1 IS
               PACKAGE DUMMY IS
               END DUMMY;

               PACKAGE BODY DUMMY IS
               BEGIN
                    DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0
                    T.FINIT_POS(1);
               END DUMMY;
          BEGIN
               NULL;
          END TT1;

          TASK BODY TT2 IS
               PACKAGE DUMMY IS
               END DUMMY;

               PACKAGE BODY DUMMY IS
               BEGIN
                    DELAY 0.2;  -- TODO: acats-delay-deviation: before was 2.0
                    T.FINIT_POS(2);
               END DUMMY;
          BEGIN
               NULL;
          END TT2;
          

     BEGIN               -- TASKS ACTIVATED NOW.

          IF SPYNUMB = 12 OR SPYNUMB = 21 THEN
               NULL;
          ELSE
               FAILED ("TASKS NOT ACTIVATED PROPERLY - SPYNUMB HAS " &
                       "ACTUAL VALUE OF: " & INTEGER'IMAGE(SPYNUMB));
          END IF;
     END BLOCK;

     RESULT;

END C93008A;
