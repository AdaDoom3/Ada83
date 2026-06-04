-- C94005B.ADA

-- CHECK THAT IF A TASK TYPE IS DECLARED IN A LIBRARY PACKAGE, ANY
--   BLOCKS, SUBPROGRAMS, OR TASKS THAT DECLARE OBJECTS OF THAT TYPE
--   DO WAIT FOR TERMINATION OF SUCH OBJECTS.
-- SUBTESTS ARE:
--   (A)  IN A MAIN PROGRAM BLOCK.
--   (B)  IN A LIBRARY FUNCTION.
--   (C)  IN A MAIN PROGRAM TASK BODY.

-- THIS TEST CONTAINS SHARED VARIABLES AND RACE CONDITIONS.

-- JRK 10/8/81
-- SPS 11/2/82
-- SPS 11/21/82
-- JWC 11/15/85    MADE THE LIBRARY PACKAGE NAME UNIQUE, C94005B_PKG.


WITH SYSTEM; USE SYSTEM;
PACKAGE C94005B_PKG IS

     GLOBAL : INTEGER;

     TASK TYPE TT IS
          ENTRY E (I : INTEGER);
          PRAGMA PRIORITY (PRIORITY'FIRST);
     END TT;

END C94005B_PKG;


PACKAGE BODY C94005B_PKG IS

     TASK BODY TT IS
          LOCAL : INTEGER;
     BEGIN
          ACCEPT E (I : INTEGER) DO
               LOCAL := I;
          END E;
          DELAY 6.0;    -- SINCE THE PARENT UNIT HAS HIGHER PRIORITY  -- TODO: acats-delay-deviation: before was 60.0
                         -- AT THIS POINT, IT WILL RECEIVE CONTROL AND
                         -- TERMINATE IF THE ERROR IS PRESENT.
          GLOBAL := LOCAL;
     END TT;

END C94005B_PKG;


WITH REPORT; USE REPORT;
WITH C94005B_PKG; USE C94005B_PKG;
FUNCTION F RETURN INTEGER IS

     T : TT;

BEGIN

     T.E (IDENT_INT(2));
     RETURN 0;

END F;


WITH SYSTEM; USE SYSTEM;
WITH REPORT; USE REPORT;
WITH C94005B_PKG; USE C94005B_PKG;
WITH F;
PROCEDURE C94005B IS

     PRAGMA PRIORITY (PRIORITY'LAST-1);

BEGIN
     TEST ("C94005B", "CHECK THAT IF A TASK TYPE IS DECLARED IN A " &
                      "LIBRARY PACKAGE, ANY BLOCKS, SUBPROGRAMS, OR " &
                      "TASKS THAT DECLARE OBJECTS OF THAT TYPE DO " &
                      "WAIT FOR TERMINATION OF SUCH OBJECTS");

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (A)

          T : TT;

     BEGIN -- (A)

          T.E (IDENT_INT(1));

     END; -- (A)

     IF GLOBAL /= 1 THEN
          FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                  "BLOCK EXIT - (A)");
     END IF;

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (B)

          I : INTEGER;

     BEGIN -- (B)

          I := F ;

          IF GLOBAL /= 2 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "FUNCTION EXIT - (B)");
          END IF;

     END; -- (B)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (C)

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST);
          END TSK;

          TASK BODY TSK IS
               T : TT;
          BEGIN
               T.E (IDENT_INT(3));
          END TSK;

     BEGIN -- (C)

          WHILE NOT TSK'TERMINATED LOOP
               DELAY 0.1;
          END LOOP;

          IF GLOBAL /= 3 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "TASK EXIT - (C)");
          END IF;

     END; -- (C)

     --------------------------------------------------

     RESULT;
END C94005B;
