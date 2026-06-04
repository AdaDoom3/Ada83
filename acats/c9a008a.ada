-- C9A008A.ADA

-- CHECK THAT AN ABORTED TASK IS REMOVED FROM ANY ENTRY QUEUE IT MAY 
-- BE ON.

-- R.WILLIAMS 9/17/86

WITH REPORT; USE REPORT;
PROCEDURE C9A008A IS

BEGIN
     TEST ( "C9A008A", "CHECK THAT AN ABORTED TASK IS REMOVED FROM " &
                        "ANY ENTRY QUEUE IT MAY BE ON" );

     DECLARE -- (A). ABORTED TASK AT FRONT OF QUEUE.

          TASK ABORTED;

          TASK T1 IS
               ENTRY RELEASE;
          END T1;

          TASK T2 IS
               ENTRY RELEASE;
          END T2;

          TASK CALLED IS
               ENTRY QENTRY (I : INTEGER);
          END CALLED;

          TASK BODY ABORTED IS
          BEGIN
               CALLED.QENTRY (IDENT_INT (0));
          END ABORTED;

          TASK BODY T1 IS
          BEGIN
               ACCEPT RELEASE;
               CALLED.QENTRY (IDENT_INT (1));
          END T1;

          TASK BODY T2 IS
          BEGIN
               ACCEPT RELEASE;
               CALLED.QENTRY (IDENT_INT (2));
          END T2;

          TASK BODY CALLED IS
               QUEUE_ORDER : INTEGER := 0;
          BEGIN
                         
               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 1;
               END LOOP;               
               
               T1.RELEASE;

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 2;
               END LOOP;               
               
               T2.RELEASE;

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 3;
               END LOOP;               
               
               ABORT ABORTED;     
          
               IF QENTRY'COUNT = 3 THEN
                    FAILED ( "ABORTED TASK NOT REMOVED FROM QUEUE " &
                             "- (A)" );
                    FOR J IN 1 .. 3 LOOP
                         ACCEPT QENTRY (I : INTEGER);
                    END LOOP;
               ELSIF QENTRY'COUNT = 2 THEN
                    FOR J IN 1 .. 2 LOOP
                         ACCEPT QENTRY (I : INTEGER) DO
                              QUEUE_ORDER := 10 * QUEUE_ORDER + I;
                         END QENTRY;
                    END LOOP;
                    IF QUEUE_ORDER /= IDENT_INT (12) THEN
                         FAILED ( "QUEUE NOT PROPERLY MAINTAINED " &
                                  "-- ORDER SHOULD BE 12 " &
                                  "-- ACTUAL VALUE IS " &
                                   INTEGER'IMAGE (QUEUE_ORDER) &
                                  " - (A)" );
                    END IF;
               ELSIF QENTRY'COUNT = 1 THEN
                    ACCEPT QENTRY (I : INTEGER) DO
                         IF I = 0 THEN 
                              FAILED ( "ALL TASKS EXCEPT ABORTED " &
                                       "REMOVED FROM QUEUE - (A)" );
                         ELSE
                              FAILED ( "TOO MANY TASKS REMOVED " &
                                       "FROM QUEUE - (A)" );
                         END IF;
                    END QENTRY;
               ELSE  
                    FAILED ( "ALL TASKS REMOVED FROM QUEUE - (A)" );
               END IF;

          END CALLED;
                                   
     BEGIN 
          NULL;          
     END;  -- (A).        

     DECLARE -- (B). ABORTED TASK IN THE MIDDLE OF QUEUE.

          TASK ABORTED IS
               ENTRY RELEASE;
          END ABORTED;

          TASK T1;

          TASK T2 IS
               ENTRY RELEASE;
          END T2;

          TASK CALLED IS
               ENTRY QENTRY (I : INTEGER);
          END CALLED;

          TASK BODY ABORTED IS
          BEGIN
               ACCEPT RELEASE;
               CALLED.QENTRY (IDENT_INT (0));
          END ABORTED;

          TASK BODY T1 IS
          BEGIN
               CALLED.QENTRY (IDENT_INT (1));
          END T1;

          TASK BODY T2 IS
          BEGIN
               ACCEPT RELEASE;
               CALLED.QENTRY (IDENT_INT (2));
          END T2;

          TASK BODY CALLED IS
               QUEUE_ORDER : INTEGER := 0;
          BEGIN

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 1;
               END LOOP;               
                         
               ABORTED.RELEASE;

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 2;
               END LOOP;               
               
               T2.RELEASE;

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 3;
               END LOOP;               
               
               ABORT ABORTED;     
          
               IF QENTRY'COUNT = 3 THEN
                    FAILED ( "ABORTED TASK NOT REMOVED FROM QUEUE " &
                             "- (B)" );
                    FOR J IN 1 .. 3 LOOP
                         ACCEPT QENTRY (I : INTEGER);
                    END LOOP;
               ELSIF QENTRY'COUNT = 2 THEN
                    FOR J IN 1 .. 2 LOOP
                         ACCEPT QENTRY (I : INTEGER) DO
                              QUEUE_ORDER := 10 * QUEUE_ORDER + I;
                         END QENTRY;
                    END LOOP;
                    IF QUEUE_ORDER /= IDENT_INT (12) THEN
                         FAILED ( "QUEUE NOT PROPERLY MAINTAINED " &
                                  "-- ORDER SHOULD BE 12 " &
                                  "-- ACTUAL VALUE IS " &
                                   INTEGER'IMAGE (QUEUE_ORDER) &
                                  " - (B)" );
                    END IF;
               ELSIF QENTRY'COUNT = 1 THEN
                    ACCEPT QENTRY (I : INTEGER) DO
                         IF I = 0 THEN 
                              FAILED ( "ALL TASKS EXCEPT ABORTED " &
                                       "REMOVED FROM QUEUE - (B)" );
                         ELSE
                              FAILED ( "TOO MANY TASKS REMOVED " &
                                       "FROM QUEUE - (B)" );
                         END IF;
                    END QENTRY;
               ELSE  
                    FAILED ( "ALL TASKS REMOVED FROM QUEUE - (B)" );
               END IF;

          END CALLED;
                                   
     BEGIN 
          NULL;          
     END;  -- (B).        

     DECLARE -- (C). ABORTED TASK AT END OF QUEUE.

          TASK ABORTED IS
               ENTRY RELEASE;
          END ABORTED;

          TASK T1;

          TASK T2 IS
               ENTRY RELEASE;
          END T2;

          TASK CALLED IS
               ENTRY QENTRY (I : INTEGER);
          END CALLED;

          TASK BODY ABORTED IS
          BEGIN
               ACCEPT RELEASE;
               CALLED.QENTRY (IDENT_INT (0));
          END ABORTED;

          TASK BODY T1 IS
          BEGIN
               CALLED.QENTRY (IDENT_INT (1));
          END T1;

          TASK BODY T2 IS
          BEGIN
               ACCEPT RELEASE;
               CALLED.QENTRY (IDENT_INT (2));
          END T2;

          TASK BODY CALLED IS
               QUEUE_ORDER : INTEGER := 0;
          BEGIN
                         
               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 1;
               END LOOP;               
                         
               T2.RELEASE;

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 2;
               END LOOP;               
               
               ABORTED.RELEASE;

               LOOP
                    DELAY 0.1;  -- TODO: acats-delay-deviation: before was 1.0
                    EXIT WHEN QENTRY'COUNT = 3;
               END LOOP;               
               
               ABORT ABORTED;     
          
               IF QENTRY'COUNT = 3 THEN
                    FAILED ( "ABORTED TASK NOT REMOVED FROM QUEUE " &
                             "- (C)" );
                    FOR J IN 1 .. 3 LOOP
                         ACCEPT QENTRY (I : INTEGER);
                    END LOOP;
               ELSIF QENTRY'COUNT = 2 THEN
                    FOR J IN 1 .. 2 LOOP
                         ACCEPT QENTRY (I : INTEGER) DO
                              QUEUE_ORDER := 10 * QUEUE_ORDER + I;
                         END QENTRY;
                    END LOOP;
                    IF QUEUE_ORDER /= IDENT_INT (12) THEN
                         FAILED ( "QUEUE NOT PROPERLY MAINTAINED " &
                                  "-- ORDER SHOULD BE 12 " &
                                  "-- ACTUAL VALUE IS " &
                                   INTEGER'IMAGE (QUEUE_ORDER) &
                                  " - (C)" );
                    END IF;
               ELSIF QENTRY'COUNT = 1 THEN
                    ACCEPT QENTRY (I : INTEGER) DO
                         IF I = 0 THEN 
                              FAILED ( "ALL TASKS EXCEPT ABORTED " &
                                       "REMOVED FROM QUEUE - (C)" );
                         ELSE
                              FAILED ( "TOO MANY TASKS REMOVED " &
                                       "FROM QUEUE - (C)" );
                         END IF;
                    END QENTRY;
               ELSE  
                    FAILED ( "ALL TASKS REMOVED FROM QUEUE - (C)" );
               END IF;

          END CALLED;
                                   
     BEGIN 
          NULL;          
     END;  -- (C).        


     RESULT;
END C9A008A;
