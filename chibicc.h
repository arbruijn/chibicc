#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#ifndef __has_attribute
#define __has_attribute(x) __GCC4_has_attribute_##x
#endif

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#ifndef __GNUC__
#define __attribute__(x)
#endif

#define PRODUCT "chibicc"
#define VERSION "1.0.21"
#define MAXLEN 101
#define DEFAULT_TARGET_MACHINE "x86_64-linux-gnu"

#define HELP PRODUCT " is a C compiler based on " PRODUCT " created by Rui Ueyama.\n \
See original project https://github.com/rui314/chibicc for more information\n \
this " PRODUCT " contains only some differences for now like new parameters\n"

#define USAGE PRODUCT " usage :\n \
--help or -h print the help\n \
--version or -v print the version of " PRODUCT "\n \
-cc1 run the cc1 function needs -cc1-input (-cc1-output optional) parameter \n \
-fuse-ld to specify other linker than ld used by default \n \
-x Specify the language of the following input files.\n \
    Permissible languages include: c assembler none\n \
    'none' means revert to the default behavior of\n \
    guessing the language based on the file's extension.\n \
-S generate assembly file \n \
-o path to output executable if omitted a.out generated\n \
-c path to source to compile \n \
-Xlinker <arg> Pass <arg> on to the linker.\n \
-Wl,<options> Pass comma-separated <options> on to the linker.\n \
-z <arg> Pass <arg> on to the linker. \n \
-soname <arg> Pass -soname <arg> on to the linker. \n \
--version-script <arg> Pass --version-script <arg> to the linker.\n \
-I<path> Pass path to the include directories \n \
-L<path> Pass path to the lib directories \n \
-D<macro> define macro example -DM13 \n \
-U<macro> undefine macro example -UM13\n \
-s to strip all symbols during linkage phasis \n \
-M -MD -MP -MMD -MF <arg> -MT <arg> -MQ <arg> compiler write a list of input files to \n \
    stdout in a format that \"make\" command can read. This feature is\n \
    used to automate file dependency management\n \
-fpic or -fPIC Generate position-independent code (PIC)\n \
-fno-pic disables the generation of position-independent code with relative address references\n \
-fcommon is the default if not specified, it's mainly useful to enable legacy code to link without errors\n \
-fno-common specifies that the compiler places uninitialized global variables in the BSS section of the object file.\n \
-static  pass to the linker to link a program statically\n \
-pthread pass to the linker to link with lpthread library \n \
-shared pass to the linker to produce a shared object which can then be linked with other objects to form an executable.\n \
-hashmap-test to test the hashmap function \n \
-idirafter <dir> apply to lookup for both the #include \"file\" and #include <file> directives.\n \
-### to dump all commands executed by chibicc \n \
-debug to dump all commands executed by chibicc in a log file in /tmp/chibicc.log\n \
-E Stop after the preprocessing stage; do not run the compiler proper. \n \
    The output is in the form of preprocessed source code, which is sent to the standard output.\n \
    Input files that don’t require preprocessing are ignored.\n \
-rpath <dir> Add a directory to the runtime library search path this parameter is passed to the linker. \n \
    This is used when linking an ELF executable with shared objects.\n \
    All -rpath arguments are concatenated and passed to the runtime linker,\n \
    which uses them to locate shared objects at runtime. \n \
    The -rpath option is also used when locating shared objects \n \
    which are needed by shared objects explicitly included in the link. \n \
-dumpmachine it's required by some projects returns x86_64-linux-gnu\n \
-dotfile generates a file with .dot extension that can be visualized using graphviz package \n \
-dM Print macro definitions in -E mode instead of normal output\n \
chibicc [ -o <path> ] <file>\n"

typedef struct Type Type;
typedef struct Node Node;
typedef struct Member Member;
typedef struct Relocation Relocation;
typedef struct Hideset Hideset;


typedef struct
{
  char *filename;
  char *funcname;
  int line_no;
} Context;

typedef Context Context;

//
// strings.c
//

typedef struct
{
  char **data;
  int capacity;
  int len;
} StringArray;

void strarray_push(StringArray *arr, char *s);
char *format(char *fmt, ...) __attribute__((format(printf, 1, 2)));

//
// tokenize.c
//

// Token
typedef enum
{
  TK_IDENT,   // Identifiers
  TK_PUNCT,   // Punctuators
  TK_KEYWORD, // Keywords
  TK_STR,     // String literals
  TK_NUM,     // Numeric literals
  TK_PP_NUM,  // Preprocessing numbers
  TK_EOF,     // End-of-file markers
} TokenKind;

typedef struct
{
  char *name;
  int file_no;
  char *contents;

  // For #line directive
  char *display_name;
  int line_delta;
} File;

// Token type
typedef struct Token Token;
struct Token
{
  TokenKind kind;   // Token kind
  Token *next;      // Next token
  int64_t val;      // If kind is TK_NUM, its value
  long double fval; // If kind is TK_NUM, its value
  char *loc;        // Token location
  int len;          // Token length
  Type *ty;         // Used if TK_NUM or TK_STR
  char *str;        // String literal contents including terminating '\0'

  File *file;       // Source location
  char *filename;   // Filename
  int line_no;      // Line number
  int line_delta;   // Line number
  bool at_bol;      // True if this token is at beginning of line
  bool has_space;   // True if this token follows a space character
  Hideset *hideset; // For macro expansion
  Token *origin;    // If this is expanded from a macro, the original token
};

noreturn void error(char *fmt, ...) __attribute__((format(printf, 1, 2)));
noreturn void error_at(char *loc, char *fmt, ...) __attribute__((format(printf, 2, 3)));
noreturn void error_tok(Token *tok, char *fmt, ...) __attribute__((format(printf, 2, 3)));
void warn_tok(Token *tok, char *fmt, ...) __attribute__((format(printf, 2, 3)));
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op, Context *ctx);
bool consume(Token **rest, Token *tok, char *str);
void convert_pp_tokens(Token *tok);
File **get_input_files(void);
File *new_file(char *name, int file_no, char *contents);
Token *tokenize_string_literal(Token *tok, Type *basety);
Token *tokenize(File *file);
Token *tokenize_file(char *filename);
bool startswith(char *p, char *q);

#define unreachable() \
  error("internal error at %s:%d", __FILE__, __LINE__)

//
// preprocess.c
//

char *search_include_paths(char *filename);
void init_macros(void);
void define_macro(char *name, char *buf);
void undef_macro(char *name);
Token *preprocess(Token *tok, bool isReadLine);
Token *preprocess3(Token *tok);


//
// parse.c
//

// Variable or function
typedef struct Obj Obj;
struct Obj
{
  Obj *next;
  char *name;     // Variable name
  char *funcname; // function name
  Type *ty;       // Type
  Token *tok;     // representative token
  bool is_local;  // local or global/function
  int align;      // alignment

  // Local variable
  int offset;
  int order;
  int nbparm;
  // Global variable or function
  bool is_function;
  bool is_definition;
  bool is_static;

  // Global variable
  bool is_tentative;
  bool is_tls;
  char *init_data;
  Relocation *rel;

  // Function
  bool is_inline;
  Obj *params;
  Node *body;
  Obj *locals;
  Obj *va_area;
  Obj *alloca_bottom;
  int stack_size;

  // Static inline function
  bool is_live;
  bool is_root;
  StringArray refs;
};

// Global variable can be initialized either by a constant expression
// or a pointer to another global variable. This struct represents the
// latter.
typedef struct Relocation Relocation;
struct Relocation
{
  Relocation *next;
  int offset;
  char **label;
  long addend;
};

// AST node
typedef enum
{
  ND_NULL_EXPR, // Do nothing
  ND_ADD,       // +
  ND_SUB,       // -
  ND_MUL,       // *
  ND_DIV,       // /
  ND_NEG,       // unary -
  ND_MOD,       // %
  ND_BITAND,    // &
  ND_BITOR,     // |
  ND_BITXOR,    // ^
  ND_SHL,       // <<
  ND_SHR,       // >>
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LT,        // <
  ND_LE,        // <=
  ND_ASSIGN,    // =
  ND_COND,      // ?:
  ND_COMMA,     // ,
  ND_MEMBER,    // . (struct member access)
  ND_ADDR,      // unary &
  ND_DEREF,     // unary *
  ND_NOT,       // !
  ND_BITNOT,    // ~
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_RETURN,    // "return"
  ND_IF,        // "if"
  ND_FOR,       // "for" or "while"
  ND_DO,        // "do"
  ND_SWITCH,    // "switch"
  ND_CASE,      // "case"
  ND_BLOCK,     // { ... }
  ND_GOTO,      // "goto"
  ND_GOTO_EXPR, // "goto" labels-as-values
  ND_LABEL,     // Labeled statement
  ND_LABEL_VAL, // [GNU] Labels-as-values
  ND_FUNCALL,   // Function call
  ND_EXPR_STMT, // Expression statement
  ND_STMT_EXPR, // Statement expression
  ND_VAR,       // Variable
  ND_VLA_PTR,   // VLA designator
  ND_NUM,       // Integer
  ND_CAST,      // Type cast
  ND_MEMZERO,   // Zero-clear a stack variable
  ND_ASM,       // "asm"
  ND_CAS,       // Atomic compare-and-swap
  ND_EXCH,      // Atomic exchange
} NodeKind;

// AST node type
struct Node
{
  NodeKind kind; // Node kind
  Node *next;    // Next node
  Type *ty;      // Type, e.g. int or pointer to int
  Token *tok;    // Representative token

  Node *lhs; // Left-hand side
  Node *rhs; // Right-hand side

  // "if" or "for" statement
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // "break" and "continue" labels
  char *brk_label;
  char *cont_label;

  // Block or statement expression
  Node *body;

  // Struct member access
  Member *member;

  // Function call
  Type *func_ty;
  Node *args;
  bool pass_by_stack;
  Obj *ret_buffer;

  // Goto or labeled statement, or labels-as-values
  char *label;
  char *unique_label;
  Node *goto_next;

  // Switch
  Node *case_next;
  Node *default_case;

  // Case
  long begin;
  long end;

  // "asm" string literal
  char *asm_str;

  // Atomic compare-and-swap
  Node *cas_addr;
  Node *cas_old;
  Node *cas_new;

  // Atomic op= operators
  Obj *atomic_addr;
  Node *atomic_expr;

  // Atomic fetch operation
  bool atomic_fetch;

  // Variable
  Obj *var;

  // Numeric literal
  int64_t val;
  long double fval;
  // for dot diagram
  int unique_number;
};

typedef struct
{
  Obj *var;
  Type *type_def;
  Type *enum_ty;
  int enum_val;
} VarScope;

Node *new_cast(Node *expr, Type *ty);
int64_t const_expr(Token **rest, Token *tok);
Obj *parse(Token *tok);
VarScope *find_var(Token *tok);
Obj *find_func(char *name);

extern bool opt_fbuiltin;
//
// type.c
//

typedef enum
{
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_FLOAT,
  TY_DOUBLE,
  TY_LDOUBLE,
  TY_ENUM,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
  TY_VLA, // variable-length array
  TY_STRUCT,
  TY_UNION,
} TypeKind;

struct Type
{
  TypeKind kind;
  int size;          // sizeof() value
  int align;         // alignment
  bool is_unsigned;  // unsigned or signed
  bool is_atomic;    // true if _Atomic
  bool is_pointer;   // true if it's a pointer
  Type *pointertype; // store the pointer type int, char...
  Type *origin;      // for type compatibility check

  // Pointer-to or array-of type. We intentionally use the same member
  // to represent pointer/array duality in C.
  //
  // In many contexts in which a pointer is expected, we examine this
  // member instead of "kind" member to determine whether a type is a
  // pointer or not. That means in many contexts "array of T" is
  // naturally handled as if it were "pointer to T", as required by
  // the C spec.
  Type *base;

  // Declaration
  Token *name;
  Token *name_pos;

  // Array
  int array_len;

  // Variable-length array
  Node *vla_len; // # of elements
  Obj *vla_size; // sizeof() value

  // Struct
  Member *members;
  bool is_flexible;
  bool is_packed;

  // Function type
  Type *return_ty;
  Type *params;
  bool is_variadic;
  Type *next;
};

// Struct member
struct Member
{
  Member *next;
  Type *ty;
  Token *tok; // for error message
  Token *name;
  int idx;
  int align;
  int offset;

  // Bitfield
  bool is_bitfield;
  int bit_offset;
  int bit_width;
};

extern Type *ty_void;
extern Type *ty_bool;

extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;

extern Type *ty_uchar;
extern Type *ty_ushort;
extern Type *ty_uint;
extern Type *ty_ulong;

extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;

bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_compatible(Type *t1, Type *t2);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
Type *vla_of(Type *base, Node *expr);
Type *enum_type(void);
Type *struct_type(void);
void add_type(Node *node);


char *nodekind2str(NodeKind kind);

//
// debug.c
//

char *tokenkind2str(TokenKind kind);
void print_debug_tokens(char *currentfilename, char *function, Token *tok);

//
// codegen.c
//

void codegen(Obj *prog, FILE *out);
int align_to(int n, int align);
char *reg_ax(int sz);
char *reg_bx(int sz);
char *reg_cx(int sz);
char *reg_dx(int sz);
char *reg_di(int sz);
char *reg_si(int sz);
char *reg_r8w(int sz);
char *reg_r9w(int sz);
void assign_lvar_offsets_assembly(Obj *fn);
int add_register_used(char *regist);
void clear_register_used();
char *register32_to_64(char *regist);
char *register16_to_64(char *regist);
char *register8_to_64(char *regist);
char *register_available();  
char *specific_register_available(char *regist); 
bool check_register_used(char *regist);
void check_register_in_template(char *template); 

//
// unicode.c
//

int encode_utf8(char *buf, uint32_t c);
uint32_t decode_utf8(char **new_pos, char *p);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
bool is_ident3(uint32_t c); // to fix issue #117
int display_width(char *p, int len);

//
// hashmap.c
//

typedef struct
{
  char *key;
  int keylen;
  void *val;
} HashEntry;

typedef struct
{
  HashEntry *buckets;
  int capacity;
  int used;
} HashMap;

void *hashmap_get(HashMap *map, char *key);
void *hashmap_get2(HashMap *map, char *key, int keylen);
void hashmap_put(HashMap *map, char *key, void *val);
void hashmap_put2(HashMap *map, char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, char *key);
void hashmap_delete2(HashMap *map, char *key, int keylen);
void hashmap_test(void);

//
// main.c
//

bool file_exists(char *path);
void dump_machine(void);

extern StringArray include_paths;
extern bool opt_fpic;
extern bool opt_fcommon;
extern char *base_file;
extern char *dot_file;
extern char *opt_o;
extern char *replace_extn(char *tmpl, char *extn);
extern FILE *dotf;
extern FILE *f;
extern bool isDotfile;
extern bool isDebug;
extern bool isPrintMacro;
extern char *extract_filename(char *tmpl);
extern char *extract_path(char *tmpl, char *basename);

//
// extended_asm.c
//

char *extended_asm(Node *node, Token **rest, Token *tok, Obj *locals);
void output_asm(Node *node, Token **rest, Token *tok, Obj *locals);
void input_asm(Node *node, Token **rest, Token *tok, Obj *locals);
char *subst_asm(char *template, char *output_str, char *input_str);
char *string_replace(char *str, char *oldstr, char *newstr);
char *generate_input_asm(char *input_str);
bool check_template(char *template);
int search_output_index(char c);
char *int_to_string(int i);
void update_offset(char *funcname, Obj *locals);
char *load_variable(int order);
char *generate_output_asm(char *output_str);
char *opcode(int size);
char *update_register_size(char *reg, int size);
char *retrieve_output_index_str(char letter);
int retrieve_output_index_from_letter(char letter);

char *retrieveVariableNumber(int index);