WITH TEXT_IO; USE TEXT_IO;
PROCEDURE PACKED_SLICES_AND_ACTUALS IS
     FUNCTION IDENT (X : INTEGER) RETURN INTEGER IS BEGIN RETURN X; END IDENT;
     TYPE BITS IS ARRAY (1 .. 16) OF BOOLEAN;
     PRAGMA PACK (BITS);
     B : BITS := (1 | 4 | 9 .. 12 => TRUE, OTHERS => FALSE);
     C : BITS := (OTHERS => FALSE);
     PROCEDURE FLIP (X : IN OUT BOOLEAN) IS
     BEGIN X := NOT X; END FLIP;
     PROCEDURE SETB (X : OUT BOOLEAN) IS
     BEGIN X := TRUE; END SETB;
BEGIN
     -- slice assignment between packed arrays (misaligned phases)
     C (IDENT (2) .. IDENT (8)) := B (IDENT (9) .. IDENT (15));
     IF NOT (C (2) AND C (3) AND C (4) AND C (5)) THEN PUT_LINE ("FAIL S1"); END IF;
     IF C (1) OR C (6) OR C (7) OR C (8) THEN PUT_LINE ("FAIL S2"); END IF;
     -- slice rvalue compared against aggregate-typed slice
     IF C (IDENT (2) .. 8) /= B (9 .. IDENT (15)) THEN PUT_LINE ("FAIL S3"); END IF;
     -- OUT / IN OUT packed element actuals
     FLIP (B (IDENT (1)));
     IF B (1) THEN PUT_LINE ("FAIL O1"); END IF;
     FLIP (B (2));
     IF NOT B (2) THEN PUT_LINE ("FAIL O2"); END IF;
     SETB (C (IDENT (16)));
     IF NOT C (16) THEN PUT_LINE ("FAIL O3"); END IF;
     IF B (3) OR B (5) THEN PUT_LINE ("FAIL O4"); END IF;
     PUT_LINE ("DONE");
END PACKED_SLICES_AND_ACTUALS;
