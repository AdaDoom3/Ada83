procedure TEST_ATTRIBUTE is
   type INT_ARRAY is array (1..10) of INTEGER;

   ARR : INT_ARRAY;

   FIRST_IDX : INTEGER;
   LAST_IDX : INTEGER;
   LEN : INTEGER;
   SIZE_VAL : INTEGER;
begin
   -- Array attributes
   FIRST_IDX := INT_ARRAY'FIRST;
   LAST_IDX := INT_ARRAY'LAST;
   LEN := INT_ARRAY'LENGTH;

   -- Size attribute
   SIZE_VAL := INTEGER'SIZE;
end TEST_ATTRIBUTE;
