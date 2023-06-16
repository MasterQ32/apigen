#include "apigen.h"
#include "parser/parser.h"

#include <stdio.h>

static bool is_unique_type(enum apigen_ParserTypeId id) {
    switch(id) {
        case apigen_parser_type_named:                      return false;
        case apigen_parser_type_enum:                       return true;
        case apigen_parser_type_struct:                     return true;
        case apigen_parser_type_union:                      return true;
        case apigen_parser_type_opaque:                     return true;
        case apigen_parser_type_array:                      return false;
        case apigen_parser_type_ptr_to_one:                 return false;
        case apigen_parser_type_ptr_to_many:                return false;
        case apigen_parser_type_ptr_to_many_sentinelled:    return false;
        case apigen_parser_type_function:                   return false;
    }
}

static struct apigen_Type const * resolve_type(struct apigen_TypePool * pool, struct apigen_ParserType const * src_type)
{
    (void)pool;
    (void)src_type;

    switch(src_type->type) {
        case apigen_parser_type_named:
            return apigen_lookup_type(pool,src_type->named_data);
            
        case apigen_parser_type_array: {
            apigen_panic("resolving array not implemented yet!");
        }
        case apigen_parser_type_ptr_to_one: {
            apigen_panic("resolving ptr_to_one not implemented yet!");
        }
        case apigen_parser_type_ptr_to_many: {
            apigen_panic("resolving ptr_to_many not implemented yet!");
        }
        case apigen_parser_type_ptr_to_many_sentinelled: {
            apigen_panic("resolving ptr_to_many_sentinelled not implemented yet!");
        }
        case apigen_parser_type_function: {
            apigen_panic("resolving function not implemented yet!");
        }
        case apigen_parser_type_enum: 
        case apigen_parser_type_struct:
        case apigen_parser_type_union: 
        case apigen_parser_type_opaque: { 
            apigen_panic("cannot resolve unique type in this part of the code!");
        }
    }

    return NULL;

}

bool apigen_analyze(struct apigen_ParserState * const state, struct apigen_Document * const out_document)
{
    APIGEN_NOT_NULL(state);
    APIGEN_NOT_NULL(out_document);

    APIGEN_ASSERT(state->top_level_declarations != NULL);

    *out_document = (struct apigen_Document) {
        .type_pool = {
            .arena = state->ast_arena,
        },

        .type_count     = 0,
        .types          = NULL,

        .function_count = 0,
        .functions      = NULL,

        .variable_count = 0,
        .variables      = NULL,

        .constant_count = 0,
        .constants      = NULL,
    };

    // Phase 1: Figure out how much memory we need for all exported declarations:
    {
        struct apigen_ParserDeclaration const * decl = state->top_level_declarations;
        while(decl != NULL) {
            switch(decl->kind) {
                case apigen_parser_const_declaration:
                case apigen_parser_var_declaration:
                    out_document->variable_count += 1;
                    break;

                case apigen_parser_constexpr_declaration:
                    out_document->constant_count += 1;
                    break;

                case apigen_parser_fn_declaration:
                    out_document->function_count += 1;
                    break;

                case apigen_parser_type_declaration:
                    out_document->type_count += 1;
                    break;
            }
            decl = decl->next;
        }

        out_document->types     = apigen_memory_arena_alloc(state->ast_arena, out_document->type_count     * sizeof(struct apigen_Type const *));
        out_document->functions = apigen_memory_arena_alloc(state->ast_arena, out_document->function_count * sizeof(struct apigen_Type const *));
        out_document->variables = apigen_memory_arena_alloc(state->ast_arena, out_document->variable_count * sizeof(struct apigen_Global));
        out_document->constants = apigen_memory_arena_alloc(state->ast_arena, out_document->constant_count * sizeof(struct apigen_Constant));
    }

    // Phase 2: Publish all named unique types (struct, union, ...) into the pool
    {
        bool ok = true;

        struct apigen_ParserDeclaration * decl = state->top_level_declarations;
        while(decl != NULL) {
            if(decl->kind == apigen_parser_type_declaration) {
                if(is_unique_type( decl->type.type)) {
                    struct apigen_Type * const unique_type = apigen_memory_arena_alloc(state->ast_arena, sizeof(struct apigen_Type));
                    *unique_type = (struct apigen_Type) {
                        .name = decl->identifier,
                    };

                    if(!apigen_register_type(&out_document->type_pool, unique_type, NULL)) {
                        fprintf(stderr, "duplicate type: %s\n", unique_type->name);
                        ok = false;
                    }

                    decl->associated_type = unique_type;
                }
            }
            decl = decl->next;
        }
        if(!ok) {
            return false;
        }
    }

    // Phase 3: Resolve and publish all global non-unique types (pointers, aliases, ...)
    {
        size_t resolved_count;
        size_t resolve_failed_count;
        bool non_resolve_error;
        do {
            resolved_count = 0;
            resolve_failed_count = 0;
            non_resolve_error = false;

            struct apigen_ParserDeclaration * decl = state->top_level_declarations;
            while(decl != NULL) {
                if(decl->kind == apigen_parser_type_declaration) {
                    if(decl->associated_type == NULL) {
                        APIGEN_ASSERT(!is_unique_type( decl->type.type));
                    
                        // TODO: Try resolve primitive type here, and retry next loop if it wasn't successful
                        struct apigen_Type const * const resolved_type = resolve_type(&out_document->type_pool, &decl->type);
                        if(resolved_type != NULL) {


                            if(apigen_register_type(&out_document->type_pool, resolved_type, decl->identifier)) {
                                decl->associated_type = (struct apigen_Type *)resolved_type;
                                resolved_count += 1;
                            }
                            else {
                                fprintf(stderr, "duplicate type: %s\n", decl->identifier);
                                non_resolve_error = true;
                            }

                        }
                        else {
                            resolve_failed_count += 1;
                        }
                    }
                }
                decl = decl->next;
            }
        } while(resolved_count > 0 && !non_resolve_error);

        if(non_resolve_error) {
            return false;
        }

        if(resolve_failed_count > 0) {
            fprintf(stderr, "found %zu cyclic dependencies or undeclared types.\n", resolve_failed_count);
            return false;
        }

    }

    // Phase 4: Now resolve all unique types
    {
        struct apigen_ParserDeclaration const * decl = state->top_level_declarations;
        while(decl != NULL) {
            if(decl->kind == apigen_parser_type_declaration) {

                
                
            }
            decl = decl->next;
        }
    }

    // Phase 5: Store all declared type into the document
    {
        size_t index = 0;
        struct apigen_ParserDeclaration const * decl = state->top_level_declarations;
        while(decl != NULL) {
            if(decl->kind == apigen_parser_type_declaration) {
                APIGEN_ASSERT(decl->associated_type != NULL);
                out_document->types[index] = decl->associated_type;
                index += 1;
            }
            decl = decl->next;
        }
        APIGEN_ASSERT(index == out_document->type_count);
    }

    return true;
}