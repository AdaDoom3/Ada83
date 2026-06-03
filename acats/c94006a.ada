-- C94006A.ADA

-- CHECK THAT A DECLARATION THAT RENAMES A TASK DOES NOT CREATE A NEW
-- MASTER FOR THE TASK.

-- TBN  9/17/86

WITH REPORT; USE REPORT;
WITH SYSTEM; USE SYSTEM;
PROCEDURE C94006A IS

     TASK TYPE TT IS
          PRAGMA PRIORITY (PRIORITY'FIRST);
          ENTRY E;
     END TT;

     TASK BODY TT IS
     BEGIN
          SELECT
               ACCEPT E;
          OR
               DELAY 3.0;  -- TODO: acats-delay-deviation: before was 30.0
          END SELECT;
     END TT;

     PRAGMA PRIORITY (PRIORITY'LAST-1);

BEGIN
     TEST ("C94006A", "CHECK THAT A DECLARATION THAT RENAMES A TASK " &
                      "DOES NOT CREATE A NEW MASTER FOR THE TASK");

     -------------------------------------------------------------------
     DECLARE
          T1 : TT;
     BEGIN
          DECLARE
               RENAME_TASK : TT RENAMES T1;
          BEGIN
               NULL;
          END;
          IF T1'TERMINATED THEN
               FAILED ("TASK DEPENDENT ON WRONG UNIT - 1");
          ELSE
               T1.E;
          END IF;
     END;

     -------------------------------------------------------------------

     DECLARE
          T2 : TT;

          PACKAGE P IS
               Q : TT RENAMES T2;
          END P;

          PACKAGE BODY P IS
          BEGIN
               NULL;
          END P;

          USE P;
     BEGIN
          IF Q'TERMINATED THEN
               FAILED ("TASK DEPENDENT ON WRONG UNIT - 2");
          ELSE
               Q.E;
          END IF;
     END;

     -------------------------------------------------------------------

     DECLARE
          TYPE ACC_TT IS ACCESS TT;
          P1 : ACC_TT;
     BEGIN
          DECLARE
               RENAME_ACCESS : ACC_TT RENAMES P1;
          BEGIN
               RENAME_ACCESS := NEW TT;
          END;
          IF P1'TERMINATED THEN
               FAILED ("TASK DEPENDENT ON WRONG UNIT - 3");
          ELSE
               P1.E;
          END IF;
     END;

     -------------------------------------------------------------------

     DECLARE
          TYPE ACC_TT IS ACCESS TT;
          P2 : ACC_TT;

          PACKAGE Q IS
               RENAME_ACCESS : ACC_TT RENAMES P2;
          END Q;

          PACKAGE BODY Q IS
          BEGIN
               RENAME_ACCESS := NEW TT;
          END Q;

          USE Q;
     BEGIN
          IF RENAME_ACCESS'TERMINATED THEN
               FAILED ("TASK DEPENDENT ON WRONG UNIT - 4");
          ELSE
               RENAME_ACCESS.E;
          END IF;
     END;

     RESULT;
END C94006A;
