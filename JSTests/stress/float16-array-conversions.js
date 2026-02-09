function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldBeCloseTo(actual, expected, epsilon) {
    if (Math.abs(actual - expected) >= epsilon)
        throw new Error("bad value: " + actual + " expected close to: " + expected + " (epsilon=" + epsilon + ")");
}

// Test Float16Array to Float32Array conversion.
function testFloat16ToFloat32() {
    var f16 = new Float16Array([0.1, 0.5, 1.0, 2.0, 100.0]);
    var f32 = new Float32Array(f16);
    for (var i = 0; i < f16.length; ++i)
        shouldBe(f16[i], f32[i]);
}

// Test Float16Array to Float64Array conversion.
function testFloat16ToFloat64() {
    var f16 = new Float16Array([0.1, 0.5, 1.0, 2.0, 100.0]);
    var f64 = new Float64Array(f16);
    for (var i = 0; i < f16.length; ++i)
        shouldBe(f16[i], f64[i]);
}

// Test Float64Array to Float16Array conversion.
function testFloat64ToFloat16() {
    var f64 = new Float64Array([0.1, 0.5, 1.0, 2.0, 100.0]);
    var f16 = new Float16Array(f64);
    shouldBeCloseTo(f16[0], 0.1, 1e-3);
    shouldBe(f16[1], 0.5);
    shouldBe(f16[2], 1.0);
    shouldBe(f16[3], 2.0);
    shouldBe(f16[4], 100.0);
}

// Test Float32Array to Float16Array conversion.
function testFloat32ToFloat16() {
    var f32 = new Float32Array([0.1, 0.5, 1.0, 2.0, 100.0]);
    var f16 = new Float16Array(f32);
    shouldBeCloseTo(f16[0], 0.1, 1e-3);
    shouldBe(f16[1], 0.5);
    shouldBe(f16[2], 1.0);
    shouldBe(f16[3], 2.0);
    shouldBe(f16[4], 100.0);
}

// Run enough iterations to hit JIT tiers.
for (var i = 0; i < testLoopCount; ++i) {
    testFloat16ToFloat32();
    testFloat16ToFloat64();
    testFloat64ToFloat16();
    testFloat32ToFloat16();
}

// Test construction from array of values with precision loss.
var f16 = new Float16Array([Math.PI, Math.E, Math.SQRT2]);
shouldBeCloseTo(f16[0], Math.PI, 0.002);
shouldBeCloseTo(f16[1], Math.E, 0.002);
shouldBeCloseTo(f16[2], Math.SQRT2, 0.001);

// Verify that the round-trip through Float16Array produces the same bit pattern.
var a = new Float16Array(1);
var b = new Float16Array(1);
a[0] = 1.234;
b[0] = a[0];
shouldBe(a[0], b[0]);
