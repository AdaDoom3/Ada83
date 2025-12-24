-- IMPDEF.ADS
--
-- Implementation-defined constants for ACATS testing
-- This package provides implementation-specific values required by ACATS tests

PACKAGE IMPDEF IS

   -- File name for tests that create/manipulate files
   LEGAL_FILE_NAME : CONSTANT STRING := "test_file.tmp";

   -- Second file name for tests that need multiple files
   LEGAL_FILE_NAME_2 : CONSTANT STRING := "test_file_2.tmp";

   -- Indicator for tests that don't apply to this implementation
   NOT_APPLICABLE : CONSTANT STRING := "N/A FOR THIS IMPLEMENTATION";

   -- Maximum line length for text I/O
   MAX_IN_LEN : CONSTANT INTEGER := 1000;

   -- Maximum digits for decimal types
   MAX_DIGITS : CONSTANT INTEGER := 15;

   -- Task-related constants (for tasking tests)
   TASK_STORAGE_SIZE : CONSTANT INTEGER := 10000;

   -- Address-related constant
   VARIABLE_ADDRESS : CONSTANT INTEGER := 0;

END IMPDEF;
