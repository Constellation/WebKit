// Tests that various instructions are valid after unreachable (polymorphic stack).
// The Bot type from an empty stack in unreachable context should satisfy any type requirement.

import { compile } from "../gc/wast-wrapper.js";

// i31.get_s after unreachable should be valid (Bot satisfies i31ref requirement)
compile(`
  (module
    (func (result i32)
      unreachable
      i31.get_s))
`);

// i31.get_u after unreachable should be valid
compile(`
  (module
    (func (result i32)
      unreachable
      i31.get_u))
`);

// ref.as_non_null after unreachable should be valid
compile(`
  (module
    (func (result (ref func))
      unreachable
      ref.as_non_null))
`);

// ref.is_null after unreachable should be valid
compile(`
  (module
    (func (result i32)
      unreachable
      ref.is_null))
`);

// any.convert_extern after unreachable should be valid
compile(`
  (module
    (func (result anyref)
      unreachable
      any.convert_extern))
`);

// extern.convert_any after unreachable should be valid
compile(`
  (module
    (func (result externref)
      unreachable
      extern.convert_any))
`);

// struct.get after unreachable should be valid
compile(`
  (module
    (type (struct (field i32)))
    (func (result i32)
      unreachable
      struct.get 0 0))
`);

// array.get after unreachable should be valid
compile(`
  (module
    (type (array i32))
    (func (result i32)
      unreachable
      array.get 0))
`);

// array.len after unreachable should be valid
compile(`
  (module
    (func (result i32)
      unreachable
      array.len))
`);

// select after unreachable should be valid
compile(`
  (module
    (func (result i32)
      unreachable
      select))
`);

// ref.eq after unreachable should be valid
compile(`
  (module
    (func (result i32)
      unreachable
      ref.eq))
`);
