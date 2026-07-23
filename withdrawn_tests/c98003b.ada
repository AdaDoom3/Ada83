-- C98003B.ADA

-- OBJECTIVE:
--     CHECK THAT A RENDEZVOUS IS EXECUTED AT THE PRIORITY OF THE
--     CALLED TASK, WHEN THE CALLED TASK HAS THE HIGHER PRIORITY.

-- HISTORY:
--     DHH 03/25/88 CREATED ORIGINAL TEST.

WITH CALENDAR; USE CALENDAR;
WITH SYSTEM; USE SYSTEM;
WITH REPORT; USE REPORT;
PROCEDURE C98003B IS

     LOW : CONSTANT INTEGER := PRIORITY'FIRST;
     HIGH : CONSTANT INTEGER := PRIORITY'LAST;
     MED : CONSTANT INTEGER := LOW/2 + HIGH/2;

     I : INTEGER := 0;

     RAN_LOW : BOOLEAN := FALSE;
     RAN_MED : BOOLEAN := FALSE;

     SUBTYPE REAL IS TIME;
     X, Y : REAL;

     FUNCTION IDENT(R : REAL) RETURN REAL IS
     BEGIN
          IF EQUAL(3,3) THEN
               RETURN R;
          ELSE
               COMMENT("EQUAL FUNCTION FAILED");
               RETURN X + DURATION'(20.0);
          END IF;
     END IDENT;

BEGIN
     TEST("C98003B", "CHECK THAT A RENDEZVOUS IS EXECUTED AT THE " &
                     "PRIORITY OF THE CALLED TASK, WHEN THE CALLED " &
                     "TASK HAS THE HIGHER PRIORITY");

     IF HIGH <= LOW THEN
          NOT_APPLICABLE("PRIORITY'LAST <= PRIORITY'FIRST");

     ELSE
          WHILE I < 5 AND NOT RAN_LOW  AND NOT RAN_MED LOOP
               DECLARE
                    TASK T IS
                         ENTRY SET(X : INTEGER);
                    END T;

                    TASK LO IS
                         PRAGMA PRIORITY(LOW);
                         ENTRY HOLD_LO;
                    END LO;

                    TASK HI IS
                         PRAGMA PRIORITY(HIGH);
                         ENTRY CALL_SET;
                    END HI;

                    TASK MEDIUM IS
                         PRAGMA PRIORITY(MED);
                         ENTRY HOLD_MEDIUM;
                    END MEDIUM;

                    TASK LO1 IS
                         PRAGMA PRIORITY(LOW);
                    END LO1;

                    TASK KILL_TIME IS
                         ENTRY D_CLOCK;
                    END KILL_TIME;

                    TASK BODY T IS
                    BEGIN
                         ACCEPT SET(X : INTEGER) DO
                              IF X = 0 THEN
                                   RAN_LOW := TRUE;
                                   COMMENT("LOW BEFORE HIGH");
                              ELSIF X = 5 THEN
                                   RAN_MED := TRUE;
                              END IF;
                         END SET;

                         ACCEPT SET(X : INTEGER) DO
                              IF MED /= LOW THEN
                                   IF X = 0 THEN
                                        RAN_LOW := TRUE;
                                        COMMENT("LOW BEFORE MEDIUM");
                                   END IF;
                              END IF;
                         END SET;

                         ACCEPT SET(X : INTEGER);
                    END T;

                    TASK BODY LO IS
                    BEGIN
                         ACCEPT HOLD_LO;
                         DELAY 0.5;  -- TODO: acats-delay-deviation: before was 5.0
                         T.SET(IDENT_INT(0));
                    END LO;

                    TASK BODY HI IS
                    BEGIN
                         ACCEPT CALL_SET DO
                              LO.HOLD_LO;
                              MEDIUM.HOLD_MEDIUM;
                              KILL_TIME.D_CLOCK;
                              DELAY 0.5;  -- TODO: acats-delay-deviation: before was 5.0
                              T.SET(IDENT_INT(9));
                         END CALL_SET;
                    END HI;

                    TASK BODY MEDIUM IS
                    BEGIN
                         ACCEPT HOLD_MEDIUM;
                         DELAY 0.5;  -- TODO: acats-delay-deviation: before was 5.0
                         T.SET(IDENT_INT(5));
                    END MEDIUM;

                    TASK BODY LO1 IS
                    BEGIN
                         HI.CALL_SET;
                    END LO1;

                    TASK BODY KILL_TIME IS
                    BEGIN
                         ACCEPT D_CLOCK;
                         DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                         Y := IDENT(CLOCK);
                         X := CLOCK + DURATION'(20.0);
                         WHILE Y <= X LOOP
                              Y := IDENT(CLOCK);
                         END LOOP;
                    END KILL_TIME;

               BEGIN
                    NULL;
               END;

               I := I + 1;
          END LOOP;

          IF RAN_LOW THEN
               FAILED("RENDEZVOUS IS NOT ALWAYS EXECUTED AT THE " &
                      "PRIORITY OF THE CALLED TASK - LOW");
          ELSIF RAN_MED THEN
               FAILED("RENDEZVOUS IS NOT ALWAYS EXECUTED AT THE " &
                      "PRIORITY OF THE CALLED TASK - MED");
          END IF;

     END IF;

     RESULT;
END C98003B;
