-- C97117C.ADA

-- CHECK THAT AN ELSE PART IS NOT EXECUTED IF A TASK IS QUEUED AT AN
-- OPEN ALTERNATIVE.

-- WRG 7/10/86

WITH REPORT; USE REPORT;
PROCEDURE C97117C IS

BEGIN

     TEST ("C97117C", "CHECK THAT AN ELSE PART IS NOT EXECUTED IF A " &
                      "TASK IS QUEUED AT AN OPEN ALTERNATIVE");

     DECLARE

          TASK T IS
               ENTRY E;
               ENTRY NO_GO;
          END T;

          TASK BODY T IS
          BEGIN
               --ENSURE THAT E HAS BEEN CALLED BEFORE PROCEEDING:
               WHILE E'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               SELECT
                    ACCEPT NO_GO;
                    FAILED ("ACCEPTED NONEXISTENT ENTRY CALL");
               OR WHEN IDENT_BOOL (TRUE)  =>
                    ACCEPT E;
               OR WHEN IDENT_BOOL (FALSE) =>
                    ACCEPT E;
                    FAILED ("CLOSED ALTERNATIVE TAKEN");
               ELSE
                    FAILED ("ELSE PART EXECUTED");
               END SELECT;
          END T;

     BEGIN

          T.E;

     END;

     RESULT;

END C97117C;
