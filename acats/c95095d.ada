-- C95095D.ADA

-- CHECK THAT OVERLOADED SUBPROGRAM AND ENTRY DECLARATIONS
-- ARE PERMITTED IN WHICH THERE IS A MINIMAL
-- DIFFERENCE BETWEEN THE DECLARATIONS.

--     (D) A SUBPROGRAM IS DECLARED IN AN OUTER DECLARATIVE
--         PART, AN ENTRY IS DECLARED IN A TASK, AND THE
--         PARAMETERS ARE ORDERED DIFFERENTLY.

-- JWC 7/24/85
-- JRK 10/2/85

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C95095D IS

     PRAGMA PRIORITY (PRIORITY'FIRST);

BEGIN
     TEST ("C95095D", "SUBPROGRAM/ENTRY OVERLOADING WITH " &
                      "MINIMAL DIFFERENCES");

     --------------------------------------------------

     -- A SUBPROGRAM IS DECLARED IN AN OUTER DECLARATIVE
     -- PART, AN ENTRY IS DECLARED IN A TASK, AND THE
     -- PARAMETERS ARE ORDERED DIFFERENTLY.

     DECLARE
          S : STRING (1..2) := "12";

          I : INTEGER := 0;

          PROCEDURE E (I1 : INTEGER; I2 : IN OUT INTEGER;
                       B1 : BOOLEAN) IS
          BEGIN
               S (1) := 'A';
          END E;

          TASK T IS
               ENTRY E (B1 : BOOLEAN; I1 : INTEGER;
                        I2 : IN OUT INTEGER);
               PRAGMA PRIORITY (PRIORITY'LAST);
          END T;

          TASK BODY T IS
          BEGIN
               E (5, I, TRUE);          -- PROCEDURE CALL.
               ACCEPT E (B1 : BOOLEAN; I1 : INTEGER;
                         I2 : IN OUT INTEGER) DO
                    S (2) := 'B';
               END E;
               E (TRUE, 5, I);          -- ENTRY CALL; SELF-BLOCKING.
               -- NOTE THAT A CALL IN WHICH ALL ACTUAL PARAMETERS
               -- ARE NAMED_ASSOCIATIONS IS AMBIGUOUS.
               FAILED ("TASK DID NOT BLOCK ITSELF");
          END T;

     BEGIN

          T.E (TRUE, 5, I);

          DELAY 1.0;  -- TODO: acats-delay-deviation: before was 10.0
          ABORT T;

          IF S /= "AB" THEN
               FAILED ("PROCEDURES/ENTRIES " &
                       "DIFFERING ONLY IN PARAMETER " &
                       "TYPE ORDER CAUSED CONFUSION");
          END IF;
     END;

     --------------------------------------------------

     RESULT;
END C95095D;
