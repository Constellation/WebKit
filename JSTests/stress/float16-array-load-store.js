function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldBeCloseTo(actual, expected, epsilon) {
    if (Math.abs(actual - expected) >= epsilon)
        throw new Error("bad value: " + actual + " expected close to: " + expected + " (epsilon=" + epsilon + ")");
}

// Test basic Float16Array store and load round-trip.
// Run enough iterations to trigger JIT compilation.
function testStoreLoad(value) {
    var a = new Float16Array(1);
    a[0] = value;
    return a[0];
}
noInline(testStoreLoad);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBeCloseTo(testStoreLoad(1.5), 1.5, 1e-4);
    shouldBeCloseTo(testStoreLoad(0.0), 0.0, 1e-8);
    shouldBeCloseTo(testStoreLoad(-3.14), -3.140625, 0.01);
    shouldBeCloseTo(testStoreLoad(65504), 65504, 1e-4); // max finite float16
}

// Special values
shouldBe(testStoreLoad(Infinity), Infinity);
shouldBe(testStoreLoad(-Infinity), -Infinity);
shouldBe(isNaN(testStoreLoad(NaN)), true);
shouldBe(testStoreLoad(0), 0);
shouldBe(1 / testStoreLoad(-0), -Infinity); // negative zero preserved

// Overflow to infinity
shouldBe(testStoreLoad(65536), Infinity);
shouldBe(testStoreLoad(-65536), -Infinity);

// Denormals (smallest positive float16 subnormal: 2^-24 ~ 5.96e-8)
shouldBe(testStoreLoad(5.96e-8) > 0, true);
shouldBe(testStoreLoad(1e-9), 0); // underflow to zero

// Test indexed access with BaseIndex addressing.
function testBaseIndex(arr, index) {
    return arr[index];
}
noInline(testBaseIndex);

var arr = new Float16Array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
for (var i = 0; i < testLoopCount; ++i) {
    shouldBeCloseTo(testBaseIndex(arr, i % 8), (i % 8) + 1.0, 1e-4);
}

// Test indexed store with BaseIndex addressing.
function testBaseIndexStore(arr, index, value) {
    arr[index] = value;
}
noInline(testBaseIndexStore);

var arr2 = new Float16Array(8);
for (var i = 0; i < testLoopCount; ++i) {
    testBaseIndexStore(arr2, i % 8, (i % 8) + 0.5);
}
for (var i = 0; i < 8; ++i)
    shouldBeCloseTo(arr2[i], i + 0.5, 1e-3);
