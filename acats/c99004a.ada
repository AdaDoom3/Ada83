-- C99004A.ADA

-- OBJECTIVE:
--     CHECK THAT THE PREFIX OF 'TERMINATED AND 'CALLABLE CAN BE A
--     FUNCTION CALL RETURNING AN OBJECT HAVING A TASK TYPE.

--     NOTE: SEE TEST C38202A FOR CHECKS INVOLVING PREFIXES WHICH ARE
--     ACCESS TYPES DENOTING TASK TYPES OR WHICH ARE FUNCTIONS
--     RETURNING ACCESS TYPES DENOTING TASK TYPES.

-- HISTORY:
--     RJW 09/16/86 CREATED ORIGINAL TEST.
--     DHH 10/15/87 CORRECTED HEADER COMMENTS.

WITH REPORT; USE REPORT;
PROCEDURE C99004A IS

     TYPE ENUM IS (A, B, C, D);

     EARRAY : ARRAY (ENUM) OF STRING (1 .. 17) :=
                                        (A => "BEFORE ACTIVATION",
                                         B => "DURING ACTIVATION",
                                         C => "DURING EXECUTION ",
                                         D => "AFTER TERMINATION" );

     FUNCTION CHECK (S : STRING; CALL, B1, TERM, B2 : BOOLEAN;
                     E : ENUM) RETURN BOOLEAN IS
     BEGIN
          IF CALL /= B1 THEN
               FAILED ( "INCORRECT VALUE FOR " & S & "'CALLABLE " &
                         EARRAY (E) & " OF TASK" );
          END IF;

          IF TERM /= B2 THEN
               FAILED ( "INCORRECT VALUE FOR " & S & "'TERMINATED " &
                         EARRAY (E) & " OF TASK" );
          END IF;

          RETURN IDENT_BOOL (TRUE);
     END CHECK;


BEGIN
     TEST ( "C99004A", "CHECK THAT THE PREFIX OF 'TERMINATED AND " &
                       "'CALLABLE CAN BE A FUNCTION CALL RETURNING " &
                       "AN OBJECT HAVING A TASK TYPE" );

     DECLARE

          TASK TYPE TT IS
               ENTRY E;
          END TT;

          PACKAGE PKG1 IS
               T1 : TT;
          END PKG1;

          FUNCTION F RETURN TT IS
          BEGIN
               RETURN PKG1.T1;
          END F;

          PACKAGE PKG2 IS
               A1 : BOOLEAN := CHECK ("F", F'CALLABLE, TRUE,
                                      F'TERMINATED, FALSE, A);
          END PKG2;

          TASK MAIN_TASK IS
               ENTRY E (INTEGER RANGE 1 .. 2);
          END MAIN_TASK;

          TASK BODY TT IS
               B1 : BOOLEAN := CHECK ("F", F'CALLABLE, TRUE,
                                      F'TERMINATED, FALSE, B);
               C1 : BOOLEAN;
          BEGIN
               C1 := CHECK ("F", F'CALLABLE, TRUE,
                            F'TERMINATED, FALSE, C);
               MAIN_TASK.E (1);
               MAIN_TASK.E (2);
          END TT;

          PACKAGE BODY PKG1 IS
          BEGIN
               NULL;
          END;

          TASK BODY MAIN_TASK IS
               D1 : BOOLEAN;
          BEGIN
               ACCEPT E (1);
               ABORT PKG1.T1;
               DELAY 0.5;  -- TODO: acats-delay-deviation: before was 5.0
               D1 := CHECK ("F", F'CALLABLE, FALSE,
                             F'TERMINATED, TRUE, D);
          END MAIN_TASK;

     BEGIN
          NULL;
     END;

     DECLARE

          TASK TYPE TT IS
               ENTRY E;
          END TT;

          T2 : TT;

          A2 : BOOLEAN := CHECK ("T2", T2'CALLABLE, TRUE,
                                  T2'TERMINATED, FALSE, A);

          TASK MAIN_TASK IS
               ENTRY E (INTEGER RANGE 1 .. 2);
          END MAIN_TASK;

          TASK BODY TT IS
               B2 : BOOLEAN := CHECK ("T2", T2'CALLABLE, TRUE,
                                       T2'TERMINATED, FALSE, B);
               C2 : BOOLEAN;
          BEGIN
               C2 := CHECK ("T2", T2'CALLABLE, TRUE,
                             T2'TERMINATED, FALSE, C);
               MAIN_TASK.E (1);
               MAIN_TASK.E (2);
          END TT;

          TASK BODY MAIN_TASK IS
               D2 : BOOLEAN;
          BEGIN
               ACCEPT E (1);
               ABORT T2;
               DELAY 0.5;  -- TODO: acats-delay-deviation: before was 5.0
               D2 := CHECK ("T2", T2'CALLABLE, FALSE,
                             T2'TERMINATED, TRUE, D);
          END MAIN_TASK;

     BEGIN
          NULL;
     END;

     RESULT;
END C99004A;
