-- C96001A.ADA

-- OBJECTIVE:
--      CHECK THAT A DELAY STATEMENT DELAYS EXECUTION FOR AT LEAST THE
--      SPECIFIED TIME. SPECIFICALLY,
--        (A) POSITIVE DELAY ARGUMENT.
--        (B) NEGATIVE DELAY ARGUMENT.
--        (C) ZERO DELAY ARGUMENT.
--        (D) DURATION'SMALL DELAY ARGUMENT.
--        (E) EXPRESSION OF TYPE DURATION AS DELAY ARGUMENT.

-- HISTORY:
--     CPP  8/14/84  CREATED ORIGINAL TEST.
--     RJW 11/13/87  ADDED CODE WHICH ALLOWS TEST TO REPORT "PASSED"
--                   IF TICK > DURATION'SMALL.

WITH CALENDAR;  USE CALENDAR;
WITH SYSTEM;    USE SYSTEM;
WITH REPORT;    USE REPORT;
PROCEDURE C96001A IS

     SUBTYPE INT IS INTEGER RANGE 0 .. 20_000;

BEGIN
     TEST ("C96001A", "CHECK THAT DELAY STATEMENT DELAYS " &
           "EXECUTION FOR AT LEAST THE SPECIFIED TIME");

     ---------------------------------------------

     DECLARE   -- (A)
          X : DURATION := 0.5;  -- TODO: acats-delay-deviation: before was 5.0
          OLD_TIME : TIME;
          LAPSE : DURATION;
     BEGIN     -- (A)
          LOOP
               OLD_TIME := CLOCK;
               DELAY X;
               LAPSE := CLOCK - OLD_TIME;
               EXIT;
          END LOOP;
          IF LAPSE < X THEN
               FAILED ("DELAY DID NOT LAPSE AT LEAST 5.0 " &
                       "SECONDS - (A)");
          END IF;
     EXCEPTION
          WHEN OTHERS =>
               FAILED ("EXCEPTION RAISED - (A)");
     END;

     ---------------------------------------------

     DECLARE   -- (B)
          OLD_TIME : TIME;
          LAPSE : DURATION;
     BEGIN     -- (B)
          LOOP
               OLD_TIME := CLOCK;
               DELAY -5.0;
               LAPSE := CLOCK - OLD_TIME;
               EXIT;
          END LOOP;
          COMMENT ("(B) - NEGATIVE DELAY LAPSED FOR " &
                   INT'IMAGE (INT (LAPSE * 1_000)) & " MILLISECONDS");
     EXCEPTION
          WHEN OTHERS =>
               FAILED ("EXCEPTION RAISED - (B)");
     END;

     ---------------------------------------------

     DECLARE   -- (C)
          X : DURATION := 0.0;
          OLD_TIME : TIME;
          LAPSE : DURATION;
     BEGIN     -- (C)
          LOOP
               OLD_TIME := CLOCK;
               DELAY X;
               LAPSE := CLOCK - OLD_TIME;
               EXIT;
          END LOOP;
          COMMENT ("(C) - ZERO DELAY LAPSED FOR " &
                   INT'IMAGE (INT (LAPSE * 1_000)) & " MILLISECONDS");
     EXCEPTION
          WHEN OTHERS =>
               FAILED ("EXCEPTION RAISED - (C)");
     END;

     ---------------------------------------------

     DECLARE   -- (D)
          X : DURATION := DURATION'SMALL;
          OLD_TIME : TIME;
          LAPSE : DURATION;
     BEGIN     -- (D)
          LOOP
               OLD_TIME := CLOCK;
               DELAY X;
               LAPSE := CLOCK - OLD_TIME;
               EXIT;
          END LOOP;
          IF LAPSE < X THEN
               IF TICK < DURATION'SMALL THEN
                    FAILED ("DELAY DID NOT LAPSE AT LEAST " &
                            "DURATION'SMALL SECONDS - (D)");
               ELSE
                    COMMENT ("TICK > DURATION'SMALL SO DELAY IN " &
                             "'(D)' IS NOT MEASURABLE");
               END IF;
          END IF;
     EXCEPTION
          WHEN OTHERS =>
               FAILED ("EXCEPTION RAISED - (D)");
     END;

     ---------------------------------------------

     DECLARE   -- (E)
          INC1 : DURATION := 0.2;  -- TODO: acats-delay-deviation: before was 2.0
          INC2 : DURATION := 0.3;  -- TODO: acats-delay-deviation: before was 3.0
          OLD_TIME : TIME;
          LAPSE : DURATION;
     BEGIN     -- (E)
          LOOP
               OLD_TIME := CLOCK;
               DELAY INC1 + INC2;
               LAPSE := CLOCK - OLD_TIME;
               EXIT;
          END LOOP;
          IF LAPSE < (INC1 + INC2) THEN
               FAILED ("DELAY DID NOT LAPSE AT LEAST " &
                       "INC1 + INC2 SECONDS - (E)");
          END IF;
     EXCEPTION
          WHEN OTHERS =>
               FAILED ("EXCEPTION RAISED - (E)");
     END;

     RESULT;
END C96001A;
