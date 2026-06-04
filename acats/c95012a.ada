-- C95012A.ADA

-- CHECK THAT A CALL TO AN ENTRY OF A TASK THAT HAS NOT BEEN ACTIVATED
--   DOES NOT RAISE EXCEPTIONS.

-- THIS TEST CONTAINS RACE CONDITIONS.

-- JRK 11/6/81
-- SPS 11/21/82

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C95012A IS

     I : INTEGER := 0;

     PRAGMA PRIORITY (PRIORITY'FIRST);

BEGIN
     TEST ("C95012A", "CHECK THAT A CALL TO AN ENTRY OF A TASK " &
                      "THAT HAS NOT BEEN ACTIVATED DOES NOT " &
                      "RAISE EXCEPTIONS");

     DECLARE

          TASK T1 IS
               ENTRY E1 (I : OUT INTEGER);
               PRAGMA PRIORITY (PRIORITY'FIRST);
          END T1;

          TASK TYPE T2T IS
               ENTRY E2 (I : OUT INTEGER);
               PRAGMA PRIORITY (PRIORITY'LAST);
          END T2T;

          TYPE AT2T IS ACCESS T2T;
          AT2 : AT2T;

          TASK BODY T1 IS
          BEGIN
               ACCEPT E1 (I : OUT INTEGER) DO
                    I := IDENT_INT (1);
               END E1;
          END T1;

          TASK BODY T2T IS
               J : INTEGER := 0;
          BEGIN
               BEGIN
                    T1.E1 (J);
               EXCEPTION
                    WHEN OTHERS =>
                         J := -1;
               END;
               ACCEPT E2 (I : OUT INTEGER) DO
                    I := J;
               END E2;
          END T2T;

          PACKAGE PKG IS
          END PKG;

          PACKAGE BODY PKG IS
          BEGIN
               AT2 := NEW T2T;
               DELAY 6.0;  -- TODO: acats-delay-deviation: before was 60.0
          END PKG;

     BEGIN

          AT2.ALL.E2 (I);

          IF I = -1 THEN 
               FAILED ("EXCEPTION RAISED");
               T1.E1 (I);
          END IF;

          IF I /= 1 THEN
               FAILED ("WRONG VALUE PASSED");
          END IF;

     END;

     RESULT;
END C95012A;
