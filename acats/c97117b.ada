-- C97117B.ADA

-- CHECK THAT AN ELSE PART IS EXECUTED IF ALL ALTERNATIVES ARE CLOSED OR
-- IF THERE ARE NO TASKS QUEUED FOR OPEN ALTERNATIVES.

-- WRG 7/10/86

WITH REPORT; USE REPORT;
PROCEDURE C97117B IS

BEGIN

     TEST ("C97117B", "CHECK THAT AN ELSE PART IS EXECUTED IF ALL " &
                      "ALTERNATIVES ARE CLOSED OR IF THERE ARE NO " &
                      "TASKS QUEUED FOR OPEN ALTERNATIVES");

     DECLARE

          TASK T IS
               ENTRY E;
               ENTRY NO_GO;
          END T;

          TASK BODY T IS
          BEGIN
               -- ENSURE THAT NO_GO HAS BEEN CALLED BEFORE PROCEEDING:
               WHILE NO_GO'COUNT = 0 LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               SELECT
                    WHEN IDENT_BOOL (FALSE) =>
                         ACCEPT E;
                         FAILED ("CLOSED ACCEPT ALTERNATIVE TAKEN " &
                                 "FOR NONEXISTENT ENTRY CALL - 1");
               OR
                    WHEN IDENT_BOOL (FALSE) =>
                         ACCEPT NO_GO;
                         FAILED ("CLOSED ALTERNATIVE TAKEN - 1");
               ELSE
                    COMMENT ("ELSE PART EXECUTED - 1");
               END SELECT;

               SELECT
                    ACCEPT E;
                    FAILED ("ACCEPTED NONEXISTENT ENTRY CALL - 2");
               OR WHEN IDENT_BOOL (FALSE) =>
                    ACCEPT NO_GO;
                    FAILED ("CLOSED ALTERNATIVE TAKEN - 2");
               ELSE
                    COMMENT ("ELSE PART EXECUTED - 2");
               END SELECT;

               ACCEPT NO_GO;
          END T;

     BEGIN

          T.NO_GO;

     END;

     RESULT;

END C97117B;
