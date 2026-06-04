-- C94001B.ADA

-- CHECK THAT A UNIT WITH DEPENDENT TASKS CREATED BY AN OBJECT
-- DECLARATION OF LIMITED PRIVATE TYPE IS NOT TERMINATED UNTIL ALL
-- DEPENDENT TASKS BECOME TERMINATED.
-- SUBTESTS ARE:
--   (A, B)  A SIMPLE TASK OBJECT, IN A BLOCK.
--   (C, D)  AN ARRAY OF TASK OBJECT, IN A FUNCTION.
--   (E, F)  AN ARRAY OF RECORD OF TASK OBJECT, IN A TASK BODY.

-- THIS TEST CONTAINS SHARED VARIABLES AND RACE CONDITIONS.

-- TBN  8/22/86

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94001B IS

     PACKAGE P IS
          MY_EXCEPTION : EXCEPTION;
          GLOBAL : INTEGER;
          TYPE TT IS LIMITED PRIVATE;
          PROCEDURE CALL_ENTRY (A : TT; B : INTEGER);
     PRIVATE
          TASK TYPE TT IS
               ENTRY E (I : INTEGER);
               PRAGMA PRIORITY (PRIORITY'FIRST);
          END TT;
     END P;

     PACKAGE BODY P IS

          PROCEDURE CALL_ENTRY (A : TT; B : INTEGER) IS
          BEGIN
               A.E (B);
          END CALL_ENTRY;

          TASK BODY TT IS
               LOCAL : INTEGER;
          BEGIN
               ACCEPT E (I : INTEGER) DO
                    LOCAL := I;
               END E;
               DELAY 3.0;    -- SINCE THE PARENT UNIT HAS HIGHER  -- TODO: acats-delay-deviation: before was 30.0
                              -- PRIORITY AT THIS POINT, IT WILL
                              -- RECEIVE CONTROL AND TERMINATE IF
                              -- THE ERROR IS PRESENT.
               GLOBAL := LOCAL;
          END TT;
     END P;

     USE P;

     PRAGMA PRIORITY (PRIORITY'LAST-1);

BEGIN
     TEST ("C94001B", "CHECK THAT A UNIT WITH DEPENDENT TASKS " &
                      "CREATED BY AN OBJECT DECLARATION OF LIMITED " &
                      "PRIVATE TYPE IS NOT TERMINATED UNTIL ALL " &
                      "DEPENDENT TASKS BECOME TERMINATED");

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (A)

          T : TT;

     BEGIN -- (A)

          CALL_ENTRY (T, IDENT_INT(1));

     END; -- (A)

     IF GLOBAL /= 1 THEN
          FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                  "BLOCK EXIT - 1");
     END IF;

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     BEGIN -- (B)
          DECLARE
               T : TT;
          BEGIN
               CALL_ENTRY (T, IDENT_INT(2));
               RAISE MY_EXCEPTION;
          END;

          FAILED ("MY_EXCEPTION WAS NOT RAISED - 2");
     EXCEPTION
          WHEN MY_EXCEPTION =>
               IF GLOBAL /= 2 THEN
                    FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                            "BLOCK EXIT - 2");
               END IF;
          WHEN OTHERS =>
               FAILED ("UNEXPECTED EXCEPTION - 2");
     END; -- (B)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (C)

          I : INTEGER;

          FUNCTION F RETURN INTEGER IS
               A : ARRAY (1..1) OF TT;
          BEGIN
               CALL_ENTRY (A(1), IDENT_INT(3));
               RETURN 0;
          END F;

     BEGIN -- (C)

          I := F;

          IF GLOBAL /= 3 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "FUNCTION EXIT - 3");
          END IF;

     END; -- (C)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (D)

          I : INTEGER;

          FUNCTION F RETURN INTEGER IS
               A : ARRAY (1..1) OF TT;
          BEGIN
               CALL_ENTRY (A(1), IDENT_INT(4));
               IF EQUAL (3, 3) THEN
                    RAISE MY_EXCEPTION;
               END IF;
               RETURN 0;
          END F;

     BEGIN -- (D)
          I := F;
          FAILED ("MY_EXCEPTION WAS NOT RAISED - 4");
     EXCEPTION
          WHEN MY_EXCEPTION =>
               IF GLOBAL /= 4 THEN
                    FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                            "FUNCTION EXIT - 4");
               END IF;
          WHEN OTHERS =>
               FAILED ("UNEXPECTED EXCEPTION - 4");
     END; -- (D)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (E)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 60 * 60;     -- ONE HOUR DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST);
          END TSK;

          TASK BODY TSK IS
               TYPE RT IS
                    RECORD
                         T : TT;
                    END RECORD;
               AR : ARRAY (1..1) OF RT;
          BEGIN
               CALL_ENTRY (AR(1).T, IDENT_INT(5));
          END TSK;

     BEGIN -- (E)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED WITHIN ONE " &
                       "HOUR - 5");
          ELSIF GLOBAL /= 5 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "TASK EXIT - 5");
          END IF;

     END; -- (E)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (F)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 60 * 60;     -- ONE HOUR DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST);
          END TSK;

          TASK BODY TSK IS
               TYPE RT IS
                    RECORD
                         T : TT;
                    END RECORD;
               AR : ARRAY (1..1) OF RT;
          BEGIN
               CALL_ENTRY (AR(1).T, IDENT_INT(6));
               IF EQUAL (3, 3) THEN
                    RAISE MY_EXCEPTION;
               END IF;
               FAILED ("EXCEPTION WAS NOT RAISED - 6");
          END TSK;

     BEGIN -- (F)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED WITHIN ONE " &
                       "HOUR - 6");
          ELSIF GLOBAL /= 6 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "TASK EXIT - 6");
          END IF;

     END; -- (F)

     RESULT;
END C94001B;
