-- C97118A.ADA

-- CHECK THAT A CALL TO A CLOSED ALTERNATIVE OF A SELECTIVE WAIT IS NOT
-- ACCEPTED.

-- WRG 7/11/86

WITH REPORT; USE REPORT;
PROCEDURE C97118A IS

BEGIN

     TEST ("C97118A", "CHECK THAT A CALL TO A CLOSED ALTERNATIVE OF " &
                      "A SELECTIVE WAIT IS NOT ACCEPTED");

     DECLARE

          TASK T IS
               ENTRY E;
          END T;

          TASK BODY T IS
          BEGIN
               -- ENSURE THAT E HAS BEEN CALLED BEFORE PROCEEDING:
               WHILE E'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               SELECT
                    WHEN IDENT_BOOL (FALSE) =>
                         ACCEPT E;
                         FAILED ("ACCEPTED CALL TO CLOSED ALTERNATIVE");
                    ELSE
                         NULL;
               END SELECT;

               IF E'COUNT = 1 THEN
                    ACCEPT E;
               END IF;
          END T;

     BEGIN

          T.E;

     END;

     RESULT;

END C97118A;
