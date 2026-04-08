// Regression test: YARR JIT ParenContext use-after-free when
// clearInnerParenContextHeadSlots missed isCopy groups and sibling/
// ancestor-sibling groups. restoreParenContext restores ALL frame slots
// (including parenContextHead for siblings), but the old clearing function
// only walked the current group's inner disjunction and skipped isCopy
// terms, leaving stale pointers to freed/recycled ParenContext objects.

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// ---------------------------------------------------------------------------
// 1. PoC pattern from the bug report.
//    ((c|b)*?(y|x)+?.){3}mp
//    +? expands to {1,1} + *?(isCopy=true). The *? copy uses ParenContext
//    and is a sibling of (c|b)*?. Both bugs trigger: isCopy skip + sibling
//    scope mismatch.
// ---------------------------------------------------------------------------
(function testPoCPattern() {
    var re = new RegExp('((c|b)*?(y|x)+?.){3}mp');
    var input = 'yyyyyyyyybbbbbxxxxxxxxx';

    // Warm up YARR JIT
    for (var i = 0; i < 200; i++) {
        re.test(input);
        re.exec(input);
    }

    // Vary input to stress backtracking paths
    for (var i = 0; i < 50; i++) {
        var s = input.substring(0, i % input.length);
        re.test(s);
        re.exec(s + s);
    }

    // The pattern should not match the input (no "mp" suffix after 3 groups).
    shouldBe(re.exec(input), null, "PoC pattern should not match");

    // Check a matching input.
    var matchInput = 'byxcyx.mp';
    // Actually construct a valid match: each group is (c|b)*?(y|x)+?.
    // e.g. "byx." matches group: (c|b)*? matches "b", (y|x)+? matches "y", then "x" fails...
    // This is complex; just verify no crash on various inputs.
    for (var i = 0; i < 200; i++)
        re.exec(matchInput);
})();

// ---------------------------------------------------------------------------
// 2. isCopy UAF: +? quantifier creates copy groups.
//    (a)+? expands to (a){1,1}(a)*? where *? is isCopy=true with
//    quantityMaxCount=infinite, uses full ParenContext.
// ---------------------------------------------------------------------------
(function testIsCopyPlusNonGreedy() {
    var re = /((a|b)+?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("acabcd");
        re.exec("abcbcd");
        re.test("aaabbbcaaabbbcd");
        re.exec("xyzxyz");
    }

    shouldBe(re.exec("acabcd"), ["acabcd", "bcd", "b"], "isCopy +? test 1");
    shouldBe(re.exec("zzz"), null, "isCopy +? no match");
})();

// ---------------------------------------------------------------------------
// 3. isCopy UAF: +? with multi-alternative inside fixed-count outer.
//    Forces FixedCount backtracking path (call site 1) to restore
//    sibling copy group's parenContextHead.
// ---------------------------------------------------------------------------
(function testIsCopyInsideFixedCount() {
    var re = /((x|y)+?(a|b)+?.){3}end/;

    for (var i = 0; i < 200; i++) {
        re.exec("xaxbyaybxbend");
        re.exec("yyaaxxbbyyaaend");
        re.test("nope");
    }

    shouldBe(re.exec("nope"), null, "isCopy inside FixedCount no match");
})();

// ---------------------------------------------------------------------------
// 4. Sibling group UAF: Two non-greedy siblings at same nesting level.
//    Backtracking the first group restores frame slots including the
//    second group's parenContextHead.
// ---------------------------------------------------------------------------
(function testSiblingGroups() {
    var re = /((a)*?(b)*?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcabcd");
        re.exec("aabcbbcd");
        re.exec("cccd");
        re.test("abcabc");
    }

    shouldBe(re.exec("cccd"), ["cccd", "cd", undefined, undefined], "sibling test minimal");
})();

// ---------------------------------------------------------------------------
// 5. Ancestor-sibling group UAF: Inner group and an ancestor's sibling
//    share frame slot range. Restoring the inner group's parent context
//    overwrites the ancestor-sibling's parenContextHead.
// ---------------------------------------------------------------------------
(function testAncestorSiblingGroups() {
    var re = /((?:(x)*?y)(z)*?w){2}end/;

    for (var i = 0; i < 200; i++) {
        re.exec("xywzwendxywzwend");
        re.exec("ywwendywwend");
        re.test("fail");
    }

    shouldBe(re.exec("fail"), null, "ancestor-sibling no match");
})();

// ---------------------------------------------------------------------------
// 6. Greedy +? with captures: verify correct capture values after fix.
//    (([a-c])b*?\2){3} - backreference requiring within-iteration
//    backtracking combined with copy expansion.
// ---------------------------------------------------------------------------
(function testGreedyCopyWithCaptures() {
    var re = /(([a-c])b*?\2){3}/;

    for (var i = 0; i < 200; i++)
        re.exec("ababbbcbc");

    shouldBe(re.exec("ababbbcbc"), ["ababbbcbc", "cbc", "c"], "greedy copy captures");
})();

// ---------------------------------------------------------------------------
// 7. Non-greedy {n,m} quantifier with n>0: creates copy with bounded max.
//    (a){2,5}? expands to (a){2,2}(a){0,3}*? (copy, maxCount=3).
// ---------------------------------------------------------------------------
(function testBoundedNonGreedyCopy() {
    var re = /((a|b){2,5}?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcaacd");
        re.exec("aabcbbbcd");
        re.test("abcd");
    }

    shouldBe(re.exec("abcabcd"), ["abcabcd", "abcd", "b"], "bounded non-greedy copy");
})();

// ---------------------------------------------------------------------------
// 8. Three-level nesting: outer FixedCount > middle NonGreedy > inner
//    NonGreedy with copy. Exercises deep tree walking.
// ---------------------------------------------------------------------------
(function testDeepNesting() {
    var re = /(((a|b)*?c)+?d){2}e/;

    for (var i = 0; i < 200; i++) {
        re.exec("acdcde");
        re.exec("abcbcdacdcde");
        re.test("xxx");
    }

    shouldBe(re.exec("xxx"), null, "deep nesting no match");
})();

// ---------------------------------------------------------------------------
// 9. Stress: run many patterns in a tight loop to maximize ParenContext
//    free-list reuse and expose any remaining stale pointer dereferences.
// ---------------------------------------------------------------------------
(function testStress() {
    var patterns = [
        /((a|b)*?(c|d)+?.){3}zz/,
        /((x)*?(y)*?z){2}w/,
        /((?:(p|q)*?r)(s)*?t){2}end/,
        /((a|b){2,4}?c){3}d/,
        /((m|n)+?(o|p)+?q){2}r/,
    ];
    var inputs = [
        "abcdabcdabcdzz",
        "xyzxyzw",
        "prstendprstend",
        "abcabcabcd",
        "monopqmonopqr",
        "zzzzzzzzzz",
        "",
        "a",
    ];

    for (var i = 0; i < 500; i++) {
        var re = patterns[i % patterns.length];
        var input = inputs[i % inputs.length];
        re.test(input);
        re.exec(input);
    }
})();

// ---------------------------------------------------------------------------
// 10. Non-greedy multi-alt group inside FixedCount: existing regression
//     pattern from the original fix. Verify it still works.
// ---------------------------------------------------------------------------
(function testOriginalRegressionPatterns() {
    /(?:(a|b)*?.){2}xx/.exec('aabbcc');
    /((a|b)*?[a-c]){3}cp/.exec('aabbbccc');
    /((a|b)*?.){2}cp/.exec('aabbbccc');

    var r5 = /((a|b)*?(.)??.){3}cp/s;
    shouldBe(r5.exec('aabbbccc'), null, "original regression 1");
    shouldBe(r5.exec('aabbccc'), null, "original regression 2");

    // Run many times to ensure JIT path is exercised.
    for (var i = 0; i < 200; i++) {
        /(?:(a|b)*?.){2}xx/.exec('aabbcc');
        /((a|b)*?[a-c]){3}cp/.exec('aabbbccc');
        r5.exec('aabbbccc');
    }
})();
