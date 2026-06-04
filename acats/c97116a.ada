-- C97116A.ADA

-- CHECK THAT THE GUARD CONDITIONS IN A SELECTIVE WAIT STATEMENT ARE NOT
-- RE-EVALUATED DURING THE WAIT.

-- THIS TEST CONTAINS SHARED VARIABLES.

-- WRG 7/10/86

WITH REPORT; USE REPORT;
PROCEDURE C97116A IS

     GUARD_EVALUATIONS : NATURAL := 0;
     DELAY_EVALUATED   : BOOLEAN := FALSE;
     UNBLOCKED         : BOOLEAN := FALSE;

     PRAGMA SHARED (GUARD_EVALUATIONS);
     PRAGMA SHARED (DELAY_EVALUATED  );
     PRAGMA SHARED (UNBLOCKED        );

     FUNCTION GUARD RETURN BOOLEAN IS
     BEGIN
          GUARD_EVALUATIONS := GUARD_EVALUATIONS + 1;
          RETURN UNBLOCKED;
     END GUARD;

     FUNCTION SO_LONG RETURN DURATION IS
     BEGIN
          DELAY_EVALUATED := TRUE;
          RETURN 2.0;  -- TODO: acats-delay-deviation: before was 20.0
     END SO_LONG;

BEGIN

     TEST ("C97116A", "CHECK THAT THE GUARD CONDITIONS IN A " &
                      "SELECTIVE WAIT STATEMENT ARE NOT RE-EVALUATED " &
                      "DURING THE WAIT");

     DECLARE

          TASK T IS
               ENTRY E;
          END T;

          TASK BODY T IS
          BEGIN
               SELECT
                    ACCEPT E;
                    FAILED ("ACCEPTED NONEXISTENT CALL TO E");
               OR WHEN GUARD     =>
                    DELAY 0.0;
                    FAILED ("EXECUTED ALTERNATIVE CLOSED BY FALSE " &
                            "GUARD FUNCTION" );
               OR WHEN UNBLOCKED =>
                    DELAY 0.0;
                    FAILED ("EXECUTED ALTERNATIVE CLOSED BY FALSE " &
                            "GUARD VARIABLE" );
               OR
                    DELAY SO_LONG;
               END SELECT;
          EXCEPTION
               WHEN OTHERS =>
                    FAILED ("EXCEPTION RAISED");
          END T;

          TASK CHANGING_OF_THE_GUARD;

          TASK BODY CHANGING_OF_THE_GUARD IS
          BEGIN
               WHILE GUARD_EVALUATIONS = 0 AND NOT DELAY_EVALUATED LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
               END LOOP;

               UNBLOCKED := TRUE;  -- IF EITHER THE CONDITION "GUARD" OR
                                   -- THE CONDITION "UNBLOCKED" ARE
                                   -- RE-EVALUATED, THEY WILL HEREAFTER
                                   -- EVALUATE TO TRUE.
          END CHANGING_OF_THE_GUARD;

     BEGIN

          NULL;

     END;

     IF GUARD_EVALUATIONS /= 1 THEN
          FAILED ("GUARD EVALUATED" &
                  NATURAL'IMAGE(GUARD_EVALUATIONS) & " TIMES");
     END IF;

     RESULT;

END C97116A;
