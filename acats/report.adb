-- ACVC support package.  All output flows through TEXT_IO's standard
-- output file so that the line and column accounting the tests observe
-- (TEXT_IO.LINE, TEXT_IO.COL on the default output) includes what
-- REPORT itself prints; tests may meanwhile redirect their own default
-- output elsewhere without silencing the verdict.

WITH TEXT_IO;

PACKAGE BODY REPORT IS

   FAILURES        : INTEGER := 0;
   SPECIAL_ACTIONS : INTEGER := 0;
   TEST_NAME       : STRING (1 .. 15);
   TEST_NAME_LEN   : INTEGER := 0;

   PROCEDURE PUT_LINE_STD (S : STRING) IS
   BEGIN
      TEXT_IO.PUT_LINE (TEXT_IO.STANDARD_OUTPUT, S);
   END PUT_LINE_STD;

   PROCEDURE TEST (NAME : STRING; DESC : STRING) IS
   BEGIN
      FAILURES := 0;
      TEST_NAME_LEN := NAME'LENGTH;
      IF TEST_NAME_LEN > 15 THEN
         TEST_NAME_LEN := 15;
      END IF;
      TEST_NAME (1 .. TEST_NAME_LEN) :=
        NAME (NAME'FIRST .. NAME'FIRST + TEST_NAME_LEN - 1);
      PUT_LINE_STD ("TEST " & NAME & ": " & DESC);
   END TEST;

   PROCEDURE FAILED (MSG : STRING) IS
   BEGIN
      PUT_LINE_STD ("FAILED: " & MSG);
      FAILURES := 1;
   END FAILED;

   PROCEDURE RESULT IS
   BEGIN
      IF FAILURES /= 0 THEN
         PUT_LINE_STD ("FAILED");
      ELSIF SPECIAL_ACTIONS /= 0 THEN
         PUT_LINE_STD ("TENTATIVELY PASSED");
      ELSE
         PUT_LINE_STD ("PASSED");
      END IF;
   END RESULT;

   PROCEDURE COMMENT (MSG : STRING) IS
   BEGIN
      PUT_LINE_STD ("COMMENT: " & MSG);
   END COMMENT;

   PROCEDURE NOT_APPLICABLE (DESCR : STRING) IS
   BEGIN
      PUT_LINE_STD ("NOT APPLICABLE: " & DESCR);
   END NOT_APPLICABLE;

   PROCEDURE SPECIAL_ACTION (DESCR : STRING) IS
   BEGIN
      SPECIAL_ACTIONS := 1;
      PUT_LINE_STD ("SPECIAL ACTION REQUIRED: " & DESCR);
   END SPECIAL_ACTION;

   FUNCTION IDENT_INT (X : INTEGER) RETURN INTEGER IS
   BEGIN
      RETURN X;
   END IDENT_INT;

   FUNCTION IDENT_BOOL (X : BOOLEAN) RETURN BOOLEAN IS
   BEGIN
      RETURN X;
   END IDENT_BOOL;

   FUNCTION IDENT_CHAR (X : CHARACTER) RETURN CHARACTER IS
   BEGIN
      RETURN X;
   END IDENT_CHAR;

   FUNCTION IDENT_STR (X : STRING) RETURN STRING IS
   BEGIN
      RETURN X;
   END IDENT_STR;

   FUNCTION EQUAL (X, Y : INTEGER) RETURN BOOLEAN IS
   BEGIN
      RETURN X = Y;
   END EQUAL;

   FUNCTION LEGAL_FILE_NAME (X : FILE_NUM := 1; NAM : STRING := "")
     RETURN STRING IS
      R : STRING (1 .. 24);
      L : INTEGER := 2;
   BEGIN
      R (1) := 'X';
      R (2) := CHARACTER'VAL (48 + INTEGER (X));
      IF NAM'LENGTH > 0 THEN
         FOR I IN NAM'RANGE LOOP
            IF L < 20 THEN
               L := L + 1;
               R (L) := NAM (I);
            END IF;
         END LOOP;
      ELSE
         FOR I IN 1 .. TEST_NAME_LEN LOOP
            L := L + 1;
            R (L) := TEST_NAME (I);
         END LOOP;
      END IF;
      R (L + 1) := '.';
      R (L + 2) := 'T';
      R (L + 3) := 'M';
      R (L + 4) := 'P';
      RETURN R (1 .. L + 4);
   END LEGAL_FILE_NAME;

END REPORT;
