-- C64103C.ADA

-- CHECK THAT THE APPROPRIATE EXCEPTION IS RAISED FOR TYPE CONVERSIONS
-- ON IN OUT ARRAY PARAMETERS.  IN PARTICULAR:
--   (A) CONSTRAINT_ERROR IS RAISED BEFORE THE CALL WHEN THE ACTUAL
--       COMPONENT'S CONSTRAINTS DIFFER FROM THE FORMAL COMPONENT'S
--       CONSTRAINTS.
--   (B) CONSTRAINT_ERROR IS RAISED BEFORE THE CALL WHEN CONVERSION TO
--       AN UNCONSTRAINED ARRAY TYPE CAUSES AN ACTUAL INDEX BOUND TO LIE
--       OUTSIDE OF A FORMAL INDEX SUBTYPE FOR A NON-NULL DIMENSION (SEE
--       AI-00313 FOR MULTIDIMENSIONAL CASE)
--   (C) CONSTRAINT_ERROR IS RAISED BEFORE THE CALL FOR CONVERSION TO A
--       CONSTRAINED ARRAY TYPE WHEN THE NUMBER OF COMPONENTS PER
--       DIMENSION OF THE ACTUAL DIFFERS FROM THAT OF THE FORMAL.
--   (D) NUMERIC_ERROR IS RAISED BEFORE THE CALL WHEN CONVERSION TO AN
--       UNCONSTRAINED ARRAY TYPE CAUSES AN ACTUAL INDEX BOUND TO LIE
--       OUTSIDE OF THE BASE INDEX TYPE OF THE FORMAL.

-- CPP 07/19/84
-- JBG 06/05/85
-- EG  10/29/85  FIX NUMERIC_ERROR/CONSTRAINT_ERROR ACCORDING TO
--               AI-00387.

WITH SYSTEM;
WITH REPORT;  USE REPORT;
PROCEDURE C64103C IS

     BEGIN
     TEST ("C64103C", "CHECK THAT APPROPRIATE EXCEPTION IS RAISED ON " &
           "TYPE CONVERSIONS OF IN OUT ARRAY PARAMETERS");

     -----------------------------------------------

     DECLARE   -- (A)
     BEGIN     -- (A)

          DECLARE
               SUBTYPE INDEX IS INTEGER RANGE 1..3;
               SUBTYPE INT IS INTEGER RANGE -8..0;
               SUBTYPE INT1 IS INTEGER RANGE -8..IDENT_INT(1);
               TYPE ARRAY_TYPE IS ARRAY (INDEX) OF INT;
               TYPE AR_TYPE IS ARRAY (INDEX) OF INT1;
               A0 : ARRAY_TYPE := (1..3 => 0);

               PROCEDURE P1 (X : IN OUT AR_TYPE) IS
               BEGIN
                    FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P1 (A)");
               END P1;
          BEGIN
               P1 (AR_TYPE (A0));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (A)");
          END;

          DECLARE
               SUBTYPE INT IS INTEGER RANGE 0..8;
               SUBTYPE INT1 IS INTEGER RANGE -1..8;
               TYPE ARRAY_TYPE IS ARRAY (INTEGER RANGE <>) OF INT;
               TYPE AR_TYPE IS ARRAY (INTEGER RANGE <>) OF INT1;
               A0 : ARRAY_TYPE (1..3) := (1..3 => 1);

               PROCEDURE P2 (X : IN OUT AR_TYPE) IS
               BEGIN
                    FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P2 (A)");
               END P2;
          BEGIN
               P2 (AR_TYPE (A0));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P2 (A)");
          END;

          DECLARE
               TYPE SUBINT IS RANGE 0..8;
               TYPE ARRAY_TYPE IS ARRAY (SUBINT RANGE <>) OF BOOLEAN;
               A0 : ARRAY_TYPE (0..3) := (0..3 => TRUE);

               PROCEDURE P2 (X : IN OUT ARRAY_TYPE) IS
               BEGIN
                    NULL;
               END P2;
          BEGIN
               P2 (ARRAY_TYPE (A0));                  -- OK.
          EXCEPTION
               WHEN OTHERS =>
                    FAILED ("EXCEPTION RAISED -P2 (A)");
          END;

     END; -- (A)

     -----------------------------------------------

     DECLARE   -- (B1) NON-NULL ACTUAL PARAMETER

          TYPE SUBINT IS RANGE 0..8;
          TYPE ARRAY_TYPE IS ARRAY (SUBINT RANGE <>) OF BOOLEAN;
          TYPE AR1 IS ARRAY (INTEGER RANGE <>) OF BOOLEAN;
          A1 : AR1 (-1..7) := (-1..7 => TRUE);
          A2 : AR1 (1..9) := (1..9 => TRUE);

          PROCEDURE P1 (X : IN OUT ARRAY_TYPE) IS
          BEGIN
               FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P1 (B)");
          END P1;

     BEGIN     -- (B1)

          BEGIN
               COMMENT ("CALL TO P1 (B1) ON A1");
               P1 (ARRAY_TYPE (A1));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (B1)");
          END;

          BEGIN
               COMMENT ("CALL TO P1 (B1) ON A2");
               P1 (ARRAY_TYPE (A2));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (B1)");
          END;

     END; -- (B1)

     DECLARE   -- (B2) NULL ACTUAL PARAMETER; MULTIDIMENSIONAL

          TYPE SUBINT IS RANGE 0..8;
          TYPE ARRAY_TYPE IS ARRAY (SUBINT RANGE <>,
                                    SUBINT RANGE <>) OF BOOLEAN;
          TYPE AR1 IS ARRAY (INTEGER RANGE <>,
                             INTEGER RANGE <>)OF BOOLEAN;
          A1 : AR1 (IDENT_INT(-1)..7, 5..4) :=
                                           (OTHERS => (OTHERS => TRUE));
          A2 : AR1 (5..4, 1..IDENT_INT(9)) :=
                                           (OTHERS => (OTHERS => TRUE));
          PROCEDURE P1 (X : IN OUT ARRAY_TYPE) IS
          BEGIN
               FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P1 (B)");
          END P1;

     BEGIN     -- (B2)

          BEGIN
               COMMENT ("CALL TO P1 (B2) ON A1");
               P1 (ARRAY_TYPE (A1));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (B2)");
          END;

          BEGIN
               COMMENT ("CALL TO P1 (B2) ON A2");
               P1 (ARRAY_TYPE (A2));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (B2)");
          END;

     END; -- (B2)

     -----------------------------------------------

     BEGIN     -- (C)

          DECLARE
               TYPE INDEX1 IS RANGE 1..3;
               TYPE INDEX2 IS RANGE 1..4;
               TYPE AR_TYPE IS ARRAY (INDEX1, INDEX2) OF BOOLEAN;
               A0 : AR_TYPE := (1..3 => (1..4 => FALSE));

               TYPE I1 IS RANGE 1..4;
               TYPE I2 IS RANGE 1..3;
               TYPE ARRAY_TYPE IS ARRAY (I1, I2) OF BOOLEAN;

               PROCEDURE P1 (X : IN OUT ARRAY_TYPE) IS
               BEGIN
                    FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P1 (C)");
               END P1;
          BEGIN
               P1 (ARRAY_TYPE (A0));
          EXCEPTION
               WHEN CONSTRAINT_ERROR =>
                    NULL;
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED -P1 (C)");
          END;

     END; -- (C)

     -----------------------------------------------

     DECLARE   -- (D)
     BEGIN     -- (D)

          DECLARE
               TYPE SM_INT IS RANGE 0..2;
               TYPE LG IS RANGE 0 .. SYSTEM.MAX_INT;
               SUBTYPE LG_INT IS LG RANGE SYSTEM.MAX_INT - 3 ..
                                          SYSTEM.MAX_INT;
               TYPE AR_SMALL IS ARRAY (SM_INT RANGE <>) OF BOOLEAN;
               TYPE AR_LARGE IS ARRAY (LG_INT RANGE <>) OF BOOLEAN;
               A0 : AR_LARGE (SYSTEM.MAX_INT - 2..SYSTEM.MAX_INT) :=
                    (SYSTEM.MAX_INT - 2..SYSTEM.MAX_INT => TRUE);

               PROCEDURE P1 (X : IN OUT AR_SMALL) IS
               BEGIN
                    FAILED ("EXCEPTION NOT RAISED BEFORE CALL -P1 (D)");
               END P1;
          BEGIN
               IF LG (SM_INT'BASE'LAST) < LG_INT'BASE'LAST THEN
                    P1 (AR_SMALL (A0));
               ELSE
                    COMMENT ("NOT APPLICABLE -P1 (D)");
               END IF;
          EXCEPTION
               WHEN NUMERIC_ERROR =>
                    COMMENT ("NUMERIC_ERROR RAISED - P1 (D)");
               WHEN CONSTRAINT_ERROR =>
                    COMMENT ("CONSTRAINT_ERROR RAISED - P1 (D)");
               WHEN OTHERS =>
                    FAILED ("WRONG EXCEPTION RAISED - P1 (D)");
          END;

     END; -- (D)

     -----------------------------------------------

     RESULT;

END C64103C;
