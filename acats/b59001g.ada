-- B59001G.ADA


-- CHECK THAT JUMPS FROM ONE SELECT_ALTERNATIVE TO ANOTHER  ARE NOT
--    ALLOWED.


-- RM 08/17/82


PROCEDURE  B59001G  IS
BEGIN

     DECLARE

          TASK  T  IS
               ENTRY  E1 ;
               ENTRY  E2 ;
          END  T ;

          TASK BODY  T  IS
          BEGIN

               SELECT 
                    ACCEPT  E1 ;  
                    GOTO  L111 ;  -- ERROR: JUMP FROM ONE SELECT_ALTERN.
                                  --     TO ANOTHER.
               OR
                    ACCEPT  E2 ;
                    << L111 >>  NULL ;
               OR
                    DELAY 1.0 ;  -- TODO: acats-delay-deviation: before was 10.0
                    GOTO  L111 ;  -- ERROR: JUMP FROM ONE SELECT_ALTERN.
                                  --     TO ANOTHER.
               END SELECT;

          END T;

     BEGIN

          NULL ;

     END;


END  B59001G ;
