-- Test Big_Integer and Big_Real types
procedure Big_Numbers is
    -- Big integer variables
    A : BIG_INTEGER;
    B : BIG_INTEGER;
    C : BIG_INTEGER;

    -- Big real variables
    X : BIG_REAL;
    Y : BIG_REAL;
    Z : BIG_REAL;

    -- For conversion tests
    I : INTEGER;
    F : FLOAT;

    -- For string conversion
    S : STRING(1..100);
begin
    -- Test big integer operations
    A := BIG_INTEGER(42);
    B := BIG_INTEGER(100);
    C := A + B;
    C := A - B;
    C := A * B;
    C := A / B;
    C := A mod B;
    C := A rem B;
    C := A ** 3;
    C := -A;
    C := abs A;

    -- Test big integer comparisons
    if A = B then
        null;
    end if;
    if A /= B then
        null;
    end if;
    if A < B then
        null;
    end if;
    if A <= B then
        null;
    end if;
    if A > B then
        null;
    end if;
    if A >= B then
        null;
    end if;

    -- Test big real operations
    X := BIG_REAL(3.14159);
    Y := BIG_REAL(2.71828);
    Z := X + Y;
    Z := X - Y;
    Z := X * Y;
    Z := X / Y;
    Z := X ** 2;
    Z := -X;
    Z := abs X;

    -- Test big real comparisons
    if X = Y then
        null;
    end if;
    if X /= Y then
        null;
    end if;
    if X < Y then
        null;
    end if;
    if X <= Y then
        null;
    end if;
    if X > Y then
        null;
    end if;
    if X >= Y then
        null;
    end if;

    -- Test conversions
    I := INTEGER(A);
    A := BIG_INTEGER(I);
    F := FLOAT(X);
    X := BIG_REAL(F);

    -- Test IMAGE attribute
    S := BIG_INTEGER'IMAGE(A);
    S := BIG_REAL'IMAGE(X);

    -- Test VALUE attribute
    A := BIG_INTEGER'VALUE("12345678901234567890");
    X := BIG_REAL'VALUE("3.141592653589793238462643383279");
end Big_Numbers;
