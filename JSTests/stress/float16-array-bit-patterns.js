function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldBeCloseTo(actual, expected, epsilon) {
    if (Math.abs(actual - expected) >= epsilon)
        throw new Error("bad value: " + actual + " expected close to: " + expected + " (epsilon=" + epsilon + ")");
}

// Float16 representation details:
// Sign: 1 bit, Exponent: 5 bits (bias 15), Mantissa: 10 bits
// Max finite: 65504, Min subnormal: ~5.96e-8, Min normal: ~6.10e-5

// Test max finite value.
var a = new Float16Array(1);
a[0] = 65504;
shouldBe(a[0], 65504);

// Values just above max finite overflow to Infinity.
a[0] = 65520;
shouldBe(a[0], Infinity);

// Test min normal value.
a[0] = 6.103515625e-5; // 2^-14 exactly
shouldBe(a[0], 6.103515625e-5);

// Test subnormal values.
a[0] = 5.960464477539063e-8; // smallest positive subnormal: 2^-24
shouldBe(a[0] > 0, true);
shouldBe(a[0], 5.960464477539063e-8);

// Values smaller than smallest subnormal become zero.
a[0] = 1e-9;
shouldBe(a[0], 0);

// Test all NaN encodings result in NaN.
a[0] = NaN;
shouldBe(isNaN(a[0]), true);

// Test negative zero.
a[0] = -0;
shouldBe(a[0], 0);
shouldBe(1 / a[0], -Infinity); // confirm negative zero

// Test positive zero.
a[0] = 0;
shouldBe(a[0], 0);
shouldBe(1 / a[0], Infinity); // confirm positive zero

// Test specific precision: 1.0 + 2^-10 should be representable exactly.
a[0] = 1.0 + 1 / 1024;
shouldBe(a[0], 1.0 + 1 / 1024);

// Test that 1.0 + 2^-11 rounds to 1.0 (below ULP at this exponent).
a[0] = 1.0 + 1 / 2048;
shouldBe(a[0], 1.0);

// Test multiple values in a larger array for consistent bit patterns via shared buffer.
var f16 = new Float16Array(4);
var u16 = new Uint16Array(f16.buffer);

f16[0] = 1.0;
shouldBe(u16[0], 0x3C00); // float16 encoding of 1.0

f16[1] = -1.0;
shouldBe(u16[1], 0xBC00); // float16 encoding of -1.0

f16[2] = 0.5;
shouldBe(u16[2], 0x3800); // float16 encoding of 0.5

f16[3] = Infinity;
shouldBe(u16[3], 0x7C00); // float16 encoding of +Infinity

// Test that shared buffer writes from Uint16 side produce correct Float16 reads.
u16[0] = 0x4000; // float16 encoding of 2.0
shouldBe(f16[0], 2.0);

u16[1] = 0x7E00; // a NaN encoding
shouldBe(isNaN(f16[1]), true);

u16[2] = 0x0000; // +0
shouldBe(f16[2], 0);
shouldBe(1 / f16[2], Infinity);

u16[3] = 0x8000; // -0
shouldBe(f16[3], 0);
shouldBe(1 / f16[3], -Infinity);

// Run JIT-heavy loop exercising bit pattern round-trips.
function bitPatternRoundTrip(halfBits) {
    var u = new Uint16Array(1);
    var f = new Float16Array(u.buffer);
    u[0] = halfBits;
    var val = f[0];
    f[0] = val;
    return u[0];
}
noInline(bitPatternRoundTrip);

for (var i = 0; i < testLoopCount; ++i) {
    // 0x3C00 = 1.0, round-trip should preserve.
    shouldBe(bitPatternRoundTrip(0x3C00), 0x3C00);
    // 0x3800 = 0.5, round-trip should preserve.
    shouldBe(bitPatternRoundTrip(0x3800), 0x3800);
}
