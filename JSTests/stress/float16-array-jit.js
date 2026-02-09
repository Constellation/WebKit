function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldBeCloseTo(actual, expected, epsilon) {
    if (Math.abs(actual - expected) >= epsilon)
        throw new Error("bad value: " + actual + " expected close to: " + expected + " (epsilon=" + epsilon + ")");
}

// Test Float16Array load in a JIT-compiled loop with a fixed address pattern.
function testFixedLoad() {
    var arr = new Float16Array([1.0, 2.0, 3.0, 4.0]);
    var sum = 0;
    for (var i = 0; i < arr.length; ++i)
        sum += arr[i];
    return sum;
}
noInline(testFixedLoad);

for (var i = 0; i < testLoopCount; ++i)
    shouldBeCloseTo(testFixedLoad(), 10.0, 1e-4);

// Test Float16Array store in a JIT-compiled loop.
function testFixedStore(value) {
    var arr = new Float16Array(4);
    for (var i = 0; i < arr.length; ++i)
        arr[i] = value;
    return arr[0] + arr[1] + arr[2] + arr[3];
}
noInline(testFixedStore);

for (var i = 0; i < testLoopCount; ++i)
    shouldBeCloseTo(testFixedStore(2.5), 10.0, 1e-3);

// Test with mixed access patterns to ensure JIT doesn't incorrectly specialize.
function testMixedAccess(arr, index) {
    arr[index] = arr[index] + 1.0;
    return arr[index];
}
noInline(testMixedAccess);

var mixed = new Float16Array(4);
mixed[0] = 0;
mixed[1] = 10;
mixed[2] = 20;
mixed[3] = 30;

for (var i = 0; i < testLoopCount; ++i) {
    var idx = i % 4;
    testMixedAccess(mixed, idx);
}

// After many increments, values should have accumulated.
// Each index is incremented 2500 times.
for (var i = 0; i < 4; ++i)
    shouldBe(mixed[i] > i * 10, true);

// Test that Float16Array works correctly with large arrays.
var large = new Float16Array(1024);
for (var i = 0; i < 1024; ++i)
    large[i] = i;
for (var i = 0; i < 1024; ++i) {
    // Float16 can represent integers exactly up to 2048.
    if (i <= 2048)
        shouldBe(large[i], i);
}

// Test Float16Array with typed array set from Float64Array (exercises double→float16 conversion).
var f64src = new Float64Array(8);
for (var i = 0; i < 8; ++i)
    f64src[i] = i * 0.1;

var f16dst = new Float16Array(8);
f16dst.set(f64src);
for (var i = 0; i < 8; ++i)
    shouldBeCloseTo(f16dst[i], i * 0.1, 1e-3);

// Test Float16Array with typed array set from Int32Array.
var i32src = new Int32Array([0, 1, 2, 3, 100, -100, 1000, -1000]);
var f16fromInt = new Float16Array(8);
f16fromInt.set(i32src);
shouldBe(f16fromInt[0], 0);
shouldBe(f16fromInt[1], 1);
shouldBe(f16fromInt[2], 2);
shouldBe(f16fromInt[3], 3);
shouldBe(f16fromInt[4], 100);
shouldBe(f16fromInt[5], -100);
shouldBe(f16fromInt[6], 1000);
shouldBe(f16fromInt[7], -1000);
