#include "parser.h"
#include "apigen.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-macro-identifier" // is generated by flex/bison
#include "lexer.yy.h"
#include "parser.yy.h"
#pragma clang diagnostic pop

struct apigen_ParserDeclaration EMPTY_DOCUMENT_SENTINEL;

bool apigen_parse(struct apigen_ParserState * state)
{
    APIGEN_NOT_NULL(state);

    APIGEN_ASSERT(state->top_level_declarations == NULL);

    {
        yyscan_t scanner;
        apigen_parser_lex_init_extra (state, &scanner);

        int const lex_result = apigen_parser_parse(scanner, state);

        apigen_parser_lex_destroy(scanner);

        if (lex_result != 0) {
            return false;
        }
    }

    if(state->top_level_declarations == &EMPTY_DOCUMENT_SENTINEL) {
        state->top_level_declarations = NULL;
    }

    return true;
}

static bool validate_include_path(char const * include_file_name)
{
    // see this:
    // https://stackoverflow.com/a/31976060

    // only lega characters in a file name are basic path symbols, 
    // and forward slashes (`/`). This way, we can keep the files portable
    // between platforms and windowqs users won't accidently use `\` for paths.
    //
    // we also disallow absolute paths for the same reasoning.

    APIGEN_NOT_NULL(include_file_name);

    char const * c = include_file_name;

    // We do not allow absolute paths in any case:
    if(*c == '\\' || *c == '/')
        return false;

    bool last_is_illegal = false;
    while(*c) {

        if((*c) < 0x20) {
            // Control characters
            return false;
        }

        switch(*c) {
            case '<': return false; // less than
            case '>': return false; // greater than
            case ':': return false; // colon - sometimes works, but is actually NTFS Alternate Data Streams
            case '"': return false; // double quote
            case '\\': return false; // backslash
            case '|': return false; // vertical bar or pipe
            case '?': return false; // question mark
            case '*': return false; // asterisk
        }

        last_is_illegal = (*c == ' ') || (*c == '.'); // not legal on windows

        c += 1;
    }   
    if(last_is_illegal) {
        return false;
    }

    return true;
}

static void split_include_path(char const * include_path, size_t * name_head)
{
    APIGEN_NOT_NULL(include_path);
    APIGEN_NOT_NULL(name_head);

    size_t i = 0;
    while(true) {
        char c = include_path[i];
        if(c == 0) {
            *name_head = 0;
            return;
        } else if(c == '/') {
            APIGEN_ASSERT(i > 0); // otherwise it would be an absolute path!
            *name_head = i + 1;
            return;
        }
        i += 1;
    }
}

struct apigen_ParserDeclaration * apigen_parser_file_include(
    struct apigen_ParserState * outer_state, 
    struct apigen_ParserLocation location, 
    struct apigen_ParserDeclaration * previous_decls,
    char const * include_path
)
{
    APIGEN_NOT_NULL(outer_state);
    APIGEN_NOT_NULL(include_path);

    if(!validate_include_path(include_path)) {
        apigen_diagnostics_emit(
            outer_state->diagnostics, 
            outer_state->file_name,
            location.first_line,
            location.first_column,
            apigen_error_invalid_include_path, 
            include_path
        );
        return previous_decls; // continue lexing, but emit error
    }

    struct apigen_ParserState inner_state = {
        .source_dir = {0},

        .file      = {0},
        .file_name = 0, 

        .ast_arena = outer_state->ast_arena,
        .line_feed = outer_state->line_feed,
        
        .diagnostics = outer_state->diagnostics,
        
        .top_level_declarations = NULL,
    };

    size_t filename_offset;
    split_include_path(include_path, &filename_offset);
    
    char const * include_file_name = include_path + filename_offset;
    
    bool dir_ok;
    if(filename_offset > 0) {
        char * const directory_path = apigen_alloc(filename_offset + 1);
        memcpy(directory_path, include_path, filename_offset);
        directory_path[filename_offset] = 0;

        dir_ok = apigen_io_open_dir(outer_state->source_dir, directory_path, &inner_state.source_dir);
        
        apigen_free(directory_path);
    }
    else {
        dir_ok = apigen_io_open_dir(outer_state->source_dir, ".", &inner_state.source_dir);
    }
    
    if(!dir_ok || !apigen_io_open_file_read(inner_state.source_dir, include_file_name, &inner_state.file)) {
        apigen_diagnostics_emit(
            outer_state->diagnostics, 
            outer_state->file_name,
            location.first_line,
            location.first_column,
            apigen_error_missing_include_file,
            include_path
        );
        return previous_decls; // continue lexing, but emit error
    }

    
    yyscan_t scanner;
    apigen_parser_lex_init_extra (&inner_state, &scanner);

    int const lex_result = apigen_parser_parse(scanner, &inner_state);

    apigen_parser_lex_destroy(scanner);

    apigen_io_close(&inner_state.file); 
    apigen_io_close_dir(&inner_state.source_dir); 

    if (lex_result != 0) {
        return previous_decls;
    }
    
    
    if(inner_state.top_level_declarations == &EMPTY_DOCUMENT_SENTINEL) {
        return previous_decls; // list was empty, we can safely return the same as before
    }
    else {
        APIGEN_ASSERT(inner_state.top_level_declarations != NULL);
        
        
        if(previous_decls != NULL) {
            struct apigen_ParserDeclaration * tail = previous_decls;
            while(tail->next != NULL) {
                tail = tail->next;
            }
            // Attach parsed items to tail:
            tail->next = inner_state.top_level_declarations;
            return previous_decls;
        }
        else {
            // We are now HEAD of the list
            return inner_state.top_level_declarations;
        }
    }
}


struct apigen_Value apigen_parser_conv_regular_str(struct apigen_ParserState * state, char const * literal)
{
    APIGEN_NOT_NULL(state);

    size_t const len = strlen(literal);
    APIGEN_ASSERT(len >= 2); // starts and ends with '"'
    APIGEN_ASSERT(literal[0] == '"');
    APIGEN_ASSERT(literal[len - 1] == '"');

    size_t converted_length = 0;
    {
        size_t i = 1;
        while (i < len - 1) {
            char c = literal[i];
            switch (c) {
                case '\\':
                    i += 2;
                    converted_length += 1;
                    break;
                default:
                    i += 1;
                    converted_length += 1;
                    break;
            }
        }
    }

    char * const converted = apigen_memory_arena_alloc(state->ast_arena, converted_length + 1);

    {
        size_t src = 1;
        size_t dst = 0;
        while (src < len - 1) {
            char c = literal[src];
            switch (c) {
                case '\\':
                    c = literal[src + 1];
                    switch (c) {
                        case 'n': converted[dst] = '\n'; break;
                        case 'r': converted[dst] = '\r'; break;
                        case 'e': converted[dst] = '\x1B'; break;
                        case '\"': converted[dst] = '\"'; break;
                        case '\'': converted[dst] = '\''; break;
                        default: converted[dst] = c; break;
                    }
                    src += 2;
                    dst += 1;
                    break;
                default:
                    converted[dst] = c;
                    src += 1;
                    dst += 1;
                    break;
            }
        }
    }

    converted[converted_length] = 0;

    return (struct apigen_Value){.type = apigen_value_str, .value_str = converted};
}

struct apigen_Value apigen_parser_conv_multiline_str(struct apigen_ParserState * state, char const * literal)
{
    APIGEN_NOT_NULL(state);

    size_t const len = strlen(literal);
    APIGEN_ASSERT(len >= 2); // starts with '\\'

    char * const converted = apigen_memory_arena_dupestr(state->ast_arena, literal + 2);

    return (struct apigen_Value){.type = apigen_value_str, .value_str = converted};
}

struct apigen_Value apigen_parser_concat_multiline_strs(struct apigen_ParserState * state, struct apigen_Value str1, struct apigen_Value str2)
{
    APIGEN_NOT_NULL(state);
    APIGEN_ASSERT(str1.type == apigen_value_str);
    APIGEN_ASSERT(str2.type == apigen_value_str);

    char const * const line_feed = (state->line_feed != NULL) ? state->line_feed : "\n";

    size_t const lf_len   = strlen(line_feed);
    size_t const str1_len = strlen(str1.value_str);
    size_t const str2_len = strlen(str2.value_str);

    size_t const total_len = lf_len + str1_len + str2_len + 1;

    char * const output_string = apigen_memory_arena_alloc(state->ast_arena, total_len);

    memcpy(output_string, str1.value_str, str1_len);
    memcpy(output_string + str1_len, line_feed, lf_len);
    memcpy(output_string + str1_len + lf_len, str2.value_str, str2_len);
    output_string[total_len] = 0;

    return (struct apigen_Value){.type = apigen_value_str, .value_str = output_string};
}

char const * apigen_parser_create_doc_string(struct apigen_ParserState * state, char const * str1)
{
    APIGEN_NOT_NULL(state);
    APIGEN_NOT_NULL(str1);

    size_t len = strlen(str1);
    APIGEN_ASSERT(len >= 3); // must start with '///'

    APIGEN_ASSERT(str1[0] == '/');
    APIGEN_ASSERT(str1[1] == '/');
    APIGEN_ASSERT(str1[2] == '/');
    len -= 3;

    char const * text = str1 + 3;
    if(len > 0 && text[0] == ' ') {
        len -= 1;
        text += 1;
    }

    if(len == 0) {
        return "";
    }

    char * const output_string = apigen_memory_arena_alloc(state->ast_arena, len + 1);
    memcpy(output_string, text, len);
    output_string[len] = 0;

    return output_string;

}

char const * apigen_parser_concat_doc_strings(struct apigen_ParserState * state, char const * str1, char const * str2)
{
    APIGEN_NOT_NULL(state);
    APIGEN_NOT_NULL(str1);
    APIGEN_NOT_NULL(str2);

    char const * const line_feed = "\n"; // doc strings always are separated by a single "\n"

    size_t const lf_len   = strlen(line_feed);
    size_t const str1_len = strlen(str1);
    size_t const str2_len = strlen(str2);

    size_t const total_len = lf_len + str1_len + str2_len;

    char * const output_string = apigen_memory_arena_alloc(state->ast_arena, total_len + 1);

    memcpy(output_string, str1, str1_len);
    memcpy(output_string + str1_len, line_feed, lf_len);
    memcpy(output_string + str1_len + lf_len, str2, str2_len);
    output_string[total_len] = 0;

    return output_string;
}

#define DEFINE_LIST_OPERATORS(_ListItem, _Prefix)                                                     \
    _ListItem * _Prefix##_init(struct apigen_ParserState * state, _ListItem item)                     \
    {                                                                                                 \
        APIGEN_NOT_NULL(state);                                                                       \
                                                                                                      \
        _ListItem * first = apigen_memory_arena_alloc(state->ast_arena, sizeof(_ListItem));           \
        *first            = item;                                                                     \
        first->next       = NULL;                                                                     \
                                                                                                      \
        return first;                                                                                 \
    }                                                                                                 \
                                                                                                      \
    _ListItem * _Prefix##_append(struct apigen_ParserState * state, _ListItem * list, _ListItem item) \
    {                                                                                                 \
        APIGEN_NOT_NULL(state);                                                                       \
                                                                                                      \
        _ListItem * new_item = apigen_memory_arena_alloc(state->ast_arena, sizeof(_ListItem));        \
        *new_item            = item;                                                                  \
        new_item->next       = NULL;                                                                  \
                                                                                                      \
        if (list == NULL) {                                                                           \
            return new_item;                                                                          \
        }                                                                                             \
                                                                                                      \
        _ListItem * iter = list;                                                                      \
        while (iter->next != NULL) {                                                                  \
            iter = iter->next;                                                                        \
        }                                                                                             \
                                                                                                      \
        APIGEN_ASSERT(iter->next == NULL);                                                            \
                                                                                                      \
        iter->next = new_item;                                                                        \
        return list;                                                                                  \
    }

DEFINE_LIST_OPERATORS(struct apigen_ParserEnumItem, apigen_parser_enum_item_list)
DEFINE_LIST_OPERATORS(struct apigen_ParserField, apigen_parser_field_list)
DEFINE_LIST_OPERATORS(struct apigen_ParserDeclaration, apigen_parser_file)

struct apigen_ParserType * apigen_parser_heapify_type(struct apigen_ParserState * state, struct apigen_ParserType type)
{
    APIGEN_NOT_NULL(state);

    struct apigen_ParserType * heapified = apigen_memory_arena_alloc(state->ast_arena, sizeof(struct apigen_ParserType));
    *heapified                           = type;
    return heapified;
}

char const * apigen_parser_conv_at_ident(struct apigen_ParserState * state, char const * at_identifier)
{
    APIGEN_NOT_NULL(state);
    APIGEN_NOT_NULL(at_identifier);

    size_t const at_identifier_len = strlen(at_identifier);
    APIGEN_ASSERT(at_identifier_len >= 3);
    APIGEN_ASSERT(at_identifier[0] == '@');
    APIGEN_ASSERT(at_identifier[1] == '\"');
    APIGEN_ASSERT(at_identifier[at_identifier_len - 1] == '\"');

    // Kind of a hack, but also very convenient:
    // @-Identifiers are basically just a regular string literal prefixed with an @, so we
    // can just apply the same conversion rules:
    struct apigen_Value converted_name = apigen_parser_conv_regular_str(state, at_identifier + 1);
    APIGEN_ASSERT(converted_name.type == apigen_value_str);
    return converted_name.value_str;
}
