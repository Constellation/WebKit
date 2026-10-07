//@ runDefaultWasm("--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")

load("wast.js", "caller relative");

// The store in the then-arm never runs when its condition folds to 0, so the second struct.get may
// reuse the first. When the condition is live, the store must be seen by the second struct.get.
const moduleBytes = WebAssemblyText.encode(`
(module
  (type $S (struct (field (mut i32))))

  (func (export "make") (param $value i32) (result (ref $S))
    (struct.new $S (local.get $value)))

  (func $deadArm (param $s (ref $S)) (param $x i32) (result i32)
    (local $t i32)
    (local.set $t (struct.get $S 0 (local.get $s)))
    (if (i32.and (local.get $x) (i32.const 0))
      (then
        (struct.set $S 0 (local.get $s) (i32.add (local.get $t) (i32.const 1)))))
    (i32.add (local.get $t) (struct.get $S 0 (local.get $s))))

  (func $liveArm (param $s (ref $S)) (param $x i32) (result i32)
    (local $t i32)
    (local.set $t (struct.get $S 0 (local.get $s)))
    (if (i32.and (local.get $x) (i32.const 1))
      (then
        (struct.set $S 0 (local.get $s) (i32.add (local.get $t) (i32.const 1)))))
    (i32.add (local.get $t) (struct.get $S 0 (local.get $s))))

  (func (export "runDeadArm") (param $s (ref $S)) (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc) (call $deadArm (local.get $s) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "runLiveArm") (param $s (ref $S)) (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc) (call $liveArm (local.get $s) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))
)
`);

const { make, runDeadArm, runLiveArm } = new WebAssembly.Instance(new WebAssembly.Module(moduleBytes)).exports;

const iterations = 1000;
for (let i = 0; i < wasmTestLoopCount / 100; ++i) {
    let result = runDeadArm(make(3), iterations);
    if (result !== 6 * iterations)
        throw new Error(`runDeadArm returned ${result}`);

    // The field starts at 3 and every odd iteration stores the value it read plus one, so
    // iteration k reads v(k) = 3 + floor(k / 2) first.
    let expected = 0;
    for (let k = 0, v = 3; k < iterations; ++k) {
        expected += (k & 1) ? 2 * v + 1 : 2 * v;
        if (k & 1)
            ++v;
    }
    result = runLiveArm(make(3), iterations);
    if (result !== expected)
        throw new Error(`runLiveArm returned ${result}, expected ${expected}`);
}
