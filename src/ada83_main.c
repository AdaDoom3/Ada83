///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   C O M P I L E R                        --
///                                                                           --
///                     M A I N   P R O G R A M   E N T R Y                   --
///                                                                           --
///  This is the main entry point for the Ada83 compiler. It handles:         --
///                                                                           --
///    - Command line argument parsing                                        --
///    - Source file loading                                                  --
///    - Include path management                                              --
///    - Compilation unit processing                                          --
///    - LLVM IR code generation (default) or interpretation                  --
///                                                                           --
///  Usage: ada83 [options] source_file [arguments...]                        --
///                                                                           --
///  Options:                                                                 --
///    -I path    Add include path for library units                          --
///    -i         Interpret mode (direct execution instead of LLVM IR)        --
///    -v         Verbose output                                              --
///    --help     Show usage information                                      --
///                                                                           --
///  By default, outputs LLVM IR to stdout (like the original ada83.c).       --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_common.h"
#include "ada83_arena.h"
#include "ada83_lexer.h"
#include "ada83_parser.h"
#include "ada83_ast.h"
#include "ada83_types.h"
#include "ada83_symbols.h"
#include "ada83_eval.h"
#include "ada83_codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


///-----------------------------------------------------------------------------
///                   G L O B A L   C O N F I G U R A T I O N
///-----------------------------------------------------------------------------

#define MAX_INCLUDE_PATHS 32

static const char *include_paths[MAX_INCLUDE_PATHS];
static int include_path_count = 0;

static bool interpret_mode = false;   ///< If true, interpret instead of codegen
static bool verbose = false;


///-----------------------------------------------------------------------------
///                   U S A G E   I N F O R M A T I O N
///-----------------------------------------------------------------------------

static void Print_Usage(const char *program_name)
{
    fprintf(stderr,
        "Ada83 Compiler - Literate Implementation\n"
        "\n"
        "Usage: %s [options] source_file [arguments...]\n"
        "\n"
        "Options:\n"
        "  -I path    Add include path for WITH'ed library units\n"
        "  -i         Interpret mode (execute directly instead of generating LLVM IR)\n"
        "  -v         Verbose output (show compilation phases)\n"
        "  --help     Show this usage information\n"
        "  --version  Show version information\n"
        "\n"
        "By default, outputs LLVM IR to stdout.\n"
        "The source file should be a valid Ada83 compilation unit.\n"
        "\n"
        "Example:\n"
        "  %s hello.ada > hello.ll     # Generate LLVM IR\n"
        "  %s -i hello.ada             # Interpret directly\n"
        "  %s -I ./libs main.ada       # Compile with library path\n"
        "\n",
        program_name, program_name, program_name, program_name);
}


static void Print_Version(void)
{
    fprintf(stderr, "Ada83 Compiler v1.0\n"
           "Literate-style implementation based on Ada83 LRM\n"
           "Modular architecture with GNAT-style naming conventions\n"
           "Outputs LLVM IR for execution via llvm-link and lli\n");
}


///-----------------------------------------------------------------------------
///                   F I L E   L O A D I N G
///-----------------------------------------------------------------------------

/// @brief Load a source file into memory
/// @param filename Path to the source file
/// @param out_size Output parameter for file size
/// @return Allocated buffer with file contents (NULL on failure)
///
static char *Load_Source_File(const char *filename, size_t *out_size)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }

    /// Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fprintf(stderr, "Error: Cannot determine size of '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    /// Allocate buffer
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Cannot allocate memory for '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    /// Read file
    size_t bytes_read = fread(buffer, 1, size, file);
    fclose(file);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Error: Failed to read '%s'\n", filename);
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    *out_size = size;

    return buffer;
}


/// @brief Search for a library unit in include paths
/// @param unit_name Name of the library unit
/// @return Loaded source (NULL if not found)
///
static char *Find_Library_Unit(String_Slice unit_name, size_t *out_size,
                              const char **out_filename)
{
    static char path_buffer[512];
    static const char *extensions[] = {".ada", ".adb", ".ads", NULL};

    /// Convert unit name to lowercase filename
    char name_lower[256];
    size_t name_len = unit_name.length < 255 ? unit_name.length : 255;
    for (size_t i = 0; i < name_len; i++) {
        char c = unit_name.data[i];
        name_lower[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }
    name_lower[name_len] = '\0';

    /// Search in include paths
    for (int i = 0; i < include_path_count; i++) {
        for (int e = 0; extensions[e]; e++) {
            snprintf(path_buffer, sizeof(path_buffer), "%s/%s%s",
                    include_paths[i], name_lower, extensions[e]);

            struct stat st;
            if (stat(path_buffer, &st) == 0) {
                char *source = Load_Source_File(path_buffer, out_size);
                if (source) {
                    *out_filename = path_buffer;
                    return source;
                }
            }
        }
    }

    /// Search in current directory
    for (int e = 0; extensions[e]; e++) {
        snprintf(path_buffer, sizeof(path_buffer), "%s%s",
                name_lower, extensions[e]);

        struct stat st;
        if (stat(path_buffer, &st) == 0) {
            char *source = Load_Source_File(path_buffer, out_size);
            if (source) {
                *out_filename = path_buffer;
                return source;
            }
        }
    }

    return NULL;
}


///-----------------------------------------------------------------------------
///                   C O M P I L A T I O N   P H A S E S
///-----------------------------------------------------------------------------

/// @brief Parse a source file into an AST
/// @param source Source text
/// @param length Source length
/// @param filename Source filename (for error messages)
/// @return Parsed compilation unit (NULL on failure)
///
static AST_Node *Parse_Source(const char *source, size_t length,
                             const char *filename)
{
    Parser_State parser = Parser_Init(source, length, filename);
    AST_Node *unit = Parse_Compilation_Unit(&parser);

    if (parser.error_count > 0) {
        return NULL;
    }

    return unit;
}


/// @brief Perform semantic analysis on a compilation unit
/// @param sem Semantic context
/// @param unit Parsed compilation unit
/// @return true if analysis succeeded
///
static bool Analyze_Source(Semantic_Context *sem, AST_Node *unit,
                          const char *filename)
{
    (void)filename;  // Used in verbose mode comments below

    /// Process WITH clauses - load library units
    if (unit->kind == N_CU && unit->comp_unit.context) {
        AST_Node *ctx = unit->comp_unit.context;

        for (uint32_t i = 0; i < ctx->context.with_clauses.count; i++) {
            AST_Node *with = ctx->context.with_clauses.data[i];
            String_Slice unit_name = with->with_clause.unit_name;

            /// Check if already loaded
            Symbol_Entry *existing = Symbol_Find(sem, unit_name);
            if (existing && existing->kind == SK_PACKAGE) {
                continue;  // Already loaded
            }

            /// Try to load the library unit
            size_t lib_size;
            const char *lib_filename;
            char *lib_source = Find_Library_Unit(unit_name, &lib_size, &lib_filename);

            if (lib_source) {
                AST_Node *lib_unit = Parse_Source(lib_source, lib_size, lib_filename);
                if (lib_unit) {
                    Analyze_Compilation_Unit(sem, lib_unit);
                }
                free(lib_source);
            }
        }
    }

    /// Analyze the main unit
    Analyze_Compilation_Unit(sem, unit);

    return true;
}


/// @brief Find the main procedure in a compilation unit
/// @param unit Compilation unit
/// @return Main procedure body node (NULL if not found)
///
static AST_Node *Find_Main_Procedure(AST_Node *unit)
{
    if (!unit || unit->kind != N_CU) return NULL;

    for (uint32_t i = 0; i < unit->comp_unit.units.count; i++) {
        AST_Node *u = unit->comp_unit.units.data[i];

        if (u->kind == N_PB) {
            /// A parameterless library-level procedure is a potential main
            AST_Node *spec = u->subprog_body.spec;
            if (spec && spec->subprog_spec.params.count == 0) {
                return u;
            }
        }
    }

    return NULL;
}


/// @brief Execute the main procedure (interpret mode)
/// @param ctx Evaluation context
/// @param main_proc Main procedure AST node
///
static void Execute_Main(Eval_Context *ctx, AST_Node *main_proc,
                        const char *filename)
{
    (void)filename;
    Exec_Call(ctx, main_proc, NULL);
}


///-----------------------------------------------------------------------------
///                   M A I N   E N T R Y   P O I N T
///-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    const char *source_file = NULL;

    /// Add current directory as default include path
    include_paths[include_path_count++] = ".";

    /// Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            Print_Usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--version") == 0) {
            Print_Version();
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            interpret_mode = true;
        }
        else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 < argc) {
                if (include_path_count < MAX_INCLUDE_PATHS) {
                    include_paths[include_path_count++] = argv[++i];
                }
                else {
                    fprintf(stderr, "Warning: Too many include paths\n");
                    i++;
                }
            }
            else {
                fprintf(stderr, "Error: -I requires an argument\n");
                return 1;
            }
        }
        else if (strncmp(argv[i], "-I", 2) == 0) {
            /// -Ipath format
            if (include_path_count < MAX_INCLUDE_PATHS) {
                include_paths[include_path_count++] = argv[i] + 2;
            }
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            Print_Usage(argv[0]);
            return 1;
        }
        else if (!source_file) {
            source_file = argv[i];
        }
        /// Additional arguments after source_file are program arguments
    }

    if (!source_file) {
        fprintf(stderr, "Error: No source file specified\n");
        Print_Usage(argv[0]);
        return 1;
    }

    /// Load source file
    size_t source_size;
    char *source = Load_Source_File(source_file, &source_size);
    if (!source) {
        return 1;
    }

    /// Parse source
    AST_Node *unit = Parse_Source(source, source_size, source_file);
    if (!unit) {
        free(source);
        return 1;
    }

    /// Initialize semantic context
    Semantic_Context sem;
    Semantic_Init(&sem);

    /// Initialize types
    Types_Initialize(&sem);

    /// Analyze source
    if (!Analyze_Source(&sem, unit, source_file)) {
        free(source);
        return 1;
    }

    if (interpret_mode) {
        /// Interpretation mode - execute directly
        Eval_Context eval;
        Eval_Init(&eval, &sem);

        AST_Node *main_proc = Find_Main_Procedure(unit);

        if (main_proc) {
            Execute_Main(&eval, main_proc, source_file);
        }
        else {
            /// Elaborate package bodies
            Elaborate_Compilation_Unit(&eval, unit);
        }

        Eval_Cleanup(&eval);
    }
    else {
        /// Default: Code generation mode - output LLVM IR to stdout
        Codegen_Context codegen;
        Codegen_Init(&codegen, stdout, &sem);

        Codegen_Compilation_Unit(&codegen, unit);

        Codegen_Cleanup(&codegen);
    }

    /// Cleanup
    free(source);

    return 0;
}


///-----------------------------------------------------------------------------
///                                  E N D                                    --
///-----------------------------------------------------------------------------
