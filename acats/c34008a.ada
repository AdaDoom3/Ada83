-- C34008A.ADA

-- OBJECTIVE:
--     CHECK THAT THE REQUIRED PREDEFINED OPERATIONS ARE DECLARED
--     (IMPLICITLY) FOR DERIVED TASK TYPES.

-- HISTORY:
--     JRK 08/27/87  CREATED ORIGINAL TEST.

WITH SYSTEM; USE SYSTEM;
WITH REPORT; USE REPORT;

PROCEDURE C34008A IS

     PACKAGE PKG IS

          TASK TYPE PARENT IS
               ENTRY E (I : IN OUT INTEGER);
               ENTRY F (1 .. 3) (I : INTEGER; J : OUT INTEGER);
               ENTRY G;
               ENTRY H (1 .. 3);
               ENTRY R (I : OUT INTEGER);
               ENTRY W (I : INTEGER);
          END PARENT;

          FUNCTION ID (X : PARENT) RETURN INTEGER;

     END PKG;

     USE PKG;

     TYPE T IS NEW PARENT;

     TASK TYPE AUX;

     X : T;
     W : PARENT;
     B : BOOLEAN := FALSE;
     I : INTEGER := 0;
     J : INTEGER := 0;
     A1, A2 : AUX;

     PROCEDURE A (X : ADDRESS) IS
     BEGIN
          B := IDENT_BOOL (TRUE);
     END A;

     FUNCTION V RETURN T IS
     BEGIN
          RETURN X;
     END V;

     PACKAGE BODY PKG IS

          TASK BODY PARENT IS
               N : INTEGER := 1;
          BEGIN
               LOOP
                    SELECT
                         ACCEPT E (I : IN OUT INTEGER) DO
                              I := I + N;
                         END E;
                    OR
                         ACCEPT F (2) (I : INTEGER; J : OUT INTEGER) DO
                              J := I + N;
                         END F;
                    OR
                         ACCEPT G DO
                              WHILE H(2)'COUNT < 2 LOOP
                                   DELAY 0.5;  -- TODO: acats-delay-deviation: before was 5.0
                              END LOOP;
                              ACCEPT H (2) DO
                                   IF E'COUNT    /= 0 OR
                                      F(1)'COUNT /= 0 OR
                                      F(2)'COUNT /= 0 OR
                                      F(3)'COUNT /= 0 OR
                                      G'COUNT    /= 0 OR
                                      H(1)'COUNT /= 0 OR
                                      H(2)'COUNT /= 1 OR
                                      H(3)'COUNT /= 0 OR
                                      R'COUNT    /= 0 OR
                                      W'COUNT    /= 0 THEN
                                        FAILED ("INCORRECT 'COUNT");
                                   END IF;
                              END H;
                              ACCEPT H (2);
                         END G;
                    OR
                         ACCEPT R (I : OUT INTEGER) DO
                              I := N;
                         END R;
                    OR
                         ACCEPT W (I : INTEGER) DO
                              N := I;
                         END W;
                    OR
                         TERMINATE;
                    END SELECT;
               END LOOP;
          END PARENT;

          FUNCTION ID (X : PARENT) RETURN INTEGER IS
               I : INTEGER;
          BEGIN
               X.R (I);
               RETURN I;
          END ID;

     END PKG;

     TASK BODY AUX IS
     BEGIN
          X.H (2);
     END AUX;

BEGIN
     TEST ("C34008A", "CHECK THAT THE REQUIRED PREDEFINED OPERATIONS " &
                      "ARE DECLARED (IMPLICITLY) FOR DERIVED TASK " &
                      "TYPES");

     X.W (IDENT_INT (2));
     IF ID (X) /= 2 THEN
          FAILED ("INCORRECT INITIALIZATION");
     END IF;

     IF ID (T'(X)) /= 2 THEN
          FAILED ("INCORRECT QUALIFICATION");
     END IF;

     IF ID (T (X)) /= 2 THEN
          FAILED ("INCORRECT SELF CONVERSION");
     END IF;

     W.W (IDENT_INT (3));
     IF ID (T (W)) /= 3 THEN
          FAILED ("INCORRECT CONVERSION FROM PARENT");
     END IF;

     IF ID (PARENT (X)) /= 2 THEN
          FAILED ("INCORRECT CONVERSION TO PARENT");
     END IF;

     I := 5;
     X.E (I);
     IF I /= 7 THEN
          FAILED ("INCORRECT SELECTION (ENTRY)");
     END IF;

     I := 5;
     X.F (IDENT_INT (2)) (I, J);
     IF J /= 7 THEN
          FAILED ("INCORRECT SELECTION (FAMILY)");
     END IF;

     IF NOT (X IN T) THEN
          FAILED ("INCORRECT ""IN""");
     END IF;

     IF X NOT IN T THEN
          FAILED ("INCORRECT ""NOT IN""");
     END IF;

     B := FALSE;
     A (T'ADDRESS);
     IF NOT B THEN
          FAILED ("INCORRECT UNIT'ADDRESS");
     END IF;

     B := FALSE;
     A (X'ADDRESS);
     IF NOT B THEN
          FAILED ("INCORRECT OBJECT'ADDRESS");
     END IF;

     IF T'BASE'SIZE < 1 THEN
          FAILED ("INCORRECT 'BASE'SIZE");
     END IF;

     IF NOT X'CALLABLE THEN
          FAILED ("INCORRECT OBJECT'CALLABLE");
     END IF;

     IF NOT V'CALLABLE THEN
          FAILED ("INCORRECT VALUE'CALLABLE");
     END IF;

     X.G;

     IF T'SIZE /= T'BASE'SIZE THEN
          FAILED ("INCORRECT TYPE'SIZE");
     END IF;

     IF X'SIZE < T'SIZE THEN
          FAILED ("INCORRECT OBJECT'SIZE");
     END IF;

     IF T'STORAGE_SIZE < 0 THEN
          FAILED ("INCORRECT TYPE'STORAGE_SIZE");
     END IF;

     IF X'STORAGE_SIZE < 0 THEN
          FAILED ("INCORRECT OBJECT'STORAGE_SIZE");
     END IF;

     IF X'TERMINATED THEN
          FAILED ("INCORRECT OBJECT'TERMINATED");
     END IF;

     IF V'TERMINATED THEN
          FAILED ("INCORRECT VALUE'TERMINATED");
     END IF;

     RESULT;
END C34008A;
