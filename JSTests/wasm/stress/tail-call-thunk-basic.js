//@ requireOptions("--useWasmTailCalls=true", "--useBBQJIT=false", "--useOMGJIT=false")
// Tests basic tail call thunk mechanism with IPInt only
// A tail call should insert a thunk frame that restores instance state when the chain returns.

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test basic tail call within same instance
let basicWat = `
(module
    ;; Function that returns its argument + 42
    (func $add42 (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.const 42))
    )

    ;; Function that tail calls add42
    (func (export "test") (param $x i32) (result i32)
        (return_call $add42 (local.get $x))
    )
)
`

// Test chain of tail calls - should reuse the same thunk frame
let chainWat = `
(module
    ;; Base case: returns value + 1
    (func $base (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.const 1))
    )

    ;; Step 1: adds 1 and tail calls base
    (func $step1 (param $x i32) (result i32)
        (return_call $base (i32.add (local.get $x) (i32.const 1)))
    )

    ;; Step 2: adds 1 and tail calls step1
    (func $step2 (param $x i32) (result i32)
        (return_call $step1 (i32.add (local.get $x) (i32.const 1)))
    )

    ;; Entry point: adds 1 and tail calls step2
    (func (export "test") (param $x i32) (result i32)
        (return_call $step2 (i32.add (local.get $x) (i32.const 1)))
    )
)
`

// Test that normal call followed by tail call creates proper thunk
let mixedWat = `
(module
    (func $tail_target (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.const 100))
    )

    (func $do_tail_call (param $x i32) (result i32)
        (return_call $tail_target (local.get $x))
    )

    ;; Normal call to do_tail_call, which then tail calls tail_target
    (func (export "test") (param $x i32) (result i32)
        (call $do_tail_call (local.get $x))
    )
)
`

async function test() {
    // Test basic tail call
    {
        const instance = await instantiate(basicWat, {}, { tail_call: true })
        const { test } = instance.exports

        for (let i = 0; i < 1000; ++i) {
            assert.eq(test(0), 42)
            assert.eq(test(10), 52)
            assert.eq(test(100), 142)
            assert.eq(test(-42), 0)
        }
    }

    // Test chain of tail calls - all should share the same thunk frame
    {
        const instance = await instantiate(chainWat, {}, { tail_call: true })
        const { test } = instance.exports

        for (let i = 0; i < 1000; ++i) {
            // Each step adds 1, so: test adds 1, step2 adds 1, step1 adds 1, base adds 1 = 4 total
            assert.eq(test(0), 4)
            assert.eq(test(10), 14)
            assert.eq(test(100), 104)
        }
    }

    // Test mixed normal call + tail call
    {
        const instance = await instantiate(mixedWat, {}, { tail_call: true })
        const { test } = instance.exports

        for (let i = 0; i < 1000; ++i) {
            assert.eq(test(0), 100)
            assert.eq(test(50), 150)
        }
    }
}

await assert.asyncTest(test())
