// Internalization of 16-bit strings whose content fits in Latin-1 must
// canonicalize to an 8-bit AtomStringImpl. Encoding is verified in
// TestWebKitAPI/Tests/WTF/AtomString.cpp; this test covers JS-visible
// dedup and round-tripping.

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(a + " !== " + b);
}

(function () {
    // Force a 16-bit source by prefixing a BMP character, then slice it off.
    const source = "Āproperty_name_atom";
    const latin1Content = source.substring(1);
    shouldBe(latin1Content, "property_name_atom");

    const obj = {};
    obj[latin1Content] = 42;

    shouldBe(obj["property_name_atom"], 42);
    shouldBe(obj.property_name_atom, 42);

    const keys = Object.keys(obj);
    shouldBe(keys.length, 1);
    shouldBe(keys[0], "property_name_atom");
    shouldBe(obj[keys[0]], 42);

    obj["property_name_atom"] = 99;
    shouldBe(obj[latin1Content], 99);
})();

(function () {
    const a = ("Āabcdef").substring(1);
    const b = ("中abcdef").substring(1);
    shouldBe(a, b);
    shouldBe(a, "abcdef");

    const obj = {};
    obj[a] = 1;
    obj[b] = 2;
    shouldBe(obj[a], 2);
    shouldBe(obj[b], 2);
    shouldBe(obj["abcdef"], 2);
    shouldBe(Object.keys(obj).length, 1);
})();
