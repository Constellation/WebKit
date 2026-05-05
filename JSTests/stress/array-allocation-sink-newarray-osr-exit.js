// Tests OSR exit through a sunk NewArray literal: when speculation fails after the
// array is sunk, MaterializeNewArrayWithButterfly must reconstruct the array with
// the correct values. Exercises the path through the decomposition added in
// dfg/DFGConstantFoldingPhase.cpp.

function assert(cond, msg) {
    if (!cond) throw new Error("assertion failed: " + msg);
}

// On the warm path the array doesn't escape and is sunk. After warmup we pass a
// String for `y` which makes the addition flow into a non-Int32 path; the GetByVal
// after the sunk array gets materialized, and we read both elements out.
{
    function f(x, y, escape) {
        var a = [x, y];
        if (escape)
            return a; // forces MaterializeNewArrayWithButterfly on this branch
        return a[0] + a[1];
    }
    noInline(f);

    for (var i = 0; i < testLoopCount; ++i) {
        var r = f(i, i + 1, false);
        assert(r === 2 * i + 1, "warm: i=" + i + " got " + r);
    }

    // Force the escape path: the materialized array must have the live values.
    var arr = f(100, 200, true);
    assert(arr.length === 2, "len=" + arr.length);
    assert(arr[0] === 100 && arr[1] === 200, "values=" + arr[0] + "," + arr[1]);
}

// OSR exit driven by a value type change. After warmup, pass a value that violates
// the previous Int32 prediction. The materialized array should still hold the new
// values when we OSR-exit and rerun in baseline.
{
    function g(x, y) {
        var a = [x, y];
        return a[0] + a[1];
    }
    noInline(g);

    for (var i = 0; i < testLoopCount; ++i) {
        var r = g(i, i + 1);
        assert(r === 2 * i + 1, "g warm: i=" + i + " got " + r);
    }

    // Trigger an OSR exit by mixing in a string.
    var r2 = g("a", "b");
    assert(r2 === "ab", "g string: " + r2);
}
