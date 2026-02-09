function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldBeCloseTo(actual, expected, epsilon) {
    if (Math.abs(actual - expected) >= epsilon)
        throw new Error("bad value: " + actual + " expected close to: " + expected + " (epsilon=" + epsilon + ")");
}

// Test fill operation on Float16Array.
var arr = new Float16Array(8);
arr.fill(2.5);
for (var i = 0; i < arr.length; ++i)
    shouldBe(arr[i], 2.5);

arr.fill(0);
for (var i = 0; i < arr.length; ++i)
    shouldBe(arr[i], 0);

arr.fill(-1.5, 2, 6);
shouldBe(arr[0], 0);
shouldBe(arr[1], 0);
shouldBe(arr[2], -1.5);
shouldBe(arr[5], -1.5);
shouldBe(arr[6], 0);
shouldBe(arr[7], 0);

// Test copyWithin on Float16Array.
var src = new Float16Array([1, 2, 3, 4, 5]);
src.copyWithin(0, 3);
shouldBe(src[0], 4);
shouldBe(src[1], 5);
shouldBe(src[2], 3);

// Test slice on Float16Array.
var original = new Float16Array([10, 20, 30, 40, 50]);
var sliced = original.slice(1, 4);
shouldBe(sliced.length, 3);
shouldBe(sliced[0], 20);
shouldBe(sliced[1], 30);
shouldBe(sliced[2], 40);
shouldBe(sliced instanceof Float16Array, true);

// Test subarray on Float16Array.
var sub = original.subarray(2, 4);
shouldBe(sub.length, 2);
shouldBe(sub[0], 30);
shouldBe(sub[1], 40);
shouldBe(sub instanceof Float16Array, true);
shouldBe(sub.buffer, original.buffer);

// Test set on Float16Array.
var target = new Float16Array(5);
target.set([1.5, 2.5, 3.5]);
shouldBe(target[0], 1.5);
shouldBe(target[1], 2.5);
shouldBe(target[2], 3.5);
shouldBe(target[3], 0);
shouldBe(target[4], 0);

var src2 = new Float16Array([10, 20]);
target.set(src2, 3);
shouldBe(target[3], 10);
shouldBe(target[4], 20);

// Test map on Float16Array.
var mapped = new Float16Array([1, 2, 3, 4]).map(function(x) { return x * 2; });
shouldBe(mapped instanceof Float16Array, true);
shouldBe(mapped[0], 2);
shouldBe(mapped[1], 4);
shouldBe(mapped[2], 6);
shouldBe(mapped[3], 8);

// Test filter on Float16Array.
var filtered = new Float16Array([1, 2, 3, 4, 5]).filter(function(x) { return x > 3; });
shouldBe(filtered instanceof Float16Array, true);
shouldBe(filtered.length, 2);
shouldBe(filtered[0], 4);
shouldBe(filtered[1], 5);

// Test reduce on Float16Array.
var sum = new Float16Array([1, 2, 3, 4]).reduce(function(acc, x) { return acc + x; }, 0);
shouldBeCloseTo(sum, 10, 1e-4);

// Test sort on Float16Array.
var sorted = new Float16Array([3, 1, 4, 1, 5, 9, 2, 6]);
sorted.sort();
shouldBe(sorted[0], 1);
shouldBe(sorted[1], 1);
shouldBe(sorted[2], 2);
shouldBe(sorted[3], 3);
shouldBe(sorted[4], 4);
shouldBe(sorted[5], 5);
shouldBe(sorted[6], 6);
shouldBe(sorted[7], 9);

// Test reverse on Float16Array.
var reversed = new Float16Array([1, 2, 3]);
reversed.reverse();
shouldBe(reversed[0], 3);
shouldBe(reversed[1], 2);
shouldBe(reversed[2], 1);

// Test indexOf / lastIndexOf on Float16Array.
var searchArr = new Float16Array([1, 2, 3, 2, 1]);
shouldBe(searchArr.indexOf(2), 1);
shouldBe(searchArr.lastIndexOf(2), 3);
shouldBe(searchArr.indexOf(99), -1);

// Test includes on Float16Array.
shouldBe(searchArr.includes(3), true);
shouldBe(searchArr.includes(99), false);

// Test find / findIndex on Float16Array.
shouldBe(searchArr.find(function(x) { return x > 2; }), 3);
shouldBe(searchArr.findIndex(function(x) { return x > 2; }), 2);

// Test every / some on Float16Array.
shouldBe(new Float16Array([2, 4, 6]).every(function(x) { return x % 2 === 0; }), true);
shouldBe(new Float16Array([1, 2, 3]).some(function(x) { return x > 2; }), true);
shouldBe(new Float16Array([1, 2, 3]).every(function(x) { return x > 2; }), false);

// Test forEach on Float16Array.
var forEachResult = 0;
new Float16Array([1, 2, 3]).forEach(function(x) { forEachResult += x; });
shouldBeCloseTo(forEachResult, 6, 1e-4);
