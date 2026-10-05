function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

const lists = [
    ["break", "case", "catch", "continue", "debugger", "default", "do", "else", "finally", "for", "function", "if", "return", "switch", "throw", "try", "var", "while", "with", "null", "true", "false", "instanceof", "typeof", "void", "delete", "new", "in", "this"],
    ["a", "ab", "abc"],
    ["aaaa", "aaa", "aa", "a"],
    ["xy", "xz", "yy", "zz"],
    ["ab", "cd", "ef", "abc", "cde", "a", "c"],
    ["a", ""],
    ["", "abc", "de"],
    ["x"],
    ["abcdefghijklmnop", "abcdefghijklmnoq", "abcdefgh", "abcdefgi"],
    ["あい", "う", "えおか"],
];

function candidates(words) {
    const result = ["", "z", "zzzzzzzzzzzzzzzzz"];
    for (const word of words) {
        result.push(word, word + "x", "x" + word, word.slice(0, -1), word.slice(1), word.slice(0, -1) + "z", word + "\n", word.toUpperCase());
        result.push((word + "Ā").slice(0, -1));
    }
    return result;
}

const cases = lists.map((words) => ({ words, regExp: new RegExp("^(?:" + words.join("|") + ")$"), inputs: candidates(words) }));
const flattened = [];
for (const { words, regExp, inputs } of cases) {
    for (const input of inputs)
        flattened.push({ regExp, input, expected: words.includes(input) });
}

function test(regExp, input) {
    return regExp.test(input);
}
noInline(test);

for (let i = 0; i < testLoopCount; i += 100) {
    for (const { regExp, input, expected } of flattened) {
        if (test(regExp, input) !== expected)
            throw new Error(regExp + ".test(" + JSON.stringify(input) + ") should be " + expected);
    }
}

for (const { words, regExp, inputs } of cases) {
    const sticky = new RegExp(regExp.source, "y");
    const multiline = new RegExp(regExp.source, "m");
    for (const input of inputs) {
        const match = regExp.exec(input);
        shouldBe(match ? match[0] : null, words.includes(input) ? input : null, regExp + ".exec(" + JSON.stringify(input) + ")");
        sticky.lastIndex = 0;
        shouldBe(sticky.test(input), words.includes(input), sticky + ".test(" + JSON.stringify(input) + ")");
        sticky.lastIndex = 1;
        shouldBe(sticky.test(input), false, sticky + ".test(" + JSON.stringify(input) + ") at lastIndex 1");
        shouldBe(multiline.test(input), input.split("\n").some((line) => words.includes(line)), multiline + ".test(" + JSON.stringify(input) + ")");
    }
}
