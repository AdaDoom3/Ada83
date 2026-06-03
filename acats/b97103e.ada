-- B97103E.ADA


-- CHECK THAT A  SELECTIVE_WAIT  STATEMENT CANNOT APPEAR OUTSIDE A
--     TASK BODY.  (PART E: INSIDE TASK SPEC)


-- RM 4/02/1982


PROCEDURE  B97103E  IS
BEGIN


     -------------------------------------------------------------------


     DECLARE


          TASK TYPE  TT2  IS

               ENTRY  A ;
               ENTRY  B ;

               SELECT                  -- ERROR: SELECTIVE_WAIT OUTSIDE
                                       --                     TASK BODY.

                    WHEN NOT FALSE =>
                         DELAY 0.25 ;  -- TODO: acats-delay-deviation: before was 2.5
               OR
                         DELAY 0.0 ;
               OR
                         ACCEPT  B ;   -- ERROR: ...
               OR
                    WHEN TRUE =>
                         ACCEPT  A ;   -- ERROR: ...
               END SELECT;

          END  TT2 ;


          TASK BODY  TT2  IS
          BEGIN
               NULL ;
          END  TT2 ;


     BEGIN


          NULL ;


     END  ;

     -------------------------------------------------------------------


END  B97103E ;
