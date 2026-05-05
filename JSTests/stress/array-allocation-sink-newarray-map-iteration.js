// Motivating case for the NewArray decomposition in dfg/DFGConstantFoldingPhase.cpp:
// `for (const [k, v] of map)` produces a per-iteration `[k, v]` array via NewArray.
// With the decomposition in place that array can be sunk by DFGObjectAllocationSinkingPhase.

function assert(cond, msg) {
    if (!cond) throw new Error("assertion failed: " + msg);
}

var m = new Map();
var expected = 0;
for (var i = 0; i < 256; ++i) {
    m.set(i, i * 2);
    expected += i + i * 2;
}

function sumMapEntries(map) {
    var s = 0;
    for (const [k, v] of map)
        s += k + v;
    return s;
}
noInline(sumMapEntries);

for (var i = 0; i < testLoopCount; ++i) {
    var r = sumMapEntries(m);
    assert(r === expected, "iter " + i + ": got " + r + " expected " + expected);
}

// Same pattern via Map.prototype.entries()
function sumMapEntriesIt(map) {
    var s = 0;
    for (const [k, v] of map.entries())
        s += k + v;
    return s;
}
noInline(sumMapEntriesIt);

for (var i = 0; i < testLoopCount; ++i) {
    var r = sumMapEntriesIt(m);
    assert(r === expected, "entries() iter " + i + ": got " + r + " expected " + expected);
}
