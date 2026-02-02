# WebAssembly Tail Call Thunk Mechanism Implementation

## Overview

This document describes the implementation of the WebAssembly tail call thunk mechanism in JavaScriptCore. The mechanism replaces costly module-wide tail call analysis with a dynamic thunk frame insertion approach. The thunk frame is inserted only on the **first** tail call in a chain and handles instance restoration when the chain returns.

## Problem Statement

WebAssembly tail calls can cross module/instance boundaries. When a tail call chain returns, the original caller's instance state (memory base, bounds checking size, instance pointer) must be restored. Previously, this required expensive compile-time analysis to determine which functions could have their instance state clobbered by tail calls.

## Solution: Dynamic Thunk Frame Insertion

Instead of compile-time analysis, we insert a special "thunk frame" at runtime when the first tail call in a chain occurs. This thunk frame:
1. Saves the original caller's instance state
2. Contains the address of a restoration thunk as its return address
3. Is reused by subsequent tail calls in the chain

### Stack Frame Transformation

```
Before tail call:                    After inserting thunk:
| caller function frame |            | caller function frame      |
| current function frame |           | special thunk frame        |
                                     | tail-called function frame |
```

## Implementation Details

### 1. Core Infrastructure

#### 1.1 TailCallThunkMode (WasmCompilationMode.h)

Added `TailCallThunkMode` to the `CompilationMode` enum to identify the thunk callee:

```cpp
enum class CompilationMode : uint8_t {
    // ... existing modes ...
    TailCallThunkMode,
};
```

#### 1.2 TailCallThunkCallee (WasmCallee.h/cpp)

Created a singleton `TailCallThunkCallee` class that serves as a marker in the thunk frame's Callee slot:

```cpp
class TailCallThunkCallee final : public NativeCallee {
public:
    static TailCallThunkCallee& singleton();

    // Thunk frame layout constants
    static constexpr size_t thunkFrameSizeInBytes = 96;  // 80 + 16 for ARM64E sp context
    static constexpr ptrdiff_t offsetOfOriginalReturnPC = 32;
    static constexpr ptrdiff_t offsetOfSavedBoundsSize = 40;
    static constexpr ptrdiff_t offsetOfSavedMemoryBase = 48;
    static constexpr ptrdiff_t offsetOfSavedInstance = 56;
    static constexpr ptrdiff_t offsetOfPACContext = 64;           // ARM64E only
    static constexpr ptrdiff_t offsetOfOriginalReturnSPContext = 72;  // ARM64E only
};
```

#### 1.3 Thunk Frame Layout (96 bytes on ARM64E)

```
+------------------------------------------+ Higher addresses
|  (Padding for 16-byte alignment)         |  offset +88
+------------------------------------------+
|  (Padding)                               |  offset +80
+------------------------------------------+
|  Original Return SP Context (ARM64E)     |  offset +72
+------------------------------------------+
|  PAC Context for lr signing (ARM64E)     |  offset +64
+------------------------------------------+
|  Original Instance Pointer               |  offset +56
+------------------------------------------+
|  Original Memory Base                    |  offset +48
+------------------------------------------+
|  Original Bounds Checking Size           |  offset +40
+------------------------------------------+
|  Original Return PC (untagged)           |  offset +32
+------------------------------------------+
|  TailCallThunkCallee* (boxed)            |  offset +24 (CallFrameSlot::callee)
+------------------------------------------+
|  Original Instance (codeBlock slot)      |  offset +16 (CallFrameSlot::codeBlock)
+------------------------------------------+
|  Restoration Thunk Return PC             |  offset +8  (return to restoration thunk)
+------------------------------------------+
|  Original Caller Frame Pointer           |  offset +0  <- FP when in thunk
+------------------------------------------+ Lower addresses
```

### 2. Instance Restoration Thunk (WasmThunks.cpp)

The `tailCallInstanceRestorationThunkGenerator` creates code that:
1. Loads saved instance state from the thunk frame
2. Restores `wasmContextInstancePointer`, `wasmBaseMemoryPointer`, `wasmBoundsCheckingSizeRegister`
3. Returns to the original caller

```cpp
MacroAssemblerCodeRef<JITThunkPtrTag> tailCallInstanceRestorationThunkGenerator(const AbstractLocker&)
{
    CCallHelpers jit;

    // The thunk frame is directly at cfr (no offset calculation needed)
    GPRReg thunkFrameReg = GPRInfo::callFrameRegister;

    // Load saved instance state
    jit.loadPtr(Address(thunkFrameReg, TailCallThunkCallee::offsetOfSavedInstance),
                GPRInfo::wasmContextInstancePointer);
    jit.loadPtr(Address(thunkFrameReg, TailCallThunkCallee::offsetOfSavedMemoryBase),
                GPRInfo::wasmBaseMemoryPointer);
    jit.loadPtr(Address(thunkFrameReg, TailCallThunkCallee::offsetOfSavedBoundsSize),
                GPRInfo::wasmBoundsCheckingSizeRegister);

#if CPU(ARM64E)
    // Load the original sp context and return address
    jit.loadPtr(Address(thunkFrameReg, TailCallThunkCallee::offsetOfOriginalReturnSPContext), scratchReg);
    jit.loadPtr(Address(thunkFrameReg, TailCallThunkCallee::offsetOfOriginalReturnPC), ARM64Registers::lr);
    jit.loadPtr(Address(thunkFrameReg), GPRInfo::callFrameRegister);
    jit.move(scratchReg, MacroAssembler::stackPointerRegister);
    jit.tagReturnAddress();  // Sign lr with sp
    jit.ret();
#else
    // Non-ARM64E: simpler return
    jit.loadPtr(Address(thunkFrameReg, TailCallThunkCallee::offsetOfOriginalReturnPC), lr);
    jit.loadPtr(Address(thunkFrameReg), GPRInfo::callFrameRegister);
    jit.ret();
#endif
}
```

### 3. JSCConfig Fields (JSCConfig.h)

Added configuration fields for the tail call thunk mechanism:

```cpp
#if ENABLE(WEBASSEMBLY)
    // For tail call thunk mechanism
    void* wasmTailCallThunkCallee;       // Boxed pointer to TailCallThunkCallee singleton
    void* wasmTailCallRestorationThunk;  // Code pointer to restoration thunk
    void* wasmIPIntDirectEntrypoint;     // Direct ipint_entry address (for ARM64E)
#endif
```

These are initialized in `Wasm::Thunks::initialize()`:

```cpp
void Thunks::initialize()
{
    // Store the boxed TailCallThunkCallee pointer
    TailCallThunkCallee& tailCallThunkCallee = TailCallThunkCallee::singleton();
    g_jscConfig.wasmTailCallThunkCallee = CalleeBits::boxNativeCallee(&tailCallThunkCallee);

    // Store the restoration thunk (untagged for manual PAC signing)
    auto restorationThunk = thunks->stub(locker, tailCallInstanceRestorationThunkGenerator);
    g_jscConfig.wasmTailCallRestorationThunk = restorationThunk.code().untaggedPtr();

    // Store direct ipint_entry for ARM64E tail calls
    auto ipintEntryCode = LLInt::getCodePtr<OperationPtrTag>(ipint_entry);
    g_jscConfig.wasmIPIntDirectEntrypoint = ipintEntryCode.retagged<WasmEntryPtrTag>().taggedPtr();
}
```

### 4. IPInt Tail Call Implementation (InPlaceInterpreter64.asm)

#### 4.1 Thunk Detection

At `.ipint_tail_call_common`, before performing the tail call:

```asm
.ipint_tail_call_common:
    # Check if the CALLER's callee is the TailCallThunkCallee singleton
    loadp [cfr], t2                       # t2 = caller's FP
    loadp Callee[t2], t2                  # t2 = caller's callee (boxed)
    leap _g_config, sc2
    loadp JSCConfigOffset + constexpr JSC::offsetOfJSCConfigWasmTailCallThunkCallee[sc2], sc2
    bpeq t2, sc2, .ipint_tail_call_has_thunk

    # No thunk exists - we need to insert one
    move (constexpr Wasm::TailCallThunkCallee::thunkFrameSizeInBytes), t2
    jmp .ipint_tail_call_continue

.ipint_tail_call_has_thunk:
    # Thunk already exists - skip insertion
    move 0, t2

.ipint_tail_call_continue:
    # t2 = thunk adjustment (0 if thunk exists, 96 if we need to insert one)
```

#### 4.2 Thunk Frame Insertion

When a thunk needs to be inserted:

```asm
    # Adjust frame pointer down to make room for thunk
    subp t2, sc2

    # Build thunk frame at sc2 + thunkFrameSize
    addp sc2, t2, t3               # t3 = thunk frame FP

    # Store original caller's FP
    storep sc1, [t3]

    # Store original return PC (untagged on ARM64E)
if ARM64E
    addp CallerFrameAndPCSize, cfr, t0
    untagReturnAddress t0
    storep lr, (constexpr Wasm::TailCallThunkCallee::offsetOfOriginalReturnPC)[t3]
    storep t0, (constexpr Wasm::TailCallThunkCallee::offsetOfOriginalReturnSPContext)[t3]
else
    storep lr, (constexpr Wasm::TailCallThunkCallee::offsetOfOriginalReturnPC)[t3]
end

    # Store instance state
    storep wasmInstance, (constexpr Wasm::TailCallThunkCallee::offsetOfSavedInstance)[t3]
    storep boundsCheckingSize, (constexpr Wasm::TailCallThunkCallee::offsetOfSavedBoundsSize)[t3]
    storep memoryBase, (constexpr Wasm::TailCallThunkCallee::offsetOfSavedMemoryBase)[t3]

    # Store boxed TailCallThunkCallee
    leap _g_config, t1
    loadp JSCConfigOffset + constexpr JSC::offsetOfJSCConfigWasmTailCallThunkCallee[t1], t1
    storeq t1, Callee[t3]

    # Store restoration thunk as return address
    leap _g_config, t1
    loadp JSCConfigOffset + constexpr JSC::offsetOfJSCConfigWasmTailCallRestorationThunk[t1], t1
if ARM64E
    # Sign restoration thunk for thunk frame context
    addp CallerFrameAndPCSize, t3, t0
    move t1, lr
    tagReturnAddress t0
    storep lr, ReturnPC[t3]

    # Sign restoration thunk for gate context
    move t1, lr
    tagReturnAddress sc2
end
```

## ARM64E Pointer Authentication Fix

### The Problem

On ARM64E, the `inPlaceInterpreterEntryThunk` calls `tagReturnAddress()` (pacibsp) which signs `lr` with `sp`. When performing tail calls:

1. Tail call code signs `lr` (restoration thunk) with `sc2` context
2. Gate authenticates `lr` with `sc2`, then re-signs with `sp` (pacibsp)
3. Gate jumps to `inPlaceInterpreterEntryThunk`
4. Entry thunk does **ANOTHER** pacibsp, corrupting `lr`
5. On return, `retab` fails to authenticate the corrupted `lr` → SIGKILL

### The Solution

For IPInt callees on ARM64E, bypass the entry thunk by jumping directly to `ipint_entry`:

```asm
if ARM64E
    # Check if the callee is an IPInt callee
    loadq Callee[sc2], t1                       # t1 = boxed callee
    btpz t1, .ipint_tail_call_skip_ipint_check
    unboxWasmCallee(t1, t0)                     # Unbox to get real pointer
    loadb Wasm::Callee::m_compilationMode[t1], t1
    bineq t1, constexpr Wasm::CompilationMode::IPIntMode, .ipint_tail_call_skip_ipint_check

    # It's an IPInt callee - use direct ipint_entry
    leap _g_config, t1
    loadp JSCConfigOffset + constexpr JSC::offsetOfJSCConfigWasmIPIntDirectEntrypoint[t1], ws0
.ipint_tail_call_skip_ipint_check:
end
```

### Correct PAC Flow After Fix

1. Tail call code signs `lr` (restoration thunk) with `sc2` context
2. Gate authenticates `lr` with `sc2` (x11), re-signs with `sp` (pacibsp)
3. Gate jumps to `ipint_entry` directly (skipping entry thunk)
4. `ipint_entry` does `preserveCallerPCAndCFR()` which pushes `lr` to stack (no signing)
5. On return, `restoreCallerPCAndCFR()` pops `lr`
6. `returnFromLLInt` does `retab` which authenticates with `sp` ✓

## Gate Thunks (LLIntThunks.cpp)

### createWasmIPIntTailCallGate

This gate handles the PAC transition for IPInt tail calls:

```cpp
MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmIPIntTailCallGate(PtrTag tag)
{
    CCallHelpers jit;

    // Gate entry state:
    // - lr = restoration thunk, signed with x11 context
    // - x11 = PAC signing context
    // - x9 = callee entrypoint, tagged with WasmEntryPtrTag

    // Step 1: Authenticate lr using x11
    jit.untagPtr(GPRInfo::wasmScratchGPR2, ARM64Registers::lr);
    jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::wasmScratchGPR2);

    // Step 2: Re-sign lr with sp (for returnFromLLInt's retab)
    jit.tagReturnAddress();

    // Step 3: Branch to callee entrypoint
    jit.farJump(GPRInfo::wasmScratchGPR0, WasmEntryPtrTag);
}
```

## Files Modified Summary

| File | Changes |
|------|---------|
| `wasm/WasmCompilationMode.h` | Added `TailCallThunkMode` enum |
| `wasm/WasmCompilationMode.cpp` | Added string for `TailCallThunkMode` |
| `wasm/WasmCallee.h` | Added `TailCallThunkCallee` class |
| `wasm/WasmCallee.cpp` | Implemented `TailCallThunkCallee` singleton |
| `wasm/WasmThunks.h` | Declared `tailCallInstanceRestorationThunkGenerator` |
| `wasm/WasmThunks.cpp` | Implemented restoration thunk and initialization |
| `runtime/JSCConfig.h` | Added config fields and offset constants |
| `llint/InPlaceInterpreter64.asm` | Implemented thunk detection and insertion |
| `llint/LLIntThunks.cpp` | Added `createWasmIPIntTailCallGate` |
| `llint/LLIntData.cpp` | Registered the IPInt tail call gate |

## Testing

### Basic Test (verified working on ARM64E)

```javascript
// func1 does return_call to func0 which returns 42
const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x03, 0x02, 0x00, 0x00,
    0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x01,
    0x0a, 0x0b, 0x02,
    0x04, 0x00, 0x41, 0x2a, 0x0b,  // func 0: i32.const 42, end
    0x04, 0x00, 0x12, 0x00, 0x0b   // func 1: return_call 0, end
]);

const module = new WebAssembly.Module(bytes);
const instance = new WebAssembly.Instance(module, {});
console.log(instance.exports.test());  // Should print 42
```

Run with:
```bash
jsc --useBBQJIT=0 --useOMGJIT=0 --useWasmTailCalls=1 test.js
```

## Remaining Work

1. **BBQ JIT Implementation**: Add thunk detection and insertion to `WasmBBQJIT.cpp`
2. **OMG JIT Implementation**: Add thunk detection and insertion to `WasmOMGIRGenerator.cpp`
3. **Comprehensive Tests**: Add stress tests for:
   - Cross-instance tail calls
   - Chained tail calls
   - Mixed normal call + tail call scenarios
   - Stack argument handling with thunk insertion

## References

- WebAssembly Tail Call Proposal: https://github.com/WebAssembly/tail-call
- ARM64E PAC documentation: Apple internal documentation
- JavaScriptCore LLInt: `llint/` directory
- WebAssembly implementation: `wasm/` directory
