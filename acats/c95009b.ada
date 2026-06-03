-- C95009B.ADA

-- CHECK THAT A TASK CAN CALL ITS OWN ENTRIES.

-- THIS TEST CONTAINS SHARED VARIABLES.

-- JRK 11/5/81
-- JWC 6/28/85   RENAMED TO -AB

WITH REPORT; USE REPORT;
PROCEDURE C95009B IS

     V : INTEGER := 0;

BEGIN
     TEST ("C95009B", "CHECK THAT A TASK CAN CALL ITS OWN ENTRIES");

     DECLARE

          TASK T IS
               ENTRY E;
          END T;

          TASK BODY T IS
          BEGIN
               V := 1;
               SELECT
                    T.E;
                    V := 2;
               OR
                    DELAY 12.0;  -- TODO: acats-delay-deviation: before was 120.0
                    V := 3;
               END SELECT;
          END T;

     BEGIN

          NULL;

     END;

     IF V /= 3 THEN
          FAILED ("TIMED ENTRY CALL NOT WORKING");
     END IF;

     RESULT;
END C95009B;
