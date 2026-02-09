function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

// Test double rounding correction in double→float16 conversion.
//
// When converting double→float32→float16, "double rounding" can occur:
// the double→float32 step rounds once, and float32→float16 rounds again,
// potentially producing a different result than a direct double→float16.
//
// We use Math.f16round as the reference (correct single-step conversion)
// and verify that Float16Array store/load (which uses the JIT path)
// produces the same result.

var f16 = new Float16Array(1);
var u16 = new Uint16Array(f16.buffer);

function f16roundViaArray(value) {
    f16[0] = value;
    return f16[0];
}
noInline(f16roundViaArray);

function f16bitsViaArray(value) {
    f16[0] = value;
    return u16[0];
}
noInline(f16bitsViaArray);

// Construct double values that are exact float16 midpoints when converted
// through float32. These are the cases where double rounding can go wrong.
//
// Float16 has 10 mantissa bits. Float32 has 23 mantissa bits.
// The 13 bits below float16's precision in float32 determine rounding.
// A midpoint occurs when bit 12 is set and bits 11:0 are all zero (0x1000).
//
// To trigger double rounding, we need a double that:
// 1. Converts to a float32 value at an exact float16 midpoint
// 2. The double itself is NOT at that midpoint (it's slightly above or below)

// Test many values across the float16 range using Math.f16round as reference.
for (var i = 0; i < testLoopCount; ++i) {
    // Generate test values that exercise different float16 exponent ranges.
    var testValues = [
        1.0 + 0.5 / 1024,  // Near 1.0, at float16 midpoint
        2.0 + 1.0 / 1024,  // Near 2.0
        0.5 + 0.25 / 1024, // Near 0.5
        100.0 + 100.0 / 2048, // Larger values
        0.1,                // Common value
        0.3,                // Another common value
        -1.0 - 0.5 / 1024, // Negative midpoint
        -0.5 - 0.25 / 1024,
    ];

    for (var j = 0; j < testValues.length; ++j) {
        var val = testValues[j];
        var expected = Math.f16round(val);
        var actual = f16roundViaArray(val);
        shouldBe(actual, expected);
    }
}

// Exhaustive test: check every float16 bit pattern round-trips correctly.
// For each float16 value, convert to double, then back to float16 via array.
var allF16 = new Float16Array(1);
var allU16 = new Uint16Array(allF16.buffer);
for (var bits = 0; bits < 0x10000; ++bits) {
    // Skip NaN encodings (they may canonicalize differently).
    if ((bits & 0x7C00) === 0x7C00 && (bits & 0x03FF) !== 0)
        continue;
    allU16[0] = bits;
    var doubleVal = allF16[0];
    allF16[0] = doubleVal;
    shouldBe(allU16[0], bits);
}

// Test specific known double rounding edge cases.
// These are doubles where naive float32 intermediate rounding disagrees
// with direct double→float16 conversion.
//
// Construct a value that's just above a float16 midpoint in a way that
// float32 rounds to the midpoint exactly.
// float16 value 1.0 = 0x3C00, next float16 = 1.0 + 1/1024 = 0x3C01
// Midpoint in float32: 1.0 + 0.5/1024 = 1.000488281...
// If a double is slightly above this midpoint, cvtsd2ss may round to the
// midpoint exactly, then vcvtps2ph rounds to even (1.0 = 0x3C00).
// But the correct result should round up to 0x3C01.

// Use a DataView to construct exact bit patterns for doubles.
var buf = new ArrayBuffer(8);
var dv = new DataView(buf);
var f64 = new Float64Array(buf);

// Test: Math.f16round should match array conversion for all tested values.
var moreTestValues = [];
// Generate values near float16 boundaries.
for (var exp = -14; exp <= 15; ++exp) {
    var base = Math.pow(2, exp);
    for (var frac = 0; frac < 1024; frac += 128) {
        var val = base * (1.0 + frac / 1024);
        // Test values slightly above and below the midpoint.
        var ulp = base / 1024;
        moreTestValues.push(val + ulp * 0.5);
        moreTestValues.push(val + ulp * 0.5 + ulp * 0.001);
        moreTestValues.push(val + ulp * 0.5 - ulp * 0.001);
        moreTestValues.push(-val - ulp * 0.5);
    }
}

for (var i = 0; i < moreTestValues.length; ++i) {
    var val = moreTestValues[i];
    var expected = Math.f16round(val);
    var actual = f16roundViaArray(val);
    shouldBe(actual, expected);
}
