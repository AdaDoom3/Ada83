-- C43209A_MIN.ADA
WITH REPORT; USE REPORT;
PROCEDURE C43209A_MIN IS
     TYPE MULTI_ARRAY IS ARRAY(1 .. 2, 1 .. 3, 1 .. 6) OF CHARACTER;
     X : MULTI_ARRAY;
BEGIN
     TEST("C43209A_MIN", "MINIMAL TEST");
     X := MULTI_ARRAY'((1 =>(1 =>"WHOZAT",
                        2 =>('A', 'B', 'C', 'D', 'E', 'F'),
                        3 =>('G', 'H', 'I', 'J', 'K', 'L')),
                       2 => (1 =>('M', 'N', 'O', 'P', 'Q', 'R'),
                        2 =>('S', 'T', 'U', 'V', 'W', 'X'),
                        3 => ('W', 'Z', 'A', 'B', 'C', 'D'))));
     RESULT;
END C43209A_MIN;
