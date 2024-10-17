#ifndef COMPILER_H
#define COMPILER_H

#define TOKEN_MAX 64
#include <inttypes.h>

#include "hashmap.h"

#define READ_BUFFER_SIZE      0x1000000
#define STRING_CONTENT_BUFFER  0x100000
#define POOL_SIZE             0x1000000
#define READ_TOKEN_CHUNK         0x1000
#define MAX_FUNCTIONS 10000
#define MAX_ALIASES    1000
#define MAX_IMPORTS     100
#define MAX_ARGS 16
#define MAX_CAPTURES 256
#define BLOCK_MAX 256
#define ERROR_BUFFER 512
#define MAX_MEMBERS  256
#define MAX_STACK_MEMBERS 10000
#define MAX_STRUCT_NESTING 8

struct pool;
typedef struct pool pool;

typedef enum TOKEN_TYPES {
	TOKEN_IDENTIFIER=0,
	TOKEN_SYNTACTIC,
	TOKEN_SYMBOL,
	TOKEN_INTEGER,
	TOKEN_FLOAT,
	TOKEN_LABEL,
	TOKEN_LABEL_JUMP,
	TOKEN_CHAR,
	TOKEN_STRING,
	TOKEN_TYPE_COUNT,
	TOKEN_IMPORT,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_FOR,
	TOKEN_MATCH,
	TOKEN_U8,
	TOKEN_U16,
	TOKEN_U32,
	TOKEN_U64,
	TOKEN_I8,
	TOKEN_I16,
	TOKEN_I32,
	TOKEN_I64,
	TOKEN_F32,
	TOKEN_F64,
	TOKEN_RETURN,
	TOKEN_ADD,
	TOKEN_SUB,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_FLADD,
	TOKEN_FLSUB,
	TOKEN_FLMUL,
	TOKEN_FLDIV,
	TOKEN_MOD,
	TOKEN_SHL,
	TOKEN_SHR,
	TOKEN_QUOTE,
	TOKEN_TICK,
	TOKEN_REF,
	TOKEN_ANGLE_OPEN,
	TOKEN_ANGLE_CLOSE,
	TOKEN_LESS_EQ,
	TOKEN_GREATER_EQ,
	TOKEN_EQ,
	TOKEN_NOT_EQ,
	TOKEN_FLANGLE_OPEN,
	TOKEN_FLANGLE_CLOSE,
	TOKEN_FLLESS_EQ,
	TOKEN_FLGREATER_EQ,
	TOKEN_FLEQ,
	TOKEN_FLNOT_EQ,
	TOKEN_BOOL_AND,
	TOKEN_BOOL_OR,
	TOKEN_BIT_AND,
	TOKEN_BIT_OR,
	TOKEN_BIT_XOR,
	TOKEN_BIT_COMP,
	TOKEN_BOOL_NOT,
	TOKEN_PIPE_LEFT,
	TOKEN_PIPE_RIGHT,
	TOKEN_COMPOSE,
	TOKEN_PASS,
	TOKEN_BRACK_OPEN,
	TOKEN_BRACK_CLOSE,
	TOKEN_SET,
	TOKEN_ARG,
	TOKEN_BRACE_OPEN,
	TOKEN_BRACE_CLOSE,
	TOKEN_PAREN_OPEN,
	TOKEN_PAREN_CLOSE,
	TOKEN_SEMI,
	TOKEN_FUNC_IMPL,
	TOKEN_ENCLOSE,
	TOKEN_COMMA,
	TOKEN_COMMENT,
	TOKEN_MULTI_OPEN,
	TOKEN_MULTI_CLOSE,
	TOKEN_TYPE,
	TOKEN_ALIAS,
	TOKEN_MUTABLE,
	TOKEN_PROC,
	TOKEN_CONST,
	TOKEN_CAST,
	TOKEN_SIZEOF,
	TOKEN_BREAK,
	TOKEN_CONTINUE,
	TOKEN_EOF
} TOKEN_TYPE_TAG;

MAP_DEF(TOKEN_TYPE_TAG)

typedef struct token {
	char* string;
	TOKEN_TYPE_TAG type;
	uint32_t len;
} token;

void show_token(const token* const tok);

typedef struct lexer {
	token* const tokens;
	uint64_t token_count;
	uint64_t index;
} lexer;

uint64_t parse_save(lexer* const lex, pool* const mem);
void parse_load(lexer* const lex, pool* const mem, uint64_t index);

void hash_keyword(TOKEN_TYPE_TAG_map* keywords, const char* key, TOKEN_TYPE_TAG value);
void add_keyword_hashes(TOKEN_TYPE_TAG_map* keywords);

uint64_t lex_char(token* const tok, uint64_t i, const char* const buffer, uint64_t size_bytes, char* err);
uint64_t lex_numeric(token* const tok, uint64_t i, const char* const buffer, uint64_t size_bytes);
token* lex_cstr(const char* const buffer, uint64_t size_bytes, pool* const mem, uint64_t* token_count, char** string_buffer, char* err);
int compile_file(char* filename);
int compile_cstr(pool* const read_buffer, uint64_t read_bytes);

uint32_t subtype(uint32_t type_index, char* const content);
uint8_t lex_identifier(const char* const string);
uint32_t identifier_subtype(uint32_t type_index, char* const content);

uint8_t lex_syntactic(const char* const string);
uint32_t syntactic_subtype(uint32_t type_index, char* const content);

uint8_t lex_symbol(const char* const string);
uint32_t symbol_subtype(uint32_t type_index, char* const content);

uint8_t lex_integer(const char* const string);
uint8_t lex_float(const char* const string);

typedef enum LABEL_REQUEST {
	LABEL_FULFILLED=0,
	LABEL_WAITING,
	LABEL_REQUESTED,
	LABEL_EXPIRED,
	LABEL_OVERDUE
} LABEL_REQUEST;

uint8_t lex_label(const char* const string);
uint8_t lex_label_jump(const char* const string);

uint32_t no_subtype(uint32_t type_index, char* const content);
uint8_t issymbol(char c);

struct new_type_ast;
struct function_ast;
struct expression_ast;
struct binding_ast;
struct structure_ast;

typedef struct structure_ast{
	struct binding_ast* binding_v;
	struct structure_ast* union_v;
	int64_t* encoding;
	token* tag_v;
	uint32_t binding_c;
	uint32_t union_c;
} structure_ast;

MAP_DEF(structure_ast)

uint8_t struct_cmp(structure_ast* const a, structure_ast* const b);
void show_structure(const structure_ast* const structure, uint8_t indent);

typedef enum PRIMITIVE_TAGS {
	U8_TYPE=0,
	U16_TYPE=1,
	U32_TYPE=2,
	U64_TYPE=3,
	I8_TYPE=4,
	I16_TYPE=5,
	I32_TYPE=6,
	I64_TYPE=7,
	INT_ANY=8,
	F32_TYPE=9,
	F64_TYPE=10,
	FLOAT_ANY=11
} PRIMITIVE_TAGS;

typedef struct type_ast {
	enum {
		FUNCTION_TYPE,
		PRIMITIVE_TYPE,
		POINTER_TYPE,
		BUFFER_TYPE,
		USER_TYPE,
		STRUCT_TYPE,
		PROCEDURE_TYPE,
		NONE_TYPE,
		INTERNAL_ANY_TYPE
	} tag;
	union {
		struct {
			struct type_ast* left;
			struct type_ast* right;
		} function;
		PRIMITIVE_TAGS primitive;
		struct type_ast* pointer; // also used for procedure
		structure_ast* structure;
		token user;
		struct {
			struct type_ast* base;
			uint32_t count;
			uint8_t constant;
			token const_binding;
		} buffer;
	} data;
	uint8_t mut;
} type_ast;

void show_type(const type_ast* const type);

typedef enum TYPE_CMP_PURPOSE{
	FOR_EQUALITY,
	FOR_MUTATION,
	FOR_APPLICATION
}TYPE_CMP_PURPOSE;

uint8_t type_cmp(type_ast* const a, type_ast* const b, TYPE_CMP_PURPOSE purpose);

typedef struct binding_ast{
	type_ast type;
	token name;
} binding_ast, new_type_ast, alias_ast;

typedef struct constant_ast {
	binding_ast value;
	token name;
} constant_ast;

void show_binding(const binding_ast* const binding);

typedef struct statement_ast{
	enum {
		IF_STATEMENT,
		FOR_STATEMENT,
		BREAK_STATEMENT,
		CONTINUE_STATEMENT
	} tag;
	union {
		struct {
			struct expression_ast* predicate;
			struct expression_ast* branch;
			struct expression_ast* alternate;
		} if_statement;
		struct {
			struct expression_ast* start;
			struct expression_ast* end;
			struct expression_ast* inc;
			struct expression_ast* procedure;
		} for_statement;
	} data;
	type_ast type;
	uint8_t labeled;
	binding_ast label;
} statement_ast;

void show_statement(const statement_ast* const statement, uint8_t indent);

typedef struct lambda_ast {
	type_ast type;
	token* argv;
	struct expression_ast* expression;
	uint32_t argc;
}lambda_ast;

void show_lambda(const lambda_ast* const lambda);

typedef struct literal_ast{
	type_ast type;
	union {
		struct {
			char* content;
			uint32_t length;
		} string;
		struct {
			struct expression_ast* member_v;
			uint32_t member_c;
		} array; // also used for struct
	} data;
	enum {
		STRING_LITERAL,
		ARRAY_LITERAL,
		STRUCT_LITERAL
	} tag;
} literal_ast;

void show_literal(const literal_ast* const lit);

typedef struct expression_ast{
	enum {
		BLOCK_EXPRESSION,
		CLOSURE_EXPRESSION,
		APPLICATION_EXPRESSION,
		STATEMENT_EXPRESSION,
		BINDING_EXPRESSION,
		VALUE_EXPRESSION,
		LITERAL_EXPRESSION,
		DEREF_EXPRESSION,
		ACCESS_EXPRESSION,
		LAMBDA_EXPRESSION,
		RETURN_EXPRESSION,
		REF_EXPRESSION,
		CAST_EXPRESSION,
		SIZEOF_EXPRESSION,
		NOP_EXPRESSION
	} tag;
	union {
		struct {
			type_ast type;
			struct expression_ast* expr_v;
			uint32_t expr_c;
		} block; // also used for application
		struct {
			binding_ast* capture_v;
			struct function_ast* func;
			uint32_t capture_c;
		} closure;
		struct expression_ast* deref; // also used for access, return, ref
		statement_ast statement;
		binding_ast binding; // also used for value, union in token, allows type checking
		lambda_ast lambda;
		literal_ast literal;
		struct {
			struct expression_ast* target;
			type_ast type;
		} cast;
		struct {
			type_ast type;
			struct expression_ast* target;
			uint64_t size;
		} size_of;
	} data;
} expression_ast;

void show_expression(const expression_ast* const expr, uint8_t indent);

MAP_DEF(new_type_ast)
MAP_DEF(alias_ast)
MAP_DEF(constant_ast)

void show_new_type(const new_type_ast* const new_type);
void show_alias(const alias_ast* const alias);
void show_constant(const constant_ast* const cnst);

typedef struct function_ast {
	expression_ast expression;
	type_ast type;
	token name;
	uint8_t enclosing;
} function_ast;

MAP_DEF(function_ast)

void show_function(const function_ast* const func);

typedef struct ast{
	token* import_v;
	function_ast* func_v;
	new_type_ast* new_type_v;
	alias_ast* alias_v;
	constant_ast* const_v;
	function_ast_map functions;
	new_type_ast_map types;
	alias_ast_map aliases;
	constant_ast_map constants;
	uint32_t import_c;	
	uint32_t func_c;
	uint32_t new_type_c;
	uint32_t alias_c;
	uint32_t const_c;
	uint32_t lifted_lambdas;
	char* string_buffer;
} ast;

void show_ast(const ast* const tree);

ast parse(token* const tokens, pool* const mem, uint64_t token_count, char* string_content_buffer, char* err);
void add_to_tree(ast* const tree, lexer* const lex, pool* const mem, char* err);
void parse_import(ast* const tree, lexer* const lex, pool* const mem, char* err);
uint8_t already_imported(ast* const tree, token filename);
type_ast parse_type(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t consume);
new_type_ast parse_new_type(lexer* const lex, pool* const mem, char* err);
alias_ast parse_alias(lexer* const lex, pool* const mem, char* err);
constant_ast parse_constant(lexer* const lex, pool* const mem, char* err);
function_ast parse_function(lexer* const lex, pool* const mem, char* err, uint8_t allowed_enclosing);
expression_ast parse_lambda(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t* simple);
expression_ast parse_application_expression(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t allow_block, int8_t limit);
expression_ast parse_block_expression(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, expression_ast first);
expression_ast unwrap_single_application(expression_ast single);
function_ast try_function(lexer* const lex, pool* const mem, char* err);
structure_ast parse_struct(lexer* const lex, pool* const mem, char* err);
literal_ast parse_array_literal(lexer* const lex, pool* const mem, char* err);
binding_ast parse_char_literal(lexer* const lex, pool* const mem, char* err);
literal_ast parse_string_literal(lexer* const lex, pool* const mem, char* err);
literal_ast parse_struct_literal(lexer* const lex, pool* const mem, char* err);

typedef struct capture_stack {
	struct capture_stack* prev;
	struct capture_stack* next;
	binding_ast binding_list[MAX_CAPTURES];
	uint16_t size;
	uint16_t binding_count_point;
} capture_stack;

typedef struct replacement_binding {
	token initial;
	token replace;
} replacement_binding;

typedef struct scope {
	binding_ast* binding_stack;
	uint16_t* frame_stack;
	uint16_t binding_count;
	uint16_t binding_capacity;
	uint16_t frame_count;
	uint16_t frame_capacity;
	capture_stack* captures;
	uint16_t capture_frame;
	uint16_t builtin_stack_frame;
	binding_ast* label_stack;
	uint16_t label_count;
	uint16_t label_capacity;
	uint16_t* label_frame_stack;
	uint16_t* label_scope_stack;
	uint16_t label_frame_count;
	uint16_t label_frame_capacity;
	uint16_t label_scope_count;
} scope;

void push_capture_frame(scope* const roll, pool* const mem);
uint16_t pop_capture_frame(scope* const roll, binding_ast** list_result);
void push_capture_binding(scope* const roll, binding_ast binding);

void push_builtins(scope* const roll, pool* const mem);
void push_frame(scope* const s);
void pop_frame(scope* const s);
void push_binding(scope* const s, binding_ast binding);
void pop_binding(scope* const s);

void push_label_frame(scope* const s);
void pop_label_frame(scope* const s);
void push_label(scope* const s, binding_ast binding);
void push_label_scope(scope* const s);
void pop_label_scope(scope* const s);

void transform_ast(ast* const tree, pool* const mem, char* err);
void roll_data_layout(ast* const tree, structure_ast* const target, token name, structure_ast_map* const touched, char* err);
void roll_type(scope* const roll, ast* const tree, pool* const mem, type_ast* const target, char* err);
void roll_struct_type(scope* const roll, ast* const tree, pool* const mem, structure_ast* const target, char* err);
type_ast roll_expression(scope* const roll, ast* const tree, pool* const mem, expression_ast* const expr, type_ast expected_type, uint32_t argc, expression_ast* const argv, uint8_t prevent_lift, char* err);
type_ast* scope_contains(scope* const roll, binding_ast* const binding, uint8_t* needs_capture);
void handle_procedural_statement(scope* const roll, ast* const tree, pool* const mem, expression_ast* const line, type_ast expected_Type, char* const err);
type_ast roll_statement_expression(scope* const roll, ast* const tree, pool* const mem, statement_ast* const statement, type_ast expected_type, uint8_t as_expression, char* err);
type_ast roll_literal_expression(scope* const roll, ast* const tree, pool* const mem, literal_ast* const lit, type_ast expected_type, char* err);
type_ast apply_type(type_ast* const func, char* err);
type_ast resolve_type_or_alias(ast* const tree, type_ast root, char* err);
type_ast resolve_alias(ast* const tree, type_ast root, char* err);
void reduce_aliases(ast* const tree, type_ast* left, type_ast* right);
type_ast prepend_captures(type_ast start, binding_ast* captures, uint16_t total_captures, pool* const mem);
uint64_t type_size_helper(ast* const tree, type_ast target_type, uint64_t rolling_size, char* err);
uint64_t struct_size_helper(ast* const tree, structure_ast target_struct, uint64_t rolling_size, char* err);
uint64_t type_size(ast* const tree, type_ast target_type, char* err);
void lift_lambda(ast* const tree, expression_ast* expr, type_ast captured_type, binding_ast* captured_bindings, uint16_t total_captures, pool* const mem);

#endif
