-- C97201B.ADA

-- CHECK THAT A CONDITIONAL ENTRY CALL IS NOT ACCEPTED IF THERE IS
-- ANOTHER TASK QUEUED FOR THE ENTRY.

-- WRG 7/11/86

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C97201B IS

     PRAGMA PRIORITY (PRIORITY'FIRST);  -- LOWEST PRIORITY.

BEGIN

     TEST ("C97201B", "CHECK THAT A CONDITIONAL ENTRY CALL IS NOT " &
                      "ACCEPTED IF THERE IS ANOTHER TASK QUEUED " &
                      "FOR THE ENTRY");

     DECLARE

          TASK T IS
               PRAGMA PRIORITY (PRIORITY'LAST);  -- HIGHEST PRIORITY.
               ENTRY E;
               ENTRY SYNCH;
               ENTRY DONE;
          END T;

          TASK BODY T IS
          BEGIN
               -- ENSURE THAT E HAS BEEN CALLED BEFORE PROCEEDING:
               WHILE E'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               ACCEPT SYNCH;

               SELECT
                    WHEN IDENT_BOOL (FALSE) =>
                         ACCEPT E;
                         FAILED ("CLOSED ALTERNATIVE TAKEN");
               OR
                    ACCEPT DONE DO
                         IF E'COUNT /= 1 THEN
                              FAILED (NATURAL'IMAGE(E'COUNT) &
                                      " CALLS WERE QUEUED FOR ENTRY " &
                                      "E OF TASK T");
                         END IF;
                    END DONE;
               OR
                    DELAY 100.0;  -- TODO: acats-delay-deviation: before was 1000.0
                    FAILED ("DELAY EXPIRED; E'COUNT =" &
                            NATURAL'IMAGE(E'COUNT) );
               END SELECT;

               WHILE E'COUNT > 0 LOOP
                    ACCEPT E;
               END LOOP;
          END T;

          TASK AGENT;

          TASK BODY AGENT IS
          BEGIN
               T.E;
          END AGENT;

     BEGIN

          T.SYNCH;

          DELAY 1.0;  -- TODO: acats-delay-deviation: before was 10.0

          SELECT
               T.E;
               FAILED ("CONDITIONAL ENTRY CALL ACCEPTED" );
          ELSE
               COMMENT ("ELSE PART EXECUTED");
               T.DONE;
          END SELECT;

     END;

     RESULT;

END C97201B;
