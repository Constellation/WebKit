// Tests that NewArray (e.g. literal `[x, y]`) is decomposed in DFGConstantFoldingPhase
// into NewButterflyWithSize + NewArrayWithButterfly + PutByVals so it can be sunk
// by DFGObjectAllocationSinkingPhase. See dfg/DFGConstantFoldingPhase.cpp.

function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

function run(func, a) {
    let expected;
    for (let i = 0; i < testLoopCount; i++) {
        if (a == undefined)
            a = [1, 2];
        let res = func(a);
        if (i == 0)
            expected = res;
        assert(res, expected);
    }
}

// ArrayWithInt32 literal, sunk
{
    function test(s) {
        let a = [s[0], s[0] + 1];
        s[0] = a[0] + a[1];

        var q = { f: s[1] ? a : 42 }; // forces materialization on the truthy branch
        return s;
    }
    noInline(test);
    run(test);
}

// ArrayWithDouble literal, sunk
{
    function test(s) {
        let a = [s[0], s[1]];
        s[0] = a[0] + a[1];

        var q = { f: s[1] ? a : 42 };
        return s;
    }
    noInline(test);
    run(test, [0.1, 0.2]);
}

// ArrayWithContiguous literal, sunk
{
    function test(s) {
        let a = [s[0], { f: s[1] }];
        s[0] = a[0];

        var q = { f: s[1] ? a : 42 };
        return s;
    }
    noInline(test);
    run(test, [1, 2]);
}

// length read on sunk NewArray literal
{
    function test(s) {
        let a = [s[0], s[0] + 1, s[0] + 2];
        s[1] = a.length;
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}

// no-sink: prototype chain invalidation prevents the InBoundsSaneChain claim
{
    function test(s) {
        Array.prototype[2] = 99;
        let a = [s[0], s[0] + 1];
        a.length = 10; // escape via length grow
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}

// no-sink: array escapes via globalThis
{
    function test(s) {
        let a = [s[0], s[0] + 1];
        s[0] = a[0];
        globalThis.ref = a;
        return s;
    }
    noInline(test);
    run(test);
}
