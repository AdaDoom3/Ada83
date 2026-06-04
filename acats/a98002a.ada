-- A98002A.ADA

-- OBJECTIVE:
--     WHEN CALLS ARE QUEUED ON SEVERAL OPEN ALTERNATIVES OF A SELECT
--     STATEMENT, CHECK TO SEE IF PRIORITIES SEEM TO BE CONSIDERED IN
--     DECIDING WHICH CALL TO ACCEPT.

-- HISTORY:
--     DHH 03/24/88 CREATED ORIGINAL TEST.

WITH SYSTEM; USE SYSTEM;
WITH REPORT; USE REPORT;
PROCEDURE A98002A IS

BEGIN

     TEST("A98002A", "WHEN CALLS ARE QUEUED ON SEVERAL OPEN " &
                     "ALTERNATIVES OF A SELECT STATEMENT, CHECK " &
                     "TO SEE IF PRIORITIES SEEM TO BE CONSIDERED IN " &
                     "DECIDING WHICH CALL TO ACCEPT");
     DECLARE
          Y : CONSTANT INTEGER := PRIORITY'LAST;
          Z : CONSTANT INTEGER := PRIORITY'FIRST;

          SUBTYPE STR IS STRING(1 .. 10);

          TYPE ARR IS ARRAY(1 .. 10) OF INTEGER;
          ARRY : ARR;

          TASK A IS
               PRAGMA PRIORITY(Y);
          END A;

          TASK B IS
               PRAGMA PRIORITY(Y);
          END B;

          TASK C IS
               PRAGMA PRIORITY(Y);
          END C;

          TASK D IS
               PRAGMA PRIORITY(Y);
          END D;

          TASK E IS
               PRAGMA PRIORITY(Y);
          END E;

          TASK F IS
               PRAGMA PRIORITY(Z);
          END F;

          TASK G IS
               PRAGMA PRIORITY(Z);
          END G;

          TASK H IS
               PRAGMA PRIORITY(Z);
          END H;

          TASK I IS
               PRAGMA PRIORITY(Z);
          END I;

          TASK J IS
               PRAGMA PRIORITY(Z);
          END J;

          TASK K IS
               PRAGMA PRIORITY(Z);
          END K;

          TASK L IS
               PRAGMA PRIORITY(Z);
          END L;

          TASK M IS
               PRAGMA PRIORITY(Z);
          END M;

          TASK N IS
               PRAGMA PRIORITY(Z);
          END N;

          TASK O IS
               PRAGMA PRIORITY(Z);
          END O;

          TASK T IS
               ENTRY WAIT;
          END T;

          TASK CHOICE IS
               ENTRY RETURN_CALL;
               ENTRY E2;
               ENTRY E1;
               ENTRY E3;
          END CHOICE;

          TASK BODY A IS
          BEGIN
               CHOICE.E1;
          END A;

          TASK BODY B IS
          BEGIN
               CHOICE.E1;
          END B;

          TASK BODY C IS
          BEGIN
               CHOICE.E1;
          END C;

          TASK BODY D IS
          BEGIN
               CHOICE.E1;
          END D;

          TASK BODY E IS
          BEGIN
               CHOICE.E1;
          END E;

          TASK BODY F IS
          BEGIN
               CHOICE.E2;
          END F;

          TASK BODY G IS
          BEGIN
               CHOICE.E2;
          END G;

          TASK BODY H IS
          BEGIN
               CHOICE.E2;
          END H;

          TASK BODY I IS
          BEGIN
               CHOICE.E2;
          END I;

          TASK BODY J IS
          BEGIN
               CHOICE.E2;
          END J;

          TASK BODY K IS
          BEGIN
               CHOICE.E3;
          END K;

          TASK BODY L IS
          BEGIN
               CHOICE.E3;
          END L;

          TASK BODY M IS
          BEGIN
               CHOICE.E3;
          END M;

          TASK BODY N IS
          BEGIN
               CHOICE.E3;
          END N;

          TASK BODY O IS
          BEGIN
               CHOICE.E3;
          END O;

          TASK BODY T IS
          BEGIN
               LOOP
                    SELECT
                         ACCEPT WAIT DO
                              DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                         END WAIT;
                         CHOICE.RETURN_CALL;
                    OR
                         TERMINATE;
                    END SELECT;
               END LOOP;
          END T;

          TASK BODY CHOICE IS
               E1_SCORE, E2_SCORE, E3_SCORE : INTEGER := 0;
               X : INTEGER := 1;
          BEGIN
               WHILE E1'COUNT + E2'COUNT + E3'COUNT < 15 LOOP
                    T.WAIT;
                    ACCEPT RETURN_CALL;
               END LOOP;

               FOR I IN 1 .. 15 LOOP
                    SELECT
                         ACCEPT E2 DO
                              E2_SCORE := E2_SCORE + X;
                              X := X + 1;
                         END E2;
                    OR
                         ACCEPT E1 DO
                              E1_SCORE := E1_SCORE + X;
                              X := X + 1;
                         END E1;
                    OR
                         ACCEPT E3 DO
                              E3_SCORE := E3_SCORE + X;
                              X := X + 1;
                         END E3;
                    END SELECT;
               END LOOP;

               IF E1_SCORE < E2_SCORE AND E1_SCORE < E3_SCORE THEN
                    COMMENT("APPARENTLY PRIORITIES ARE CONSIDERED IN " &
                            "DECIDING WHICH CALLS TO ACCEPT");
               ELSE
                    COMMENT("APPARENTLY PRIORITIES ARE NOT CONSIDERED" &
                            " IN DECIDING WHICH CALLS TO ACCEPT");
               END IF;

          END CHOICE;
     BEGIN
          NULL;
     END;

     RESULT;
END A98002A;
