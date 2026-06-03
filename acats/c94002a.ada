-- C94002A.ADA

-- CHECK THAT A UNIT WITH DEPENDENT TASKS CREATED BY (LOCAL)
--   ALLOCATORS DOES NOT TERMINATE UNTIL ALL DEPENDENT TASKS ARE
--   TERMINATED.
-- SUBTESTS ARE:
--   (A, B)  A SIMPLE TASK ALLOCATOR, IN A BLOCK.
--   (C, D)  A RECORD OF TASK ALLOCATOR, IN A FUNCTION.
--   (E, F)  A RECORD OF ARRAY OF TASK ALLOCATOR, IN A TASK BODY.

-- THIS TEST CONTAINS SHARED VARIABLES AND RACE CONDITIONS.

-- JRK 10/2/81
-- SPS 11/2/82
-- SPS 11/21/82
-- JRK 11/29/82
-- TBN  8/25/86     REDUCED DELAYS; ADDED LIMITED PRIVATE TYPES; 
--                  INCLUDED EXITS BY RAISING AN EXCEPTION.

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94002A IS

     PACKAGE P IS
          MY_EXCEPTION : EXCEPTION;
          GLOBAL : INTEGER;
          TASK TYPE T1 IS
               ENTRY E (I : INTEGER);
               PRAGMA PRIORITY (PRIORITY'FIRST);
          END T1;
          TYPE T2 IS LIMITED PRIVATE;
          PROCEDURE CALL_ENTRY (A : T2; B : INTEGER);
     PRIVATE
          TASK TYPE T2 IS
               ENTRY E (I : INTEGER);
               PRAGMA PRIORITY (PRIORITY'FIRST);
          END T2;
     END P;

     PACKAGE BODY P IS
          TASK BODY T1 IS
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
          END T1;

          TASK BODY T2 IS
               LOCAL : INTEGER;
          BEGIN
               ACCEPT E (I : INTEGER) DO
                    LOCAL := I;
               END E;
               DELAY 3.0;  -- TODO: acats-delay-deviation: before was 30.0
               GLOBAL := LOCAL;
          END T2;

          PROCEDURE CALL_ENTRY (A : T2; B : INTEGER) IS
          BEGIN
               A.E (B);
          END CALL_ENTRY;
     END P;

     USE P;

     PRAGMA PRIORITY (PRIORITY'LAST-1);

BEGIN
     TEST ("C94002A", "CHECK THAT A UNIT WITH DEPENDENT TASKS " &
                      "CREATED BY (LOCAL) ALLOCATORS DOES NOT " &
                      "TERMINATE UNTIL ALL DEPENDENT TASKS " &
                      "ARE TERMINATED");

     --------------------------------------------------
     GLOBAL := IDENT_INT (0);
     BEGIN -- (A)
          DECLARE
               TYPE A_T IS ACCESS T1;
               A : A_T;
          BEGIN
               IF EQUAL (3, 3) THEN
                    A := NEW T1;
                    A.ALL.E (IDENT_INT(1));
                    RAISE MY_EXCEPTION;
               END IF;
          END;

          FAILED ("MY_EXCEPTION WAS NOT RAISED - 1");
     EXCEPTION
          WHEN MY_EXCEPTION =>
               IF GLOBAL /= 1 THEN
                    FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                            "BLOCK EXIT - 1");
               END IF;
          WHEN OTHERS =>
               FAILED ("UNEXPECTED EXCEPTION RAISED - 1");
     END; -- (A)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (B)
          TYPE A_T IS ACCESS T2;
          A : A_T;
     BEGIN -- (B)
          IF EQUAL (3, 3) THEN
               A := NEW T2;
               CALL_ENTRY (A.ALL, IDENT_INT(2));
          END IF;
     END; -- (B)

     IF GLOBAL /= 2 THEN
          FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                  "BLOCK EXIT - 2");
     END IF;

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (C)
          I : INTEGER;

          FUNCTION F RETURN INTEGER IS
               TYPE RT;
               TYPE ART IS ACCESS RT;
               TYPE RT IS
                    RECORD
                         A : ART;
                         T : T1;
                    END RECORD;
               LIST : ART;
               TEMP : ART;
          BEGIN
               FOR I IN 1 .. IDENT_INT (1) LOOP
                    TEMP := NEW RT;
                    TEMP.A := LIST;
                    LIST := TEMP;
                    LIST.T.E (IDENT_INT(3));
               END LOOP;
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
               TYPE RT;
               TYPE ART IS ACCESS RT;
               TYPE RT IS
                    RECORD
                         A : ART;
                         T : T2;
                    END RECORD;
               LIST : ART;
               TEMP : ART;
          BEGIN
               FOR I IN 1 .. IDENT_INT (1) LOOP
                    TEMP := NEW RT;
                    TEMP.A := LIST;
                    LIST := TEMP;
                    CALL_ENTRY (LIST.T, IDENT_INT(4));
                    IF EQUAL (3, 3) THEN
                         RAISE MY_EXCEPTION;
                    END IF;
               END LOOP;
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
               FAILED ("UNEXPECTED EXCEPTION RAISED - 4");
     END; -- (D)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (E)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 5 * 60;     -- FIVE MINUTE DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST);
          END TSK;

          TASK BODY TSK IS
               TYPE ARR IS ARRAY (1..1) OF T1;
               TYPE RAT;
               TYPE ARAT IS ACCESS RAT;
               TYPE RAT IS
                    RECORD
                         A : ARAT;
                         T : ARR;
                    END RECORD;
               LIST : ARAT;
               TEMP : ARAT;
          BEGIN
               FOR I IN 1 .. IDENT_INT (1) LOOP
                    TEMP := NEW RAT;
                    TEMP.A := LIST;
                    LIST := TEMP;
                    LIST.T(1).E (IDENT_INT(5));
                    IF EQUAL (3, 3) THEN
                         RAISE MY_EXCEPTION;
                    END IF;
               END LOOP;
          END TSK;

     BEGIN -- (E)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED WITHIN FIVE " &
                       "MINUTES - 5");
          END IF;

          IF GLOBAL /= 5 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "TASK EXIT - 5");
          END IF;

     END; -- (E)

     --------------------------------------------------

     GLOBAL := IDENT_INT (0);

     DECLARE -- (F)

          LOOP_COUNT : INTEGER := 0;
          CUT_OFF : CONSTANT := 5 * 60;     -- FIVE MINUTE DELAY.

          TASK TSK IS
               ENTRY ENT;
               PRAGMA PRIORITY (PRIORITY'LAST);
          END TSK;

          TASK BODY TSK IS
               TYPE ARR IS ARRAY (1..1) OF T2;
               TYPE RAT;
               TYPE ARAT IS ACCESS RAT;
               TYPE RAT IS
                    RECORD
                         A : ARAT;
                         T : ARR;
                    END RECORD;
               LIST : ARAT;
               TEMP : ARAT;
          BEGIN
               FOR I IN 1 .. IDENT_INT (1) LOOP
                    TEMP := NEW RAT;
                    TEMP.A := LIST;
                    LIST := TEMP;
                    CALL_ENTRY (LIST.T(1), IDENT_INT(6));
               END LOOP;
          END TSK;

     BEGIN -- (F)

          WHILE NOT TSK'TERMINATED AND LOOP_COUNT < CUT_OFF LOOP
               DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               LOOP_COUNT := LOOP_COUNT + 1;
          END LOOP;

          IF LOOP_COUNT >= CUT_OFF THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED WITHIN FIVE " &
                       "MINUTES - 6");
          END IF;

          IF GLOBAL /= 6 THEN
               FAILED ("DEPENDENT TASK NOT TERMINATED BEFORE " &
                       "TASK EXIT - 6");
          END IF;

     END; -- (F)

     RESULT;
END C94002A;
