.arch armv9-a+sve

// SVE basic arithmetic
add z0.b, z1.b, z2.b
add z0.h, z1.h, z2.h
add z0.s, z1.s, z2.s
add z0.d, z1.d, z2.d

// SVE predicated add
add z0.b, p0/m, z0.b, z1.b
add z0.h, p0/m, z0.h, z1.h
add z0.s, p0/m, z0.s, z1.s
add z0.d, p0/m, z0.d, z1.d

// SVE multiply
fmul z0.h, z1.h, z2.h
fmul z0.s, z1.s, z2.s
fmul z0.d, z1.d, z2.d

// SVE load/store
ld1w z0.s, p0/z, [x0]
ld1d z0.d, p0/z, [x0]
st1w z0.s, p0, [x0]
st1d z0.d, p0, [x0]

// SVE compare
fcmge p0.h, p0/z, z0.h, z1.h
fcmge p0.s, p0/z, z0.s, z1.s
fcmge p0.d, p0/z, z0.d, z1.d

// SVE logical
and z0.d, z1.d, z2.d
orr z0.d, z1.d, z2.d
eor z0.d, z1.d, z2.d
