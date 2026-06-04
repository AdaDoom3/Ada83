-- C94007A.ADA

-- OBJECTIVE:
--     CHECK THAT A TASK THAT IS DECLARED IN A NON-LIBRARY PACKAGE
--     (SPECIFICATION OR BODY) DOES NOT "DEPEND" ON THE PACKAGE,
--     BUT ON THE INNERMOST ENCLOSING BLOCK, SUBPROGRAM BODY,
--     OR TASK BODY.
--     SUBTESTS ARE:
--       (A)  A SIMPLE TASK OBJECT, IN A VISIBLE PART, IN A BLOCK.
--       (B)  AN ARRAY OF TASK OBJECT, IN A PRIVATE PART, IN A FUNCTION.
--       (C)  AN ARRAY OF RECORD OF TASK OBJECT, IN A PACKAGE BODY,
--            IN A TASK BODY.

-- HISTORY:
--     JRK 10/13/81
--     SPS 11/21/82
--     DHH 09/07/88 REVISED HEADER, ADDED EXCEPTION HANDLERS ON OUTER
--                  BLOCKS, AND ADDED CASE TO INSURE THAT LEAVING A
--                  PACKAGE VIA AN EXCEPTION WOULD NOT ABORT TASKS.

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94007A IS

     TASK TYPE SYNC IS
          ENTRY ID (C : CHARACTER);
          ENTRY INNER;
          ENTRY OUTER;
          PRAGMA PRIORITY (PRIORITY'LAST-2);
     END SYNC;

     TASK BODY SYNC IS
          ID_C : CHARACTER;
     BEGIN
          ACCEPT ID (C : CHARACTER) DO
               ID_C := C;
          END ID;
          DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
          SELECT
               ACCEPT OUTER;
          OR
               DELAY 12.0;  -- TODO: acats-delay-deviation: before was 120.0
               FAILED ("PROBABLY BLOCKED - (" & ID_C & ')');
          END SELECT;
          ACCEPT INNER;
     END SYNC;

     PRAGMA PRIORITY (PRIORITY'LAST);

BEGIN
     TEST ("C94007A", "CHECK THAT A TASK THAT IS DECLARED IN A " &
                      "NON-LIBRARY PACKAGE (SPECIFICATION OR BODY) " &
                      "DOES NOT ""DEPEND"" ON THE PACKAGE, BUT ON " &
                      "THE INNERMOST ENCLOSING BLOCK, SUBPROGRAM " &
                      "BODY, OR TASK BODY");

     --------------------------------------------------

     DECLARE -- (A)

          S : SYNC;

     BEGIN -- (A)

          S.ID ('A');

          DECLARE

               PACKAGE PKG IS
                    TASK T IS
                         ENTRY E;
                         PRAGMA PRIORITY (PRIORITY'FIRST);
                    END T;
               END PKG;

               PACKAGE BODY PKG IS
                    TASK BODY T IS
                    BEGIN
                         S.INNER;  -- PROBABLE INNER BLOCK POINT.
                    END T;
               END PKG;            -- PROBABLE OUTER BLOCK POINT.

          BEGIN

               S.OUTER;

          EXCEPTION
               WHEN TASKING_ERROR => NULL;
          END;

     EXCEPTION
          WHEN OTHERS =>
               FAILED("UNEXPECTED EXCEPTION RAISED - A");
     END; -- (A)

     --------------------------------------------------

     DECLARE -- (B)

          S : SYNC;

          I : INTEGER;

          FUNCTION F RETURN INTEGER IS

               PACKAGE PKG IS
               PRIVATE
                    TASK TYPE TT IS
                         ENTRY E;
                         PRAGMA PRIORITY (PRIORITY'FIRST);
                    END TT;
                    A : ARRAY (1..1) OF TT;
               END PKG;

               PACKAGE BODY PKG IS
                    TASK BODY TT IS
                    BEGIN
                         S.INNER;  -- PROBABLE INNER BLOCK POINT.
                    END TT;
               END PKG;            -- PROBABLE OUTER BLOCK POINT.

          BEGIN -- F

               S.OUTER;
               RETURN 0;

          EXCEPTION
               WHEN TASKING_ERROR => RETURN 0;
          END F;

     BEGIN -- (B)

          S.ID ('B');
          I := F;

     EXCEPTION
          WHEN OTHERS =>
               FAILED("UNEXPECTED EXCEPTION RAISED - B");

     END; -- (B)

     --------------------------------------------------

     DECLARE -- (C)

          S : SYNC;

     BEGIN -- (C)

          S.ID ('C');

          DECLARE

               TASK TSK IS
                    PRAGMA PRIORITY (PRIORITY'LAST-1);
               END TSK;

               TASK BODY TSK IS

                    PACKAGE PKG IS
                    END PKG;

                    PACKAGE BODY PKG IS
                         TASK TYPE TT IS
                              ENTRY E;
                              PRAGMA PRIORITY (PRIORITY'FIRST);
                         END TT;

                         TYPE RT IS
                              RECORD
                                   T : TT;
                              END RECORD;

                         AR : ARRAY (1..1) OF RT;

                         TASK BODY TT IS
                         BEGIN
                              S.INNER;  -- PROBABLE INNER BLOCK POINT.
                         END TT;
                    END PKG;            -- PROBABLE OUTER BLOCK POINT.

               BEGIN -- TSK

                    S.OUTER;

               EXCEPTION
                    WHEN TASKING_ERROR => NULL;
               END TSK;

          BEGIN
               NULL;
          END;

     EXCEPTION
          WHEN OTHERS =>
               FAILED("UNEXPECTED EXCEPTION RAISED - C");
     END; -- (C)

     --------------------------------------------------

     DECLARE -- (D)

     GLOBAL : INTEGER := IDENT_INT(5);

     BEGIN -- (D)

          DECLARE

               PACKAGE PKG IS
                    TASK T IS
                         ENTRY E;
                    END T;

                    TASK T1 IS
                    END T1;
               END PKG;

               PACKAGE BODY PKG IS
                    TASK BODY T IS
                    BEGIN
                         ACCEPT E DO
                              RAISE CONSTRAINT_ERROR;
                         END E;
                    END T;

                    TASK BODY T1 IS
                    BEGIN
                         DELAY 12.0;  -- TODO: acats-delay-deviation: before was 120.0
                         GLOBAL := IDENT_INT(1);
                    END T1;

               BEGIN
                    T.E;

               END PKG;
               USE PKG;
          BEGIN
               NULL;
          END;

     EXCEPTION
          WHEN CONSTRAINT_ERROR =>
               IF GLOBAL /= IDENT_INT(1) THEN
                    FAILED("TASK NOT COMPLETED");
               END IF;

          WHEN OTHERS =>
               FAILED("UNEXPECTED EXCEPTION RAISED - D");
     END; -- (D)

     RESULT;
END C94007A;
