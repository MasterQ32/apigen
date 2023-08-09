# APIGEN - API Definition Language

APIGEN provides tooling to create a single-source-of-truth API definition and generate bindings and implementation stubs from it.

The project goal is that developers don't have to maintain the guts of library wrappers anymore and provide a precise way to declare APIs:

```zig
type apigen_TypeId = enum {
    apigen_typeid_void,
    apigen_typeid_anyopaque,
    apigen_typeid_opaque,

    // ...
};

type apigen_Type = struct {
  /// What kind of type is this type.
  id: apigen_TypeId,

  /// Points to extra information to interpret this type.
  extra: ?*anyopaque,

  /// Non-NULL if the type has was explicitly declared with a name. Otherwise, this type is anonymous
  name: ?[*:0]const char,
};

type apigen_MemoryArenaChunk = opaque{};

type apigen_MemoryArena = struct {
  first_chunk: ?*apigen_MemoryArenaChunk,
  last_chunk: ?*apigen_MemoryArenaChunk,
  chunk_size: usize,
};

fn apigen_memory_arena_init(arena: *apigen_MemoryArena) void;
fn apigen_memory_arena_deinit(arena: *apigen_MemoryArena) void;
```

## Backends

- [x] C
- [ ] C++
- [x] Zig
- [ ] Rust
- [ ] Go
- [ ] C#

## Missing features

- [ ] Field alignment
- [ ] Pointer alignment
- [ ] Macro/inline function support
- [ ] Implementation stub generator
- [ ] Packed structs
- [ ] Bitfield generation (Zig: `packed struct`, C: int + macro helpers, Rust: ???)
- [ ] Namespaces
- [ ] Calling convention support
- [ ] Include files / structuring

## apigen Language

The following docs use `T` as a placeholder for arbitrary other types, `N` is used as a placeholder for a natural number. `I` is used for arbitrary integers.

### Overview

```zig

/// documentation comment for the following thing:
type MyTypeName = T;

// regular comment doesn't document things!

var   errno: c_int; // declares an external, mutable variable
const errno: c_int; // declares an external, immutable variable

constexpr pi: f32 = 10; // declares a global, api-wide constant value

// external functions can be defined like this:
fn square(i: i32) i32;

// documentation can be applied to everything that declares something:

/// Error type
type E = enum {
    /// no error happened
    ok,

    /// an allocation failed
    out_of_memory,
};

/// A position in the 2D plane
type Point = struct {
    /// horizontal distance to the origin
    x: f32,
    /// vertical distance to the origin
    y: f32,
};

/// Creates a new window
fn CreateWindow(
    /// The window title that is displayed in the bar above the content
    title: [*:0]const u8,
    /// Width of the content in pixels
    width: u32,
    /// Width of the content in pixels
    height: u32,
);

```

### tl;dr

- Syntax like zig, but top level declarations differ
- Trailing commas are legal, but not required
- Doc comments have semantic meanings, regular comments don't.

### Type System

The following table contains a short description of all available type constructs:

| Syntax          | Description                                                           | Range                |
| --------------- | --------------------------------------------------------------------- | -------------------- |
| `void`          | Return type of a function that returns nothing.                       |                      |
| `bool`          | A boolean type.                                                       | `false` or `true`    |
| `u8`            | unsigned int, 8 bit                                                   | 0 … 2⁸-1             |
| `u16`           | unsigned int, 16 bit                                                  | 0 … 2¹⁶-1            |
| `u32`           | unsigned int, 32 bit                                                  | 0 … 2³²-1            |
| `u64`           | unsigned int, 64 bit                                                  | 0 … 2⁶⁴-1            |
| `usize`         | unsigned int, pointer width                                           | *platform dependent* |
| `i8`            | signed int, 8 bit                                                     | -2⁷  … 2⁷-1          |
| `i16`           | signed int, 16 bit                                                    | -2¹⁵ … 2¹⁵-1         |
| `i32`           | signed int, 32 bit                                                    | -2³¹ … 2³¹-1         |
| `i64`           | signed int, 64 bit                                                    | -2⁶³ … 2⁶³-1         |
| `isize`         | signed int, pointer width                                             | *platform dependent* |
| `f32`           | The `binary32` floating point type defined by IEEE 754                |                      |
| `f64`           | The `binary64` floating point type defined by IEEE 754                |                      |
| `*T`            | non-nullable pointer to a single mutable `T`                          |                      |
| `[*]T`          | non-nullable pointer to many mutable `T`s                             |                      |
| `[*:N]T`        | non-nullable pointer to many mutable `T`s terminated by `N`           |                      |
| `*const T`      | non-nullable pointer to a single constant `T`                         |                      |
| `[*]const T`    | non-nullable pointer to many constant `T`s                            |                      |
| `[*:N]const T`  | non-nullable pointer to many constant `T`s terminated by `N`          |                      |
| `?*T`           | nullable pointer to a single mutable `T`                              |                      |
| `?[*]T`         | nullable pointer to many mutable `T`s                                 |                      |
| `?[*:N]T`       | nullable pointer to many mutable `T`s terminated by `N`               |                      |
| `?*const T`     | nullable pointer to a single constant `T`                             |                      |
| `?[*]const T`   | nullable pointer to many constant `T`s                                |                      |
| `?[*:N]const T` | nullable pointer to many constant `T`s terminated by `N`              |                      |
| `[N]T`          | array of `N` consecutive `T`s.                                        |                      |
| `struct { … }`  | A composite structure of several types. *See below*                   |                      |
| `union { … }`   | An alias structure of several types. *See below*                      |                      |
| `enum { … }`    | An automatically typed list of named values. *See below*              |                      |
| `enum(T) { … }` | A list of named values of type `T`. *See below*                       |                      |
| `opaque { }`    | A specific type defined elsewhere, but can be pointed to. *See below* |                      |
| `anyopaque`     | A unspecified type defined  elsewhere, but can be pointed to.         |                      |
| `c_char`        | The equivalent to the C type `char`                                   | 0 … 127              |
| `c_uchar`       | The equivalent to the C type `unsigned char`                          | 0 … 255              |
| `c_ichar`       | The equivalent to the C type `signed char`                            | -128 … 127           |
| `c_short`       | The equivalent to the C type `short`                                  | *platform dependent* |
| `c_ushort`      | The equivalent to the C type `unsigned short`                         | *platform dependent* |
| `c_int`         | The equivalent to the C type `int`                                    | *platform dependent* |
| `c_uint`        | The equivalent to the C type `unsigned int`                           | *platform dependent* |
| `c_long`        | The equivalent to the C type `long`                                   | *platform dependent* |
| `c_ulong`       | The equivalent to the C type `unsigned long`                          | *platform dependent* |
| `c_longlong`    | The equivalent to the C type `long long`                              | *platform dependent* |
| `c_ulonglong`   | The equivalent to the C type `unsigned long long`                     | *platform dependent* |

#### `struct { … }`

```zig
struct {
    field_1: T,
    field_2: T,
    …
}
```

A structure is a composite of several types, each layed out in seqeuence next to each other in memory, with respect to natural alignment.

#### `union { … }`

```zig
union {
    field_1: T,
    field_2: T,
    …
}
```

A union is declared syntacially similar to a struct, but all fields occupy the same memory address and only one field can be active at a time.

#### `enum { … }`

```zig
enum {
    item_1 = I,
    item_2 = I,
    item_3 = I,
    …
}
```

An enumeration is a type that only allows an exclusive set of named items. Each items has an integer value assigned to it.

An enumeration is stored in an integer that can store all item values and is selected automatically by the system.

#### `enum(T) { … }`

```zig
enum(T) {
    item_1 = I,
    item_2 = I,
    item_3 = I,
    …
}
```

Similar to a regular enum, but the backing integer is explicitly given instead of being deduced by the system.

#### `opaque { }`

This is an opaque type (like `anyopaque`) which is only legal to be used in pointers. It has no specified size, and
cannot be instantiated, but might be implemented by external means.

With this, types can be implementation-defined and hidden from the API surface.

## Building

### Dependencies

- [`zig`](https://ziglang.org/download/), at least 0.11.0-dev.3258 or later
- [`flex`](https://github.com/westes/flex), at least version 2.6.4
- [`bison`](https://www.gnu.org/software/bison/), at least version 3.8.2

### Build Commands

### Build

```sh-session
user@host:/workspace/projects/apigen$ zig build install
user@host:/workspace/projects/apigen$
```

will output `zig-out/bin/apigen`

### Tests

```sh-session
user@host:/workspace/projects/apigen$ zig build test
user@host:/workspace/projects/apigen$
```

will run the test suite and outputs the failed tests, if any.

This may take a while as a lot of variants are tested.

## FAQ

### Why write in in C when there is Zig already a dependency?

This reason is quite simple: It allows other projects to vendor a distribution
of `apigen` without the Zig build stuff. The whole thing is designed to be compiled with a `gcc src/*.c` invocation after the parser and lexer have been generated.

This makes it really easy to vendor everything.

