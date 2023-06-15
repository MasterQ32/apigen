#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define APIGEN_NORETURN __attribute__((noreturn))
#define APIGEN_UNREACHABLE() __builtin_unreachable()
#define APIGEN_NOT_NULL(_X)                                                    \
  do {                                                                         \
    if ((_X) == NULL) {                                                        \
      apigen_panic("Value " #_X " was NULL");                                  \
    }                                                                          \
  } while (false)

#define APIGEN_STR(_X) #_X
#define APIGEN_SSTR(_X) APIGEN_STR(_X)

#define APIGEN_ASSERT(_Cond)                                                   \
  do {                                                                         \
    if ((_Cond) == 0)                                                          \
      apigen_panic(__FILE__ ":" APIGEN_SSTR(__LINE__) ":" "Assertion failure: " #_Cond " did not assert!");          \
  } while (false)

typedef void *apigen_Stream;
typedef void (*apigen_StreamWriter)(apigen_Stream stream, char const *string,
                                    size_t length);

#define APIGEN_IO_WRITE_STR(_Stream, _Writer, _X)                              \
  _Writer(_Stream, _X, strlen(_X))

void APIGEN_NORETURN apigen_panic(char const *msg);

void apigen_io_writeStdOut(apigen_Stream null_stream, char const *string,
                           size_t length);
void apigen_io_writeStdErr(apigen_Stream null_stream, char const *string,
                           size_t length);
void apigen_io_writeFile(apigen_Stream file, char const *string, size_t length);

enum apigen_TypeId {
  apigen_typeid_void,
  apigen_typeid_anyopaque,
  apigen_typeid_opaque,

  apigen_typeid_bool,

  apigen_typeid_uchar, // unsigned char
  apigen_typeid_ichar, // signed char
  apigen_typeid_char,  // char

  // unsigned ints:
  apigen_typeid_u8,
  apigen_typeid_u16,
  apigen_typeid_u32,
  apigen_typeid_u64,
  apigen_typeid_usize,
  apigen_typeid_c_ushort,
  apigen_typeid_c_uint,
  apigen_typeid_c_ulong,
  apigen_typeid_c_ulonglong,

  // signed ints:
  apigen_typeid_i8,
  apigen_typeid_i16,
  apigen_typeid_i32,
  apigen_typeid_i64,
  apigen_typeid_isize,
  apigen_typeid_c_short,
  apigen_typeid_c_int,
  apigen_typeid_c_long,
  apigen_typeid_c_longlong,

  // pointers:
  apigen_typeid_ptr_to_one,                       // extra: apigen_Type
  apigen_typeid_ptr_to_many,                      // extra: apigen_Type
  apigen_typeid_ptr_to_sentinelled_many,          // extra: apigen_Type
  apigen_typeid_nullable_ptr_to_many,             // extra: apigen_Type
  apigen_typeid_nullable_ptr_to_one,              // extra: apigen_Type
  apigen_typeid_nullable_ptr_to_sentinelled_many, // extra: apigen_Type

  apigen_typeid_const_ptr_to_one,                       // extra: apigen_Type
  apigen_typeid_const_ptr_to_many,                      // extra: apigen_Type
  apigen_typeid_const_ptr_to_sentinelled_many,          // extra: apigen_Type
  apigen_typeid_nullable_const_ptr_to_many,             // extra: apigen_Type
  apigen_typeid_nullable_const_ptr_to_one,              // extra: apigen_Type
  apigen_typeid_nullable_const_ptr_to_sentinelled_many, // extra: apigen_Type

  // compound types:
  apigen_typeid_enum,     // extra: apigen_Enum
  apigen_typeid_struct,   // extra: apigen_UnionOrStruct
  apigen_typeid_union,    // extra: apigen_UnionOrStruct
  apigen_typeid_array,    // extra: apigen_Array
  apigen_typeid_function, // extra: apigen_Function

  APIGEN_TYPEID_LIMIT,
};

struct apigen_Type {
  enum apigen_TypeId id;
  void *extra;           ///< Points to extra information to interpret this type.
  char const * name;     ///< Non-NULL if the type has was explicitly declared with a name. Otherwise, this type is anonymous
};

struct apigen_EnumItem {
    char const * documentation;
    char const * name;
    union {
        uint64_t uvalue; ///< Active for all enums based on an unsigned integer
        int64_t  ivalue; ///< Active for all enums based on an signed integer
    };
};

struct apigen_NamedValue {
    char const *         documentation;
    char const *         name;
    struct apigen_Type * type;
};

struct apigen_Enum {
    struct apigen_Type * underlying_type;
};

struct apigen_UnionOrStruct {
    size_t                     field_count;
    struct apigen_NamedValue * fields;
};

struct apigen_Array {
    uint64_t             size;
    struct apigen_Type * underlying_type;
};

struct apigen_Function {
    struct apigen_Type *       return_type;

    size_t                     parameter_count;
    struct apigen_NamedValue * parameters;
    
};

void apigen_type_free(struct apigen_Type *type);

struct apigen_Generator {
  void (*render_type)(struct apigen_Generator const *,
                      struct apigen_Type const *, apigen_Stream,
                      apigen_StreamWriter);
};

extern struct apigen_Generator apigen_gen_c;
extern struct apigen_Generator apigen_gen_cpp;
extern struct apigen_Generator apigen_gen_zig;
extern struct apigen_Generator apigen_gen_rust;

void apigen_generator_renderType(struct apigen_Generator const *generator,
                                 struct apigen_Type const *type,
                                 apigen_Stream stream,
                                 apigen_StreamWriter writer);

// memory module:

void * apigen_alloc(size_t size);
void apigen_free(void * ptr);

struct apigen_memory_arena_chunk;

struct apigen_memory_arena {
  struct apigen_memory_arena_chunk *first_chunk;
  struct apigen_memory_arena_chunk *last_chunk;
  size_t chunk_size;
};

void apigen_memory_arena_init(struct apigen_memory_arena *arena);
void apigen_memory_arena_deinit(struct apigen_memory_arena *arena);

void *apigen_memory_arena_alloc(struct apigen_memory_arena *arena, size_t size);
char *apigen_memory_arena_dupestr(struct apigen_memory_arena *arena, char const * str);

struct apigen_parser_declaration;

struct apigen_parser_state {
  FILE *file;
  char const * file_name;
  struct apigen_memory_arena *ast_arena;
  char const * line_feed; ///< used for multiline strings

  // output data:
  struct apigen_parser_declaration * top_level_declarations;
};

int apigen_parse(struct apigen_parser_state *state);
