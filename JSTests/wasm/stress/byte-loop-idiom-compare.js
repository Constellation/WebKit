// Exercises the recognition of loops that compare two ranges of linear memory one byte per
// iteration, which OMG compiles as a word-at-a-time bulk scan instead. The bulk form is only
// faithful to the loop when both ranges are in bounds, so every case here is checked against a
// JavaScript model of the loop itself: the result is the exact difference of the first
// mismatching byte pair, the loop leaves its pointer, counter and byte locals at specific
// values, and a compare that would read off the end of memory traps.

import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const PAGE = 65536;

// Every case in the matrix below touches memory inside this window, which leaves room on both
// sides for a bulk operation that read outside the loop's range to show up as a difference.
const WINDOW_BEGIN = 512;
const WINDOW_END = 3072;

let wat = `
(module
    (memory (export "mem") 1)
    (global $a (mut i32) (i32.const 0))
    (global $b (mut i32) (i32.const 0))
    (global $n (mut i32) (i32.const 0))

    ;; result = 0; while (n && mem[a] == mem[b]) { ++a; ++b; --n; } if (n) result = mem[a] - mem[b];
    ;; spelled as the loop a toolchain without a library memcmp emits.
    (func (export "compare") (param i32 i32 i32) (result i32)
        (local i32 i32 i32)
        block
            local.get 2
            i32.eqz
            br_if 0
            loop
                local.get 0
                i32.load8_u
                local.tee 4
                local.get 1
                i32.load8_u
                local.tee 5
                i32.eq
                if
                    local.get 1
                    i32.const 1
                    i32.add
                    local.set 1
                    local.get 0
                    i32.const 1
                    i32.add
                    local.set 0
                    local.get 2
                    i32.const 1
                    i32.sub
                    local.tee 2
                    br_if 1
                    br 2
                end
            end
            local.get 4
            local.get 5
            i32.sub
            local.set 3
        end
        local.get 0
        global.set $a
        local.get 1
        global.set $b
        local.get 2
        global.set $n
        local.get 3)

    (func (export "readA") (result i32) global.get $a)
    (func (export "readB") (result i32) global.get $b)
    (func (export "readN") (result i32) global.get $n)

    ;; The same loop with the count fixed at 40, which OMG's B3 strength reduction turns into a
    ;; branchless word-at-a-time scan instead of a call.
    (func (export "compare40") (param i32 i32) (result i32)
        (local i32 i32 i32 i32)
        i32.const 40
        local.set 2
        block
            loop
                local.get 0
                i32.load8_u
                local.tee 3
                local.get 1
                i32.load8_u
                local.tee 4
                i32.eq
                if
                    local.get 1
                    i32.const 1
                    i32.add
                    local.set 1
                    local.get 0
                    i32.const 1
                    i32.add
                    local.set 0
                    local.get 2
                    i32.const 1
                    i32.sub
                    local.tee 2
                    br_if 1
                    br 2
                end
            end
            local.get 3
            local.get 4
            i32.sub
            local.set 5
        end
        local.get 0
        global.set $a
        local.get 1
        global.set $b
        local.get 2
        global.set $n
        local.get 5)
)
`;

// The model throws where the wasm loop would trap, so the same function serves both the in-bounds
// cases and the out-of-bounds ones.
function access(i)
{
    if (i < 0 || i >= PAGE)
        throw new RangeError("out of bounds");
    return i;
}

function modelCompare(memory, a, b, n)
{
    let result = 0;
    if (n) {
        for (;;) {
            const ca = memory[access(a)];
            const cb = memory[access(b)];
            if (ca !== cb) {
                result = ca - cb;
                break;
            }
            a = (a + 1) | 0;
            b = (b + 1) | 0;
            n = (n - 1) | 0;
            if (!n)
                break;
        }
    }
    return [result, a, b, n];
}

// Lengths bracket the sizes at which a bulk scan switches between 64-bit words and byte checks,
// and the mismatch positions cover no mismatch, and a mismatch at either end or the middle.
const lengths = [1, 2, 3, 7, 8, 15, 16, 17, 31, 63, 127, 128, 129, 300];
const mismatchAt = [-1, 0, 1, 5, 13, 62];

async function test()
{
    const instance = await instantiate(wat, {}, {});
    const { compare, compare40, readA, readB, readN, mem } = instance.exports;
    const memory = new Uint8Array(mem.buffer);
    const expected = new Uint8Array(PAGE);

    function seedWindow()
    {
        for (let i = WINDOW_BEGIN; i < WINDOW_END; ++i)
            memory[i] = expected[i] = (i * 31 + 7) & 0xff;
    }

    function seedEverything()
    {
        for (let i = 0; i < PAGE; ++i)
            memory[i] = expected[i] = (i * 31 + 7) & 0xff;
    }

    function assertMemoryMatches(begin, end)
    {
        let i = begin;
        while (i < end && memory[i] === expected[i])
            ++i;
        assert.eq(i, end, "memory differs at index " + i);
    }

    function check(a, b, n)
    {
        seedWindow();
        assert.eq(compare(a, b, n) | 0, modelCompare(expected, a, b, n)[0]);
        assert.eq([readA(), readB(), readN()], modelCompare(expected, a, b, n).slice(1));
        assertMemoryMatches(WINDOW_BEGIN, WINDOW_END);
    }

    function checkEveryCase()
    {
        for (const n of lengths) {
            for (const mismatch of mismatchAt) {
                if (mismatch >= n)
                    continue;
                // The same bytes on both sides, possibly differing at one position.
                for (let i = 0; i < n; ++i)
                    expected[2000 + i] = expected[1000 + i] = (i * 13 + 5) & 0xff;
                if (mismatch >= 0)
                    expected[2000 + mismatch] ^= 0x5a;
                check(1000, 2000, n);
            }
            // Identical ranges and fully overlapping ranges always match.
            check(1000, 1000, n);
            check(1000, 1001, n);
        }
        // A zero count never reads anything and leaves everything alone.
        check(1000, 2000, 0);
    }

    // The fixed-count variant, checked against the same model with the count it hardcodes.
    function check40()
    {
        for (const mismatch of [-1, 0, 1, 13, 39]) {
            for (let i = 0; i < 40; ++i)
                expected[2000 + i] = expected[1000 + i] = (i * 13 + 5) & 0xff;
            if (mismatch >= 0)
                expected[2000 + mismatch] ^= 0x5a;
            seedWindow();
            assert.eq(compare40(1000, 2000) | 0, modelCompare(expected, 1000, 2000, 40)[0]);
            assert.eq([readA(), readB(), readN()], modelCompare(expected, 1000, 2000, 40).slice(1));
        }
    }

    // Check every case below OMG, then warm the function up until OMG compiles it and check every
    // case again there, since only OMG replaces the loop with a bulk operation.
    for (let repeat = 0; repeat < 2; ++repeat) {
        checkEveryCase();
        check40();
        for (let i = 0; i < wasmTestLoopCount; ++i) {
            compare(1000, 2000, 64);
            compare40(1000, 2000);
        }
    }
    checkEveryCase();
    check40();

    // A compare that would read off the end of memory must trap once the matching prefix reaches
    // the boundary, exactly like the byte loop.
    function assertTraps(a, b, n)
    {
        seedEverything();
        for (let i = 0; i < 64; ++i)
            memory[i + PAGE - 64] = expected[i + PAGE - 64] = 0x42;
        assert.throws(() => modelCompare(expected, a, b, n), RangeError, "out of bounds");
        assert.throws(() => compare(a, b, n), WebAssembly.RuntimeError, "Out of bounds memory access");
    }
    assertTraps(PAGE - 10, PAGE - 20, 40);
    assertTraps(PAGE - 20, PAGE - 10, 40);

    // Right up against the end of memory is in bounds and must not trap.
    seedEverything();
    for (let i = 0; i < 64; ++i)
        memory[i + PAGE - 64] = expected[i + PAGE - 64] = 0x42;
    assert.eq(compare(PAGE - 40, PAGE - 64, 24), 0);
    assert.eq([readA(), readB(), readN()], modelCompare(expected, PAGE - 40, PAGE - 64, 24).slice(1));
}

await assert.asyncTest(test());
