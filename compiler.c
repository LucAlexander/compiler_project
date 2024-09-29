#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>

#include "compiler.h"
#include "pool.h"

MAP_IMPL(function_ast)
MAP_IMPL(new_type_ast)
MAP_IMPL(alias_ast)
MAP_IMPL(constant_ast)

token create_token(lexer* const lex){
	uint32_t (*subtypes[TOKEN_TYPE_COUNT])(uint32_t, char* const) = {
		identifier_subtype,
		syntactic_subtype,
		symbol_subtype,
		no_subtype,
		no_subtype
	};
	token tok = {
		.line=lex->line,
		.col=lex->col,
		.len=lex->index,
		.type=subtypes[lex->working](lex->working, lex->tok)
	};
	strncpy(tok.string, lex->tok, lex->index);
	return tok;
}

token pass_comment(lexer* const lex, token tok){
	char c;
	switch (tok.type){
	case TOKEN_COMMENT:
		do{
			c = fgetc(lex->fd);
		} while (c != '\n' && c != EOF);
		lex->index = 0;
		return parse_token(lex);
	case TOKEN_MULTI_OPEN:
		do{
			tok = parse_token(lex);
		} while(tok.type != TOKEN_EOF && tok.type != TOKEN_MULTI_CLOSE);
		return parse_token(lex);
	default:
		break;
	}
	return tok;
}

token parse_token(lexer* const lex){
	if (lex->fd == NULL){
		return (token){.type=TOKEN_EOF};
	}
	uint8_t err = 0;
	char c;
	uint8_t (*token_lexers[TOKEN_TYPE_COUNT])(const char* const) = {
		lex_identifier,
		lex_syntactic,
		lex_symbol,
		lex_integer,
		lex_float
	};
	while ((c = fgetc(lex->fd)) != EOF){
		lex->col += 1;
		switch(c){
		case '\n':
			lex->line += 1;
			lex->col = 0;
		case ' ':
		case '\r':
		case '\t':
			if (lex->index > 0){
				token complete = create_token(lex);
				lex->index = 0;
				return pass_comment(lex, complete);
			}
			continue;
		}
		lex->tok[lex->index] = c;
		lex->tok[lex->index+1] = '\0';
		if (lex->index == 0){
			err = 1;
			for (int i = 0;i<TOKEN_TYPE_COUNT;++i){
				if (token_lexers[i](lex->tok) == 0){
					lex->working = i;
					err = 0;
					break;
				}
			}
			if (err == 0){
				lex->index += 1;
			}
			else{
				fprintf(stderr, "No token can start with %c\n", c);
			}
			continue;
		}
		lex->index += 1;
		uint32_t last_working = lex->working;
		while (token_lexers[lex->working](lex->tok) != 0){
			if (lex->working < TOKEN_TYPE_COUNT-1){
				lex->working += 1;
				continue;
			}
			lex->tok[lex->index-1] = '\0';
			lex->working = last_working;
			token complete = create_token(lex);
			err = 1;
			lex->tok[0] = c;
			lex->tok[1] = '\0';
			lex->index = 1;
			for (int i = 0;i<TOKEN_TYPE_COUNT;++i){
				if (token_lexers[i](lex->tok) == 0){
					lex->working = i;
					err = 0;
					break;
				}
			}
			if (err != 0){
				fprintf(stderr, "No token can start with %c\n", c);
				lex->index = 0;
			}
			return pass_comment(lex, complete);
		}
		if (lex->index == TOKEN_MAX-1){
			token complete = create_token(lex);
			lex->index = 0;
			return pass_comment(lex, complete);
		}
	}
	return (token){.type=TOKEN_EOF};
}

uint8_t parse_crawl(lexer* const lex, pool* const mem, char* content, uint32_t* length){
	char c;
	for (size_t i = 0;i<lex->index;++i){
		content[i] = lex->tok[i];
		*length += 1;
		pool_byte(mem);
	}
	content[*length] = '\0';
	lex->tok[0] = '\0';
	lex->index = 0;
	while ((c=fgetc(lex->fd)) != EOF){
		lex->col += 1;
		switch (c){
		case '\"':
			return 0;
		case '\n':
			lex->line += 1;
			lex->col = 0;
		default:
			content[*length] = c;
			*length += 1;
			pool_byte(mem);
			content[*length] = '\0';
			break;
		}
	}
	return 1;
}

uint8_t issymbol(char c){
	return (c > 32 && c < 48)
		|| (c > 122 && c < 127)
		|| (c > 90 && c < 97)
		|| (c > 57 && c < 65);
}

uint8_t lex_identifier(const char* const string){
	const char* c = string;
	if (isdigit(*c)){
		return 1;
	}
	for (; *c != '\0';++c){
		if (!isupper(*c) && !islower(*c) && !isdigit(*c) && (*c != '_')){
			return 1;
		}
	}
	return 0;
}

uint32_t identifier_subtype(uint32_t type_index, char* const content){
	if (strncmp(content, "if", TOKEN_MAX) == 0){ return TOKEN_IF; }
	if (strncmp(content, "else", TOKEN_MAX) == 0){ return TOKEN_ELSE; }
	if (strncmp(content, "using", TOKEN_MAX) == 0){ return TOKEN_IMPORT; }
	if (strncmp(content, "match", TOKEN_MAX) == 0){ return TOKEN_MATCH; }
	if (strncmp(content, "return", TOKEN_MAX) == 0){ return TOKEN_RETURN; }
	if (strncmp(content, "u8", TOKEN_MAX) == 0){ return TOKEN_U8; }
	if (strncmp(content, "u16", TOKEN_MAX) == 0){ return TOKEN_U16; }
	if (strncmp(content, "u32", TOKEN_MAX) == 0){ return TOKEN_U32; }
	if (strncmp(content, "u64", TOKEN_MAX) == 0){ return TOKEN_U64; }
	if (strncmp(content, "i8", TOKEN_MAX) == 0){ return TOKEN_I8; }
	if (strncmp(content, "i16", TOKEN_MAX) == 0){ return TOKEN_I16; }
	if (strncmp(content, "i32", TOKEN_MAX) == 0){ return TOKEN_I32; }
	if (strncmp(content, "i64", TOKEN_MAX) == 0){ return TOKEN_I64; }
	if (strncmp(content, "f32", TOKEN_MAX) == 0){ return TOKEN_F32; }
	if (strncmp(content, "f64", TOKEN_MAX) == 0){ return TOKEN_F64; }
	if (strncmp(content, "type", TOKEN_MAX) == 0){ return TOKEN_TYPE; }
	if (strncmp(content, "alias", TOKEN_MAX) == 0){ return TOKEN_ALIAS; }
	if (strncmp(content, "var", TOKEN_MAX) == 0){ return TOKEN_MUTABLE; }
	if (strncmp(content, "ptr", TOKEN_MAX) == 0){ return TOKEN_REF; }
	if (strncmp(content, "procedure", TOKEN_MAX) == 0){ return TOKEN_PROC; }
	if (strncmp(content, "constant", TOKEN_MAX) == 0){ return TOKEN_CONST; }
	if (strncmp(content, "as", TOKEN_MAX) == 0){ return TOKEN_CAST; }
	if (strncmp(content, "sizeof", TOKEN_MAX) == 0){ return TOKEN_SIZEOF; }
	return type_index;
}

uint8_t lex_syntactic(const char* const string){
	if (strlen(string) == 1){
		switch(string[0]){
		case '{':
		case '}':
		case '[':
		case ']':
		case '(':
		case ')':
		case '"':
		case '\'':
		case '\\':
		case ';':
		case '.':
		case '$':
		case '#':
		case ',':
			return 0;
		}
	}
	return 1;
}

uint32_t syntactic_subtype(uint32_t type_index, char* const content){
	if (strncmp(content, "{", TOKEN_MAX) == 0){ return TOKEN_BRACE_OPEN; }
	if (strncmp(content, "}", TOKEN_MAX) == 0){ return TOKEN_BRACE_CLOSE; }
	if (strncmp(content, "(", TOKEN_MAX) == 0){ return TOKEN_PAREN_OPEN; }
	if (strncmp(content, ")", TOKEN_MAX) == 0){ return TOKEN_PAREN_CLOSE; }
	if (strncmp(content, "[", TOKEN_MAX) == 0){ return TOKEN_BRACK_OPEN; }
	if (strncmp(content, "]", TOKEN_MAX) == 0){ return TOKEN_BRACK_CLOSE; }
	if (strncmp(content, "\"", TOKEN_MAX) == 0){ return TOKEN_QUOTE; }
	if (strncmp(content, "\\", TOKEN_MAX) == 0){ return TOKEN_ARG; }
	if (strncmp(content, "'", TOKEN_MAX) == 0){ return TOKEN_TICK; }
	if (strncmp(content, ";", TOKEN_MAX) == 0){ return TOKEN_SEMI; }
	if (strncmp(content, ".", TOKEN_MAX) == 0){ return TOKEN_COMPOSE; }
	if (strncmp(content, "$", TOKEN_MAX) == 0){ return TOKEN_PASS; }
	if (strncmp(content, "#", TOKEN_MAX) == 0){ return TOKEN_ENCLOSE; }
	if (strncmp(content, ",", TOKEN_MAX) == 0){ return TOKEN_COMMA; }
	return type_index;
}

uint8_t lex_symbol(const char* const string){
	for (const char* c = string; *c != '\0';++c){
		if ((!issymbol(*c))
		|| (*c == 40)
		|| (*c == 41)
		|| (*c == 91)
		|| (*c == 93)
		|| (*c == 123)
		|| (*c == 125)
		|| (*c == ';')){
			return 1;
		}
	}
	return 0;
}

uint32_t symbol_subtype(uint32_t type_index, char* const content){
	if (strncmp(content, "+", TOKEN_MAX) == 0){ return TOKEN_ADD; }
	if (strncmp(content, "-", TOKEN_MAX) == 0){ return TOKEN_SUB; }
	if (strncmp(content, "*", TOKEN_MAX) == 0){ return TOKEN_MUL; }
	if (strncmp(content, "/", TOKEN_MAX) == 0){ return TOKEN_DIV; }
	if (strncmp(content, ".+", TOKEN_MAX) == 0){ return TOKEN_FLADD; }
	if (strncmp(content, ".-", TOKEN_MAX) == 0){ return TOKEN_FLSUB; }
	if (strncmp(content, ".*", TOKEN_MAX) == 0){ return TOKEN_FLMUL; }
	if (strncmp(content, "./", TOKEN_MAX) == 0){ return TOKEN_FLDIV; }
	if (strncmp(content, "%", TOKEN_MAX) == 0){ return TOKEN_MOD; }
	if (strncmp(content, "<<", TOKEN_MAX) == 0){ return TOKEN_SHL; }
	if (strncmp(content, ">>", TOKEN_MAX) == 0){ return TOKEN_SHR; }
	if (strncmp(content, ".<", TOKEN_MAX) == 0){ return TOKEN_FLANGLE_OPEN; }
	if (strncmp(content, ".>", TOKEN_MAX) == 0){ return TOKEN_FLANGLE_CLOSE; }
	if (strncmp(content, ".<=", TOKEN_MAX) == 0){ return TOKEN_FLLESS_EQ; }
	if (strncmp(content, ".>=", TOKEN_MAX) == 0){ return TOKEN_FLGREATER_EQ; }
	if (strncmp(content, "=", TOKEN_MAX) == 0){ return TOKEN_SET; }
	if (strncmp(content, ".==", TOKEN_MAX) == 0){ return TOKEN_FLEQ; }
	if (strncmp(content, ".!=", TOKEN_MAX) == 0){ return TOKEN_FLNOT_EQ; }
	if (strncmp(content, "&&", TOKEN_MAX) == 0){ return TOKEN_BOOL_AND; }
	if (strncmp(content, "||", TOKEN_MAX) == 0){ return TOKEN_BOOL_OR; }
	if (strncmp(content, "&", TOKEN_MAX) == 0){ return TOKEN_BIT_AND; }
	if (strncmp(content, "|", TOKEN_MAX) == 0){ return TOKEN_BIT_OR; }
	if (strncmp(content, "^", TOKEN_MAX) == 0){ return TOKEN_BIT_XOR; }
	if (strncmp(content, "~", TOKEN_MAX) == 0){ return TOKEN_BIT_COMP; }
	if (strncmp(content, "!", TOKEN_MAX) == 0){ return TOKEN_BOOL_NOT; }
	if (strncmp(content, "->", TOKEN_MAX) == 0){ return TOKEN_FUNC_IMPL; }
	if (strncmp(content, "<|", TOKEN_MAX) == 0){ return TOKEN_PIPE_LEFT; }
	if (strncmp(content, "|>", TOKEN_MAX) == 0){ return TOKEN_PIPE_RIGHT; }
	if (strncmp(content, "//", TOKEN_MAX) == 0){ return TOKEN_COMMENT; }
	if (strncmp(content, "/*", TOKEN_MAX) == 0){ return TOKEN_MULTI_OPEN; }
	if (strncmp(content, "*/", TOKEN_MAX) == 0){ return TOKEN_MULTI_CLOSE; }
	return type_index;
}

uint8_t lex_integer(const char* const string){
	for (const char* c = string; *c != '\0';++c){
		if (!isdigit(*c)){
			return 1;
		}
	}
	return 0;
}
//TODO neither of these support negatives lol, also This is not the full IEEE 754
uint8_t lex_float(const char* const string){
	uint8_t dec = 0;
	const char* c = string;
	if (!isdigit(*c)){
		return 1;
	}
	++c;
	for (;*c != '\0';++c){
		if (!isdigit(*c)){
			if ((dec == 0) && (*c) == '.'){
				dec = 1;
				continue;
			}
			return 1;
		}
	}
	return 0;
}

uint32_t no_subtype(uint32_t type_index, char* const content){
	return type_index;
}

ast parse(FILE* fd, pool* const mem, char* err){
	lexer lex = {
		.fd=fd,
		.line=1,
		.col=0,
		.index=0,
		.working=0,
		.tok=""
	};
	token tok;
	ast tree = {
		.import_c = 0,
		.func_c = 0,
		.new_type_c = 0,
		.alias_c = 0,
		.const_c = 0,
		.functions = function_ast_map_init(mem),
		.types = new_type_ast_map_init(mem),
		.aliases = alias_ast_map_init(mem),
		.constants = constant_ast_map_init(mem),
		.lifted_lambdas=0
	};
	// parse imports
	tree.import_v = pool_request(mem, sizeof(token)*MAX_IMPORTS);
	while (1){
		tok = parse_token(&lex);
		if (tok.type != TOKEN_IMPORT){
			break;
		}
		if (tree.import_c >= MAX_IMPORTS){
			*err = 1;
			return tree;
			fprintf(stderr, "too many imports\n");
		}
		if (parse_import(&tree, &lex, mem, err) != 0){
			*err = 1;
			return tree;
		}
	}
	if (tok.type == TOKEN_EOF){
		return tree;
	}
	// parse actual code
	tree.func_v = pool_request(mem, sizeof(function_ast)*MAX_FUNCTIONS) - (sizeof(token)*(MAX_IMPORTS - tree.import_c));
	tree.new_type_v = pool_request(mem, sizeof(new_type_ast)*MAX_ALIASES);
	tree.alias_v = pool_request(mem, sizeof(alias_ast)*MAX_ALIASES);
	tree.const_v = pool_request(mem, sizeof(constant_ast)*MAX_ALIASES);
	while (1){
		if (tok.type == TOKEN_EOF){
			return tree;
		}
		if (tok.type == TOKEN_TYPE){
			new_type_ast a = parse_new_type(&lex, mem, err);
			if (*err != 0){
				return tree;
			}
			tree.new_type_v[tree.new_type_c] = a;
			uint8_t collision = new_type_ast_map_insert(&tree.types, tree.new_type_v[tree.new_type_c].name.string, &tree.new_type_v[tree.new_type_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined multiple times\n", tree.new_type_v[tree.new_type_c].name.string);
			}
			else if (function_ast_map_access(&tree.functions, tree.new_type_v[tree.new_type_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined prior as function\n", tree.new_type_v[tree.new_type_c].name.string);
			}
			else if (alias_ast_map_access(&tree.aliases, tree.new_type_v[tree.new_type_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined prior as alias\n", tree.new_type_v[tree.new_type_c].name.string);
			}
			else if (constant_ast_map_access(&tree.constants, tree.new_type_v[tree.new_type_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined prior as constant\n", tree.new_type_v[tree.new_type_c].name.string);
			}
			tree.new_type_c += 1;
		}
		else if (tok.type == TOKEN_ALIAS){
			alias_ast a = parse_alias(&lex, mem, err);
			if (*err != 0){
				return tree;
			}
			tree.alias_v[tree.alias_c] = a;
			uint8_t collision = alias_ast_map_insert(&tree.aliases, tree.alias_v[tree.alias_c].name.string, &tree.alias_v[tree.alias_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined multiple times\n", tree.alias_v[tree.alias_c].name.string);
			}
			else if (function_ast_map_access(&tree.functions, tree.alias_v[tree.alias_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined prior as function\n", tree.alias_v[tree.alias_c].name.string);
			}
			else if (new_type_ast_map_access(&tree.types, tree.alias_v[tree.alias_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined prior as type\n", tree.alias_v[tree.alias_c].name.string);
			}
			else if (constant_ast_map_access(&tree.constants, tree.alias_v[tree.alias_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined prior as constant\n", tree.alias_v[tree.alias_c].name.string);
			}
			tree.alias_c += 1;
		}
		else if (tok.type == TOKEN_CONST){
			constant_ast cnst = parse_constant(&lex, mem, err);
			if (*err != 0){
				return tree;
			}
			tree.const_v[tree.const_c] = cnst;
			uint8_t collision = constant_ast_map_insert(&tree.constants, tree.const_v[tree.const_c].name.string, &tree.const_v[tree.const_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' was defined multiple times\n", tree.const_v[tree.const_c].name.string);
			}
			else if (function_ast_map_access(&tree.functions, tree.const_v[tree.const_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' defined prior as function\n", tree.const_v[tree.const_c].name.string);
			}
			else if (new_type_ast_map_access(&tree.types, tree.const_v[tree.const_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' defined prior as type\n", tree.const_v[tree.const_c].name.string);
			}
			else if (alias_ast_map_access(&tree.aliases, tree.const_v[tree.const_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' defined prior as alias\n", tree.const_v[tree.const_c].name.string);
			}
			tree.const_c += 1;
		}
		else{
			function_ast f = parse_function(&lex, mem, tok, err, 0);
			if (*err != 0){
				return tree;
			}
			tree.func_v[tree.func_c] = f;
			uint8_t collision = function_ast_map_insert(&tree.functions, tree.func_v[tree.func_c].name.string, &tree.func_v[tree.func_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined multiple times\n", tree.func_v[tree.func_c].name.string);
			}
			else if (new_type_ast_map_access(&tree.types, tree.func_v[tree.func_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined prior as type\n", tree.func_v[tree.func_c].name.string);
			}
			else if (alias_ast_map_access(&tree.aliases, tree.func_v[tree.func_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined prior as alias\n", tree.func_v[tree.func_c].name.string);
			}
			else if (constant_ast_map_access(&tree.constants, tree.func_v[tree.func_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined prior as constant\n", tree.func_v[tree.func_c].name.string);
			}
			tree.func_c += 1;
		}
		tok = parse_token(&lex);
	}
	*err = 1;
	fprintf(stderr, "how did you get here?\n");
	return tree;
}

uint8_t parse_import(ast* const tree, lexer* const lex, pool* const mem, char* err){
	token filename = parse_token(lex);
	if (filename.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Tried to import non identifier: '%s'\n", lex->line, lex->col, filename.string);
		return 1;
	}
	token semi = parse_token(lex);
	if (semi.type != TOKEN_SEMI){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected ; after import %s, found '%s'\n", lex->line, lex->col, filename.string, semi.string);
		return 1;
	}
	tree->import_v[tree->import_c] = filename;
	tree->import_c += 1;
	return 0;
}

structure_ast parse_struct(lexer* const lex, pool* const mem, char* err){
	structure_ast outer = {
		.binding_v=pool_request(mem, sizeof(binding_ast)*MAX_MEMBERS),
		.union_v=NULL,
		.encoding=NULL,
		.tag_v=NULL,
		.binding_c=0,
		.union_c=0
	};
	while (1){
		token tok = parse_token(lex);
		if (tok.type == TOKEN_BRACE_CLOSE){
			return outer;
		}
		uint64_t save = ftell(lex->fd);
		pool_save(mem);
		lexer copy = *lex;
		type_ast type = parse_type(lex, mem, err, tok, TOKEN_IDENTIFIER, 0);
		if (*err != 0){
			if (fseek(lex->fd, save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack after parsing type for structure member\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*err = 0;
			*lex = copy;
			if (tok.type != TOKEN_IDENTIFIER){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected identifier for enumerated union member or type for struct member, found '%s'\n", lex->line, lex->col, tok.string);
				return outer;
			}
			if (outer.union_c == 0){
				outer.union_v = pool_request(mem, sizeof(structure_ast)*MAX_MEMBERS);
				outer.encoding = pool_request(mem, sizeof(int64_t)*MAX_MEMBERS);
				outer.tag_v = pool_request(mem, sizeof(token)*MAX_MEMBERS);
				outer.encoding[0] = 0;
			}
			else{
				outer.encoding[outer.union_c] = outer.encoding[outer.union_c-1]+1;
			}
			outer.tag_v[outer.union_c] = tok;
			outer.union_c += 1;
			token open = parse_token(lex);
			switch(open.type){
			case TOKEN_SEMI:
				break;
			case TOKEN_BRACE_OPEN:
				structure_ast s = parse_struct(lex, mem, err);
				if (*err != 0){
					return outer;
				}
				outer.union_v[outer.union_c-1] = s;
				token semi = parse_token(lex);
				if (semi.type == TOKEN_SEMI){
					break;
				}
				if (semi.type != TOKEN_SET){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected ';' to end union or '=' to set enumerated encoding, found '%s'\n", lex->line, lex->col, semi.string);
					return outer;
				}
			case TOKEN_SET:
				token encoding = parse_token(lex);
				if (encoding.type != TOKEN_INTEGER){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected enumerator encoding, found '%s'\n", lex->line, lex->col, encoding.string);
					return outer;
				}
				token true_semi = parse_token(lex);
				if (true_semi.type == TOKEN_SEMI){
					outer.encoding[outer.union_c-1] = atoi(encoding.string);
					break;
				}
			default:
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected '{', for union data or '=' for enum encoding, found '%s'\n", lex->line, lex->col, open.string);
				return outer;
			}
		}
		else{
			tok = parse_token(lex);
			token semi = parse_token(lex);
			if (tok.type != TOKEN_IDENTIFIER || semi.type != TOKEN_SEMI){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected 'identifier ;' for struct member name, found '%s' '%s'\n", lex->line, lex->col, tok.string, semi.string);
				return outer;
			}
			binding_ast binding = {
				.type=type,
				.name=tok
			};
			outer.binding_v[outer.binding_c] = binding;
			outer.binding_c += 1;
		}
	}
	snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u How did you get here in struct parse\n", lex->line, lex->col);
	return outer;
}

type_ast parse_type(lexer* const lex, pool* const mem, char* err, token name, TOKEN_TYPE_TAG end_token, uint8_t consume){
	type_ast outer = (type_ast){
		.tag=USER_TYPE,
		.data.user=name,
		.mut=0
	};
	switch (name.type){
	case TOKEN_BRACK_OPEN:
		name = parse_token(lex);
		outer = parse_type(lex, mem, err, name, TOKEN_BRACK_CLOSE, 1);
		if (*err != 0){
			return outer;
		}
		if (outer.tag != BUFFER_TYPE){
			type_ast base = outer;
			outer.tag = POINTER_TYPE;
			outer.data.pointer = pool_request(mem, sizeof(type_ast));
			*outer.data.pointer = base;
			outer.mut = 0;
		}
		break;
	case TOKEN_PAREN_OPEN:
		name = parse_token(lex);
		outer = parse_type(lex, mem, err, name, TOKEN_PAREN_CLOSE, 1);
		if (*err != 0){
			return outer;
		}
		break;
	case TOKEN_BRACE_OPEN:
		structure_ast s = parse_struct(lex, mem, err);
		if (*err != 0){
			return outer;
		}
		outer.tag = STRUCT_TYPE;
		outer.data.structure = pool_request(mem, sizeof(structure_ast));
		*outer.data.structure = s;
		break;
	case TOKEN_U8:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = U8_TYPE;
		break;
	case TOKEN_U16:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = U16_TYPE;
		break;
	case TOKEN_U32:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = U32_TYPE;
		break;
	case TOKEN_U64:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = U64_TYPE;
		break;
	case TOKEN_I8:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = I8_TYPE;
		break;
	case TOKEN_I16:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = I16_TYPE;
		break;
	case TOKEN_I32:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = I32_TYPE;
		break;
	case TOKEN_I64:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive = I64_TYPE;
		break;
	case TOKEN_F32:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive=F32_TYPE;
		break;
	case TOKEN_F64:
		outer.tag=PRIMITIVE_TYPE;
		outer.data.primitive=F64_TYPE;
		break;
	case TOKEN_PROC:
		outer.tag=PROCEDURE_TYPE;
		type_ast procedure_optional = parse_type(lex, mem, err, parse_token(lex), end_token, 0);
		if (*err != 0){
			return outer;
		}
		outer.data.pointer = pool_request(mem, sizeof(type_ast));
		*outer.data.pointer = procedure_optional;
		break;
	default:
		if (name.type!=TOKEN_IDENTIFIER){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected type name, found non identifier '%s'\n", lex->line, lex->col, name.string);
			return (type_ast){.tag=NONE_TYPE};
		}
		break;
	}
	while (1){
		uint64_t save = ftell(lex->fd);
		pool_save(mem);
		lexer copy = *lex;
		token tok = parse_token(lex);
		if (tok.type == end_token || (end_token == TOKEN_IDENTIFIER && tok.type == TOKEN_SYMBOL)){
			if (consume == 0){
				if (fseek(lex->fd, save, SEEK_SET) != 0){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack after parsing type\n", lex->line, lex->col);
					return outer;
				}
				pool_load(mem);
				*lex = copy;
			}
			return outer;
		}
		switch(tok.type){
		case TOKEN_MUTABLE:
			if (outer.mut == 1){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error at %u:%u Type has two mutable tags\n", lex->line, lex->col);
				return outer;
			}
			outer.mut = 1;
			break;
		case TOKEN_IDENTIFIER:
			if (end_token != TOKEN_BRACK_CLOSE){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error at %u:%u Type given buffer size when not buffer or pointer\n", lex->line, lex->col);
				return outer;
			}
			type_ast idenbase = outer;
			outer.tag = BUFFER_TYPE;
			outer.data.buffer.base = pool_request(mem, sizeof(type_ast));
			*outer.data.buffer.base = idenbase;
			outer.data.buffer.count = 0;
			outer.data.buffer.constant = 1;
			outer.data.buffer.const_binding = tok;
			outer.mut = 0;
			break;
		case TOKEN_INTEGER:
			if (end_token != TOKEN_BRACK_CLOSE){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error at %u:%u Type given buffer size when not buffer or pointer\n", lex->line, lex->col);
				return outer;
			}
			type_ast base = outer;
			outer.tag = BUFFER_TYPE;
			outer.data.buffer.base = pool_request(mem, sizeof(type_ast));
			*outer.data.buffer.base = base;
			outer.data.buffer.count = atoi(tok.string);
			outer.data.buffer.constant = 0;
			outer.mut = 0;
			break;
		case TOKEN_FUNC_IMPL:
			type_ast arg = outer;
			outer.tag = FUNCTION_TYPE;
			outer.data.function.left = pool_request(mem, sizeof(type_ast));
			outer.data.function.right = pool_request(mem, sizeof(type_ast));
			*outer.data.function.left = arg;
			outer.mut = 0;
			tok = parse_token(lex);
			*outer.data.function.right = parse_type(lex, mem, err, tok, end_token, consume);
			return outer;
		default:
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unexpected token in or after type: '%s'\n", lex->line, lex->col, tok.string);
			return outer;
		}
	}
	snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Reached end of file while parsing type\n", lex->line, lex->col);
	return (type_ast){.tag=NONE_TYPE};
}

new_type_ast parse_new_type(lexer* const lex, pool* const mem, char* err){
	token name = parse_token(lex);
	if (name.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected identifier for type new_type name, found '%s'\n", lex->line, lex->col, name.string);
		return (new_type_ast){};
	}
	type_ast type = parse_type(lex, mem, err, parse_token(lex), TOKEN_SEMI, 1);
	if (*err != 0){
		return (new_type_ast){};
	}
	return (new_type_ast){
		.type=type,
		.name=name
	};
}

constant_ast parse_constant(lexer* const lex, pool* const mem, char* err){
	token name = parse_token(lex);
	constant_ast constant = {
		.value.type.tag=PRIMITIVE_TYPE,
		.value.type.data.primitive=INT_ANY
	};
	if (name.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected name for constant identifier\n", lex->line, lex->col);
		return constant;
	}
	constant.name=name;
	token eq = parse_token(lex);
	if (eq.type != TOKEN_SET){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected token '=' to set constant value\n", lex->line, lex->col);
	}
	token val = parse_token(lex);
	switch (val.type){
	case TOKEN_FLOAT:
		constant.value.type=(type_ast){
			.tag=PRIMITIVE_TYPE,
			.data.primitive=FLOAT_ANY
		};
		constant.value.name=val;
		break;
	case TOKEN_INTEGER:
		constant.value.type=(type_ast){
			.tag=PRIMITIVE_TYPE,
			.data.primitive=INT_ANY
		};
		constant.value.name=val;
		break;
	default:
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Constants can currently only evaluate to integers and floats, in the future this may just be turned into a compile time expression evaluation feature\n", lex->line, lex->col);
		return constant;
	}
	token semi = parse_token(lex);
	if (semi.type != TOKEN_SEMI){
		snprintf(err, ERROR_BUFFER, " <!> Parsing ERror at %u:%u Constants definitions must end with token ';'\n", lex->line, lex->col);
		return constant;
	}
	return constant;
}

alias_ast parse_alias(lexer* const lex, pool* const mem, char* err){
	token name = parse_token(lex);
	if (name.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected identifier for type new_type name, found '%s'\n", lex->line, lex->col, name.string);
		return (new_type_ast){};
	}
	type_ast type = parse_type(lex, mem, err, parse_token(lex), TOKEN_SEMI, 1);
	if (*err != 0){
		return (new_type_ast){};
	}
	return (new_type_ast){
		.type=type,
		.name=name
	};
}

function_ast parse_function(lexer* const lex, pool* const mem, token tok, char* err, uint8_t allowed_enclosing){
	type_ast type = parse_type(lex, mem, err, tok, TOKEN_IDENTIFIER, 0);
	if (*err != 0){
		return (function_ast){
			.type=type
		};
	}
	token name = parse_token(lex);
	if (name.type != TOKEN_IDENTIFIER && name.type != TOKEN_SYMBOL){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected function name, found '%s'\n", lex->line, lex->col, name.string);
		return (function_ast){};
	}
	tok = parse_token(lex);
	uint8_t enclosing = 0;
	if (tok.type != TOKEN_SET){
		if (allowed_enclosing != 1 || tok.type != TOKEN_ENCLOSE){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected '=' to set function to value, found %s\n", lex->line, lex->col, tok.string);
			return (function_ast){};
		}
		enclosing = 1;
	}
	expression_ast expr = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_SEMI, 0, -1);
	if (expr.tag == APPLICATION_EXPRESSION && expr.data.block.expr_c == 1){
		expr = expr.data.block.expr_v[0];
	}
	return (function_ast){
		.type=type,
		.name=name,
		.enclosing=enclosing,
		.expression=expr
	};
}

expression_ast parse_lambda(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t* simple){
	expression_ast outer = {
		.tag=LAMBDA_EXPRESSION,
		.data.lambda={
			.argv=pool_request(mem, sizeof(token)*MAX_ARGS),
			.argc=0,
			.expression=NULL,
			.type.tag=NONE_TYPE
		}
	};
	token tok;
	while (1){
		tok = parse_token(lex);
		expression_ast build;
		uint64_t save;
		lexer copy;
		switch (tok.type){
		case TOKEN_IDENTIFIER:
			outer.data.lambda.argv[outer.data.lambda.argc] = tok;
			outer.data.lambda.argc += 1;
			if (outer.data.lambda.argc >= MAX_ARGS){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Too many args for arena allocated buffer\n", lex->line, lex->col);
				return outer;
			}
			break;
		case TOKEN_BRACK_OPEN:
			copy = *lex;
			save = ftell(lex->fd);
			pool_save(mem);
			literal_ast lit = parse_array_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
				*outer.data.lambda.expression = build;
				return outer;
			}
			if (fseek(lex->fd, save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing dereference expression\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*lex = copy;
			*err = 0;
			build = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_BRACK_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			expression_ast deref = {
				.tag=DEREF_EXPRESSION,
				.data.deref = pool_request(mem, sizeof(expression_ast))
			};
			*deref.data.deref = build;
			outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
			*outer.data.lambda.expression = deref;
			return outer;
		case TOKEN_BRACE_OPEN:
			copy = *lex;
			save = ftell(lex->fd);
			pool_save(mem);
			lit = parse_struct_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
				*outer.data.lambda.expression = build;
				return outer;
			}
			if (fseek(lex->fd, save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing dereference expression\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*lex = copy;
			*err = 0;
			build = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_BRACE_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			expression_ast access = {
				.tag=ACCESS_EXPRESSION,
				.data.deref = pool_request(mem, sizeof(expression_ast))
			};
			*access.data.deref = build;
			outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
			*outer.data.lambda.expression = access;
			return outer;
		case TOKEN_PAREN_OPEN:
			build = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_PAREN_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
			*outer.data.lambda.expression = build;
			return outer;
		default:
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unexpected token provided as function argument: '%s'\n", lex->line, lex->col, tok.string);
			return outer;
		}
	}
	snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Reached unexpected case in lambda parsing\n", lex->line, lex->col);
	return outer;
}

function_ast try_function(lexer* const lex, pool* const mem, token expr, char* err){
	uint32_t save;
	function_ast func;
	switch(expr.type){
	case TOKEN_IDENTIFIER:
	case TOKEN_BRACE_OPEN:
	case TOKEN_PAREN_OPEN:
	case TOKEN_BRACK_OPEN:
	case TOKEN_U8:
	case TOKEN_U16:
	case TOKEN_U32:
	case TOKEN_U64:
	case TOKEN_I8:
	case TOKEN_I16:
	case TOKEN_I32:
	case TOKEN_I64:
	case TOKEN_F32:
	case TOKEN_F64:
		lexer copy = *lex;
		save = ftell(lex->fd);
		pool_save(mem);
		func = parse_function(lex, mem, expr, err, 1);
		if (*err == 0){
			return func;
		}
		if (fseek(lex->fd, save, SEEK_SET) != 0){
			snprintf(err, ERROR_BUFFER, "<!> Parsing Error at %u:%u Unable to backtrack after closure parse attempt\n", lex->line, lex->col);
			return func;
		}
		pool_load(mem);
		*lex = copy;
		return func;
	default:
		break;
	}
	*err = 1;
	return func;
}

expression_ast unwrap_single_application(expression_ast single){
	while (single.tag == APPLICATION_EXPRESSION && single.data.block.expr_c == 1){
		single = single.data.block.expr_v[0];
	}
	return single;
}

expression_ast parse_block_expression(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, expression_ast first){
	expression_ast outer = {
		.tag=BLOCK_EXPRESSION,
		.data.block.type={.tag=NONE_TYPE},
		.data.block.expr_c=0
	};
	outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	first = unwrap_single_application(first);
	if (first.tag != NOP_EXPRESSION){
		outer.data.block.expr_c = 1;
		outer.data.block.expr_v[0] = first;
	}
	if (first.tag == RETURN_EXPRESSION){
		return outer;
	}
	while(1){
		expression_ast build = {
			.tag=APPLICATION_EXPRESSION
		};
		token tok = parse_token(lex);
		if (tok.type == end_token){
			return outer;
		}
		build = parse_application_expression(lex, mem, tok, err, TOKEN_SEMI, 2, -1);
		if (*err != 0){
			return outer;
		}
		build = unwrap_single_application(build);
		outer.data.block.expr_v[outer.data.block.expr_c] = build;
		outer.data.block.expr_c += 1;
	}
	snprintf(err, ERROR_BUFFER, " <!> Parser Error %u:%u how did you get to this condition in the block expression parser\n", lex->line, lex->col);
	return outer;
}

expression_ast parse_application_expression(lexer* const lex, pool* const mem, token expr, char* err, TOKEN_TYPE_TAG end_token, uint8_t allow_block, int8_t limit){
	expression_ast outer = {
		.tag=APPLICATION_EXPRESSION,
		.data.block.type={.tag=NONE_TYPE},
		.data.block.expr_c=0
	};
	outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	if (allow_block != 0){
		function_ast func = try_function(lex, mem, expr, err);
		if (*err == 0){
			outer.tag = CLOSURE_EXPRESSION;
			outer.data.closure.capture_v = NULL;
			outer.data.closure.capture_c = 0;
			outer.data.closure.func = pool_request(mem, sizeof(function_ast));
			*outer.data.closure.func = func;
			if (allow_block == 2){
				return outer;
			}
			return parse_block_expression(lex, mem, err, end_token, outer);
		}
		*err = 0;
	}
	expression_ast pass_expression;
	expression_ast* last_pass;
	uint8_t pass = 0;
	uint64_t limit_save;
	lexer limit_copy;
	if (limit != -1){
		if (allow_block != 0){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Assertion error %u:%u blocks cannot be allowed when applications are limit requested\n", lex->line, lex->col);
			return outer;
		}
		limit_save = ftell(lex->fd);
		pool_save(mem);
		limit_copy = *lex;
		expr = parse_token(lex);
	}
	while (1){
		expression_ast build = {
			.tag=BINDING_EXPRESSION
		};
		literal_ast lit;
		if (expr.type == end_token){
			if (limit != -1){
				if (fseek(lex->fd, limit_save, SEEK_SET) != 0){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing limited application expression\n", lex->line, lex->col);
					return outer;
				}
				pool_load(mem);
				*lex = limit_copy;
			}
			if (outer.data.block.expr_c == 0 && end_token == TOKEN_SEMI){
				return parse_application_expression(lex, mem, parse_token(lex), err, end_token, allow_block, -1);
			}
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		}
		if (limit == 0 || (limit != -1 && expr.type == TOKEN_SEMI)){
			if (fseek(lex->fd, limit_save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing limited application expression\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*lex = limit_copy;
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		}
		uint8_t simple = 0;
		uint64_t save;
		lexer copy;
		switch (expr.type){
		case TOKEN_RETURN:
			if (outer.data.block.expr_c != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unexpected token 'return'\n", lex->line, lex->col);
				return outer;
			}
			build.tag = RETURN_EXPRESSION;
			build.data.deref = pool_request(mem, sizeof(expression_ast));
			expression_ast retexpr = parse_application_expression(lex, mem, parse_token(lex), err, end_token, allow_block, -1);
			if (*err != 0){
				return outer;
			}
			if (allow_block == 1 && retexpr.tag == BLOCK_EXPRESSION && retexpr.data.block.expr_c == 1){
				retexpr.tag = APPLICATION_EXPRESSION;
				*build.data.deref = retexpr;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				if (pass == 0){
					return parse_block_expression(lex, mem, err, end_token, outer);
				}
				last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
				last_pass->data.block.expr_c += 1;
				return parse_block_expression(lex, mem, err, end_token, pass_expression);
			}
			*build.data.deref = retexpr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		case TOKEN_REF:
			build.tag = REF_EXPRESSION;
			build.data.deref = pool_request(mem, sizeof(expression_ast));
			*build.data.deref = parse_application_expression(lex, mem, parse_token(lex), err, end_token, allow_block, -1);
			if (*err != 0){
				return outer;
			}
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		case TOKEN_SIZEOF:
			build.tag = SIZEOF_EXPRESSION;
			copy = *lex;
			save = ftell(lex->fd);
			pool_save(mem);
			build.data.size_of.type = parse_type(lex, mem, err, parse_token(lex), end_token, 0);
			if (*err == 0){
				build.data.size_of.target = NULL;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				break;
			}
			if (fseek(lex->fd, save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing sizeof expression intrinsic\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*lex = copy;
			*err = 0;
			build.data.size_of.target = pool_request(mem, sizeof(expression_ast));
			*build.data.size_of.target = parse_application_expression(lex, mem, parse_token(lex), err, end_token, allow_block, -1);
			if (*err != 0){
				return outer;
			}
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		case TOKEN_SET:
			if (outer.data.block.expr_c != 1){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Mutation with '=' requires 1 left operand, was operand number %u\n", lex->line, lex->col, outer.data.block.expr_c);
				return outer;
			}
			uint8_t tag = outer.data.block.expr_v[0].tag;
			if (tag != DEREF_EXPRESSION && tag != BINDING_EXPRESSION && tag != ACCESS_EXPRESSION){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u First operand of mutation '=' must be either a dereference or a bound variable name\n", lex->line, lex->col);
				return outer;
			}
			build.data.binding.type.tag=NONE_TYPE;
			build.data.binding.name=expr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_ARG:
			build = parse_lambda(lex, mem, err, end_token, &simple);
			if (*err != 0){
				return outer;
			}
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			if (simple == 1){
				if (pass == 0){
					return outer;
				}
				last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
				last_pass->data.block.expr_c += 1;
				return pass_expression;
			}
			break;
		case TOKEN_ADD:
		case TOKEN_SUB:
		case TOKEN_MUL:
		case TOKEN_DIV:
		case TOKEN_MOD:
		case TOKEN_SHL:
		case TOKEN_SHR:
		case TOKEN_ANGLE_OPEN:
		case TOKEN_ANGLE_CLOSE:
		case TOKEN_LESS_EQ:
		case TOKEN_GREATER_EQ:
		case TOKEN_EQ:
		case TOKEN_NOT_EQ:
		case TOKEN_BOOL_AND:
		case TOKEN_BOOL_OR:
		case TOKEN_BIT_AND:
		case TOKEN_BIT_OR:
		case TOKEN_BIT_XOR:
		case TOKEN_BIT_COMP:
		case TOKEN_BOOL_NOT:
		case TOKEN_PIPE_LEFT:
		case TOKEN_PIPE_RIGHT:
		case TOKEN_COMPOSE:
		case TOKEN_SYMBOL:
			if (outer.data.block.expr_c == 0){
				build.data.binding.type.tag = NONE_TYPE;
				build.data.binding.name=expr;
				outer.data.block.expr_v[0] = build;
				outer.data.block.expr_c = 1;
				break;
			}
			expression_ast left_expr = outer;
			outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
			outer.data.block.expr_c = 2;
			build.data.binding.type.tag=NONE_TYPE;
			build.data.binding.name=expr;
			outer.data.block.expr_v[0] = build;
			outer.data.block.expr_v[1] = left_expr;
			break;
		case TOKEN_CAST:
			if (outer.data.block.expr_c == 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Pointer cast requires left argument\n", lex->line, lex->col);
			}
			token open_br = parse_token(lex);
			if (open_br.type != TOKEN_BRACK_OPEN){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Attempted to cast to non pointer type\n", lex->line, lex->col);
				return outer;
			}
			type_ast cast_target = parse_type(lex, mem, err, parse_token(lex), TOKEN_BRACK_CLOSE, 1);
			if (*err != 0){
				return outer;
			}
			expression_ast ptr_cast = {
				.tag=CAST_EXPRESSION,
				.data.cast.target=pool_request(mem, sizeof(expression_ast)),
				.data.cast.type=cast_target
			};
			*ptr_cast.data.cast.target = outer;
			outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
			outer.data.block.expr_c = 1;
			outer.data.block.expr_v[0] = ptr_cast;
			break;
		case TOKEN_PASS:
			if (pass == 0){
				pass = 1;
				pass_expression = outer;
				outer.data.block.type.tag=NONE_TYPE;
				outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
				outer.data.block.expr_c = 0;
				last_pass = &pass_expression;
			}
			else{
				last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
				last_pass->data.block.expr_c += 1;
				outer.data.block.type.tag=NONE_TYPE;
				outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
				outer.data.block.expr_c = 0;
				last_pass = &last_pass->data.block.expr_v[last_pass->data.block.expr_c-1];
			}
			break;
		case TOKEN_FLOAT:
			build.tag = VALUE_EXPRESSION;
			build.data.binding.type=(type_ast){
				.tag=PRIMITIVE_TYPE,
				.data.primitive=FLOAT_ANY
			};
			build.data.binding.name=expr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_INTEGER:
			build.tag = VALUE_EXPRESSION;
			build.data.binding.type=(type_ast){
				.tag=PRIMITIVE_TYPE,
				.data.primitive=INT_ANY
			};
			build.data.binding.name=expr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_IDENTIFIER:
			build.data.binding.type.tag=NONE_TYPE;
			build.data.binding.name=expr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_BRACK_OPEN:
			copy = *lex;
			save = ftell(lex->fd);
			pool_save(mem);
			lit = parse_array_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				break;
			}
			if (fseek(lex->fd, save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing dereference expression\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*lex = copy;
			*err = 0;
			build = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_BRACK_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			expression_ast deref = {
				.tag=DEREF_EXPRESSION,
				.data.deref = pool_request(mem, sizeof(expression_ast))
			};
			*deref.data.deref = build;
			outer.data.block.expr_v[outer.data.block.expr_c] = deref;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_PAREN_OPEN:
			build = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_PAREN_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_BRACE_OPEN:
			copy = *lex;
			save = ftell(lex->fd);
			pool_save(mem);
			lit = parse_struct_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				break;
			}
			if (fseek(lex->fd, save, SEEK_SET) != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing dereference expression\n", lex->line, lex->col);
				return outer;
			}
			pool_load(mem);
			*lex = copy;
			*err = 0;
			build = parse_application_expression(lex, mem, parse_token(lex), err, TOKEN_BRACE_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			expression_ast access = {
				.tag=ACCESS_EXPRESSION,
				.data.deref = pool_request(mem, sizeof(expression_ast))
			};
			*access.data.deref = build;
			outer.data.block.expr_v[outer.data.block.expr_c] = access;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_TICK:
			binding_ast char_lit = parse_char_literal(lex, mem, err);
			if (*err != 0){
				return outer;
			}
			build.tag = VALUE_EXPRESSION;
			build.data.binding = char_lit;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_QUOTE:
			lit = parse_string_literal(lex, mem, err);
			if (*err != 0){
				return outer;
			}
			build.tag = LITERAL_EXPRESSION;
			build.data.literal = lit;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_IF:
			build = parse_application_expression(lex, mem, expr, err, end_token, 0, 3);
			if (*err != 0){
				return outer;
			}
			if (build.data.block.expr_c < 2){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error %u:%u Require at least one consequent for if special form\n", lex->line, lex->col);
				return outer;
			}
			statement_ast iff = {
				.tag=IF_STATEMENT,
				.data.if_statement.predicate = pool_request(mem, sizeof(expression_ast)),
				.data.if_statement.branch = pool_request(mem, sizeof(expression_ast)),
				.data.if_statement.alternate = NULL,
				.type.tag=NONE_TYPE
			};
			*iff.data.if_statement.predicate = build.data.block.expr_v[0];
			*iff.data.if_statement.branch = build.data.block.expr_v[1];
			if (build.data.block.expr_c == 3){
				iff.data.if_statement.alternate = pool_request(mem, sizeof(expression_ast));
				*iff.data.if_statement.alternate = build.data.block.expr_v[2];
			}
			build.tag = STATEMENT_EXPRESSION;
			build.data.statement = iff;
			build.data.statement.type.tag=NONE_TYPE;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_SEMI:
			if (allow_block == 1){
				if (outer.data.block.expr_c == 0){
					outer.tag = NOP_EXPRESSION;
					return parse_block_expression(lex, mem, err, end_token, outer);
				}
				if (pass == 0){
					return parse_block_expression(lex, mem, err, end_token, outer);
				}
				last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
				last_pass->data.block.expr_c += 1;
				return parse_block_expression(lex, mem, err, end_token, pass_expression);
			}
		case TOKEN_BRACK_CLOSE:
		case TOKEN_PAREN_CLOSE:
		default:
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unexpected token '%s'\n", lex->line, lex->col, expr.string);
			return outer;
		}
		if (limit != -1){
			limit_save = ftell(lex->fd);
			pool_save(mem);
			limit_copy = *lex;
			if (limit > 0){
				limit -= 1;
			}
		}
		expr = parse_token(lex);
	}
	snprintf(err, ERROR_BUFFER, " <!> Parser Error at %u:%u How did you get here in the application parser\n", lex->line, lex->col);
	return outer;
}

literal_ast parse_struct_literal(lexer* const lex, pool* const mem, char* err){
	literal_ast lit = {
		.tag=STRUCT_LITERAL,
		.type.tag=NONE_TYPE
	};
	lit.data.array.member_v = NULL;
	lit.data.array.member_c = 0;
	token tok = parse_token(lex);
	if (tok.type == TOKEN_BRACE_CLOSE){
		return lit;
	}
	lit.data.array.member_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	expression_ast build = parse_application_expression(lex, mem, tok, err, TOKEN_COMMA, 0, -1);
	if (*err != 0){
		return lit;
	}
	lit.data.array.member_v[lit.data.array.member_c] = build;
	lit.data.array.member_c += 1;
	tok = parse_token(lex);
	if (tok.type == TOKEN_BRACE_CLOSE){
		return lit;
	}
	while (1){
		uint64_t save;
		save = ftell(lex->fd);
		pool_save(mem);
		lexer copy = *lex;
		expression_ast build = parse_application_expression(lex, mem, tok, err, TOKEN_COMMA, 0, -1);
		if (*err == 0){
			lit.data.array.member_v[lit.data.array.member_c] = build;
			lit.data.array.member_c += 1;
			tok = parse_token(lex);
			continue;
		}
		if (fseek(lex->fd, save, SEEK_SET) != 0){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing struct literal\n", lex->line, lex->col);
			return lit;
		}
		pool_load(mem);
		*lex = copy;
		*err = 0;
		build = parse_application_expression(lex, mem, tok, err, TOKEN_BRACE_CLOSE, 0, -1);
		if (*err != 0){
			return lit;
		}
		lit.data.array.member_v[lit.data.array.member_c] = build;
		lit.data.array.member_c += 1;
		return lit;
	}
}

literal_ast parse_array_literal(lexer* const lex, pool* const mem, char* err){
	literal_ast lit = {
		.tag=ARRAY_LITERAL,
		.type.tag=NONE_TYPE
	};
	lit.data.array.member_v = NULL;
	lit.data.array.member_c = 0;
	token tok = parse_token(lex);
	if (tok.type == TOKEN_BRACK_CLOSE){
		return lit;
	}
	lit.data.array.member_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	expression_ast build = parse_application_expression(lex, mem, tok, err, TOKEN_COMMA, 0, -1);
	if (*err != 0){
		return lit;
	}
	lit.data.array.member_v[lit.data.array.member_c] = build;
	lit.data.array.member_c += 1;
	tok = parse_token(lex);
	if (tok.type == TOKEN_BRACK_CLOSE){
		return lit;
	}
	while (1){
		uint64_t save;
		save = ftell(lex->fd);
		pool_save(mem);
		lexer copy = *lex;
		build = parse_application_expression(lex, mem, tok, err, TOKEN_COMMA, 0, -1);
		if (*err == 0){
			lit.data.array.member_v[lit.data.array.member_c] = build;
			lit.data.array.member_c += 1;
			tok = parse_token(lex);
			continue;
		}
		if (fseek(lex->fd, save, SEEK_SET) != 0){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Unable to backtrack while parsing array literal\n", lex->line, lex->col);
			return lit;
		}
		pool_load(mem);
		*lex = copy;
		*err = 0;
		build = parse_application_expression(lex, mem, tok, err, TOKEN_BRACK_CLOSE, 0, -1);
		if (*err != 0){
			return lit;
		}
		lit.data.array.member_v[lit.data.array.member_c] = build;
		lit.data.array.member_c += 1;
		return lit;
	}
	snprintf(err, ERROR_BUFFER, " <!> Parser Error at %u:%u How did you get here in array literal parser\n", lex->line, lex->col);
	return lit;
}

binding_ast parse_char_literal(lexer* const lex, pool* const mem, char* err){
	//TODO excape sequences
	token tok = parse_token(lex);
	binding_ast char_lit = {.type.tag=NONE_TYPE};
	if (tok.len != 2){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected single character for character literal, found \"%s\"\n", lex->line, lex->col, tok.string);
		return char_lit;
	}
	char target = tok.string[0];
	snprintf(tok.string, 5, "%d", (int8_t)target);
	tok.len = 3;
	tok.type = TOKEN_INTEGER;
	char_lit.type.tag=PRIMITIVE_TYPE;
	char_lit.type.data.primitive=I8_TYPE;
	char_lit.name=tok;
	tok = parse_token(lex);
	if (tok.type != TOKEN_TICK){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at %u:%u Expected end of character literal token: (')\n", lex->line, lex->col);
		return char_lit;
	}
	return char_lit;
}

literal_ast parse_string_literal(lexer* const lex, pool* const mem, char* err){
	literal_ast lit = {
		.tag=STRING_LITERAL,
		.type.tag=NONE_TYPE
	};
	lit.data.string.content = pool_byte(mem);
	lit.data.string.length = 0;
	lit.data.string.content[0] = '\0';
	if (parse_crawl(lex, mem, lit.data.string.content, &lit.data.string.length) != 0){
		snprintf(err, ERROR_BUFFER, " <!> Parser Error at %u:%u Unable to parse string literal\n", lex->line, lex->col);
	}
	return lit;
}

void push_frame(scope* const s){
	s->frame_stack[s->frame_count] = s->binding_count;
	s->frame_count += 1;
}

void pop_frame(scope* const s){
	s->frame_count -= 1;
	s->binding_count = s->frame_stack[s->frame_count];
}

void push_binding(scope* const s, binding_ast binding){
	s->binding_stack[s->binding_count] = binding;
	s->binding_count += 1;
}

void pop_binding(scope* const s){
	s->binding_count -= 1;
}

void transform_ast(scope* const roll, ast* const tree, pool* const mem, char* err){
	for (uint32_t i = 0;i<tree->func_c;++i){
		function_ast* f = &tree->func_v[i];
		roll_type(roll, tree, mem, &f->type, err);
		if (*err != 0){
			return;
		}
		roll_expression(roll, tree, mem, &f->expression, f->type, 0, NULL, 1, err);
		if (*err != 0){
			return;
		}
	}
}

/* TODO LIST

	0 size intrinsic
	1 module system
	2 loops
	3 memory arena/pool builtin stuff         < TODO current task
		1 free intrinsic
		2 size calcuation function
	4 memory optimizations
		1 fix lost space in arena
		2 make structs smaller
		3 read from file at start, skipping system call on subsequent parse_token calls
	5 matches on enumerated struct union, maybe with @
	6 tagged break/continue that work with procedures
	7 parametric types
	8 Finish semantic pass stuff
		0 all the little todos
		1 fix type equality
		2 enum access with tag
		3 group function application into distinct calls for defined top level functions
		4 create structures for partial application cases
	9 code generation
	10 Good error system

*/
void handle_procedural_statement(scope* const roll, ast* const tree, pool* const mem, expression_ast* const line, type_ast expected_type, char* const err){
	if (expected_type.tag == PROCEDURE_TYPE){
		expected_type = *expected_type.data.pointer;
	}
	type_ast statement_return = roll_statement_expression(roll, tree, mem, &line->data.statement, expected_type, 0, err);
	if (*err != 0){
		pop_frame(roll);
		return;
	}
	if (statement_return.tag != PROCEDURE_TYPE){
		pop_frame(roll);
		snprintf(err, ERROR_BUFFER, " [!] Statement in block did not invoke procedure\n");
		return;
	}
	if (type_cmp(&expected_type, statement_return.data.pointer, FOR_APPLICATION) != 0){
		pop_frame(roll);
		snprintf(err, ERROR_BUFFER, " [!] Statement expression branch returned incorrect type to root block\n");
		return;
	}
}

void roll_type(scope* const roll, ast* const tree, pool* const mem, type_ast* const target, char* err){
	switch (target->tag){
	case FUNCTION_TYPE:
		roll_type(roll, tree, mem, target->data.function.left, err);
		if (*err != 0){
			return;
		}
		roll_type(roll, tree, mem, target->data.function.right, err);
		return;
	case PROCEDURE_TYPE:
	case POINTER_TYPE:
		roll_type(roll, tree, mem, target->data.pointer, err);
		return;
	case BUFFER_TYPE:
		if (target->data.buffer.constant == 1){
			constant_ast* param = constant_ast_map_access(&tree->constants, target->data.buffer.const_binding.string);
			if (param == NULL){
				snprintf(err, ERROR_BUFFER, " [!] Parameterized '%s' size not bound to constant\n", target->data.buffer.const_binding.string);
				return;
			}
			if (param->value.type.tag == PRIMITIVE_TYPE && param->value.type.data.primitive == INT_ANY){
				target->data.buffer.constant = 0;
				target->data.buffer.count = atoi(param->value.name.string);
			}
			else{
				snprintf(err, ERROR_BUFFER, " [!] Parameterized '%s' size bound to non integer constant\n", target->data.buffer.const_binding.string);
				return;
			}
		}
		roll_type(roll, tree, mem, target->data.buffer.base, err);
		return;
	case STRUCT_TYPE:
		roll_struct_type(roll, tree, mem, target->data.structure, err);
		return;
	case USER_TYPE:
	case PRIMITIVE_TYPE:
	case NONE_TYPE:
		return;
	default:
		snprintf(err, ERROR_BUFFER, " [!] Unexpected type tag\n");
		return;
	}
}

void roll_struct_type(scope* const roll, ast* const tree, pool* const mem, structure_ast* const target, char* err){
	if (target == NULL){
		return;
	}
	for (uint32_t i = 0;i<target->binding_c;++i){
		roll_type(roll, tree, mem, &target->binding_v[i].type, err);
		if (*err != 0){
			return;
		}
	}
	for (uint32_t i = 0;i<target->union_c;++i){
		roll_struct_type(roll, tree, mem, &target->union_v[i], err);
		if (*err != 0){
			return;
		}
	}
}

type_ast roll_expression(
	scope* const roll,
	ast* const tree,
	pool* const mem,
	expression_ast* const expr,
	type_ast expected_type,
	uint32_t argc,
	expression_ast* const argv,
	uint8_t prevent_lift,
	char* err
){
	type_ast infer = {
		.tag=NONE_TYPE
	};
	switch(expr->tag){

	case BLOCK_EXPRESSION:
		if (expr->data.block.type.tag != NONE_TYPE){
			return expr->data.block.type;
		}
		push_frame(roll);
		if (expr->data.block.expr_c == 0){
			pop_frame(roll);
			if (expected_type.tag == NONE_TYPE){
				expr->data.block.type = infer;
				return infer;
			}
			snprintf(err, ERROR_BUFFER, " [!] Block expression expected return expression\n");
			return infer;
		}
		uint32_t i = 0;
		expression_ast* line;
		for (;i<expr->data.block.expr_c-1;++i){
			line = &expr->data.block.expr_v[i];
			if (line->tag == STATEMENT_EXPRESSION) {
				handle_procedural_statement(roll, tree, mem, line, expected_type, err);
				if (*err != 0){
					pop_frame(roll);
					return expected_type;
				}
				continue;
			}
			roll_expression(roll, tree, mem, line, infer, 0, NULL, 0, err);
			if (*err != 0){
				pop_frame(roll);
				return infer;
			}
		}
		line = &expr->data.block.expr_v[i];
		if ((line->tag != RETURN_EXPRESSION) && (expected_type.tag == PROCEDURE_TYPE)){
			if (line->tag == STATEMENT_EXPRESSION){
				handle_procedural_statement(roll, tree, mem, line, expected_type, err);
				if (*err != 0){
					return expected_type;
				}
			}
			else{
				roll_expression(roll, tree, mem, line, infer, 0, NULL, 0, err);
			}
			pop_frame(roll);
			expr->data.block.type = expected_type;
			return expected_type;
		}
		type_ast filled_type;
		if (expected_type.tag == PROCEDURE_TYPE){
			filled_type = roll_expression(roll, tree, mem, line->data.deref, *expected_type.data.pointer, 0, NULL, 0, err);
			pop_frame(roll);
			if (*err != 0){
				return filled_type;
			}
			expr->data.block.type = expected_type;
			return expected_type;
		}
		filled_type = roll_expression(roll, tree, mem, line->data.deref, expected_type, 0, NULL, 0, err);
		pop_frame(roll);
		if (*err != 0){
			return filled_type;
		}
		if (expected_type.tag == NONE_TYPE){
			expr->data.block.type = filled_type;
			return filled_type;
		}
		expr->data.block.type = filled_type;
		return filled_type;

	case CLOSURE_EXPRESSION:
		if (expected_type.tag != NONE_TYPE){
			snprintf(err, ERROR_BUFFER, " [!] Unexpected closure encountered in expression that requires determinable type\n");
			return expected_type;
		}
		roll_type(roll, tree, mem, &expr->data.closure.func->type, err);
		if (*err != 0){
			return expected_type;
		}
		type_ast desired = expr->data.closure.func->type;
		expression_ast* equation = &expr->data.closure.func->expression;
		binding_ast scope_item = {
			.type=desired,
			.name=expr->data.closure.func->name
		};
		uint8_t needs_capture = 0;
		if (scope_contains(roll, &scope_item, &needs_capture) != NULL){
			snprintf(err, ERROR_BUFFER, " [!] Binding with name '%s' already in scope\n", scope_item.name.string);
			return expected_type;
		}
		if (needs_capture == 1){
			push_capture_binding(roll, scope_item);
		}
		if (expr->data.closure.func->expression.tag == LAMBDA_EXPRESSION){
			push_binding(roll, scope_item);
			push_capture_frame(roll, mem);
			roll_expression(roll, tree, mem, equation, desired, 0, NULL, 1, err);
		}
		else{
			push_capture_frame(roll, mem);
			roll_expression(roll, tree, mem, equation, desired, 0, NULL, 1, err);
			push_binding(roll, scope_item);
		}
		binding_ast* captured_binds = NULL;
		uint16_t num_caps = pop_capture_frame(roll, &captured_binds);
		if (expr->data.closure.func->expression.tag == LAMBDA_EXPRESSION){
			lambda_ast* focus_lambda = &expr->data.closure.func->expression.data.lambda;
			type_ast captured_type = prepend_captures(desired, captured_binds, num_caps, mem);
			for (uint32_t i = 0;i<focus_lambda->argc;++i){
				uint32_t index = focus_lambda->argc-(i+1);
				focus_lambda->argv[index+num_caps] = focus_lambda->argv[index];
			}
			for (uint32_t i = 0;i<num_caps;++i){
				focus_lambda->argv[num_caps-(1+i)] = captured_binds[i].name;
			}
			focus_lambda->argc += num_caps;
			function_ast lifted_closure = *expr->data.closure.func;
			lifted_closure.type = captured_type;
			snprintf(lifted_closure.name.string, TOKEN_MAX, ":CLOSURE_%u", tree->lifted_lambdas);
			tree->lifted_lambdas += 1;
			tree->func_v[tree->func_c] = lifted_closure;
			function_ast_map_insert(&tree->functions, tree->func_v[tree->func_c].name.string, &tree->func_v[tree->func_c]);
			tree->func_c += 1;
			binding_ast new_binding = {
				.type=lifted_closure.type,
				.name=lifted_closure.name
			};
			expression_ast new_binding_expr = {
				.tag=BINDING_EXPRESSION,
				.data.binding=new_binding
			};
			uint32_t repl_size = num_caps+1;
			expression_ast new_application_expr = {
				.tag=APPLICATION_EXPRESSION,
				.data.block.expr_c=repl_size,
				.data.block.expr_v=pool_request(mem, repl_size*sizeof(expression_ast)),
				.data.block.type=desired
			};
			new_application_expr.data.block.expr_v[0] = new_binding_expr;
			for (uint32_t i = 1;i<repl_size;++i){
				expression_ast repl_binding = {
					.tag=BINDING_EXPRESSION,
					.data.binding=captured_binds[i-1]
				};
				new_application_expr.data.block.expr_v[repl_size-i] = repl_binding;
			}
			expr->data.closure.func->expression = new_application_expr;
		}
		return expected_type;

	case APPLICATION_EXPRESSION:
		if (expr->data.block.type.tag != NONE_TYPE){
			return expr->data.block.type;
		}
		if (expr->data.block.expr_c == 1){
			type_ast only = roll_expression(roll, tree, mem, &expr->data.block.expr_v[0], expected_type, argc, argv, prevent_lift, err);
			if (*err == 0){
				expr->data.block.type = only;
			}
			return only;
		}
		uint32_t index = 0;
		uint8_t is_mutation = 0;
		if (expr->data.block.expr_c > 2){
			expression_ast set = expr->data.block.expr_v[1];
			if (set.tag == BINDING_EXPRESSION && set.data.binding.name.type == TOKEN_SET){
				index = 2;
				is_mutation = 1;
			}
		}
		expression_ast* leftmost = &expr->data.block.expr_v[index];
		index += 1;
		type_ast lefttype = infer;
		if (is_mutation == 1){
			expression_ast* left_side = &expr->data.block.expr_v[0];
			lefttype = roll_expression(roll, tree, mem, left_side, infer, 0, NULL, 0, err);
			if (*err != 0){
				return lefttype;
			}
		}
		type_ast full_type = roll_expression(roll, tree, mem, leftmost, lefttype, expr->data.block.expr_c-1, &expr->data.block.expr_v[index], 0, err);
		if (*err != 0){
			return expected_type;
		}
	   	for (;index<expr->data.block.expr_c;++index){
			expression_ast* term = &expr->data.block.expr_v[index];
			type_ast left_type = apply_type(&full_type, err);
			if (*err != 0){
				return full_type;
			}
			roll_expression(roll, tree, mem, term, left_type, 0, NULL, 0, err);
			if (*err != 0){
				return full_type;
			}
		}
		if (is_mutation == 1){
			if (type_cmp(&lefttype, &full_type, FOR_MUTATION) != 0){
				snprintf(err, ERROR_BUFFER, " [!] Left side of mutation must match right type, and be mutable\n");
				return lefttype;
			}
			full_type = lefttype;
		}
		if (expected_type.tag != NONE_TYPE){
			if (type_cmp(&expected_type, &full_type, FOR_APPLICATION) != 0){
				snprintf(err, ERROR_BUFFER, " [!] Application expression returned unexpected type\n");
				return full_type;
			}
		}
		expr->data.block.type = full_type;
		return full_type;

	case STATEMENT_EXPRESSION:
		return roll_statement_expression(roll, tree, mem, &expr->data.statement, expected_type, 1, err);

	case BINDING_EXPRESSION:
		if (expr->data.binding.type.tag != NONE_TYPE){
			return expr->data.binding.type;
		}
		uint8_t needs_capturing = 0;
		type_ast* bound_type = scope_contains(roll, &expr->data.binding, &needs_capturing);
		if (bound_type == NULL){
			function_ast* bound_function = function_ast_map_access(&tree->functions, expr->data.binding.name.string);
			if (bound_function != NULL){
				bound_type = &bound_function->type;
			}
			else{
				constant_ast* bound_constant = constant_ast_map_access(&tree->constants, expr->data.binding.name.string);
				if (bound_constant != NULL){
					bound_type = &bound_constant->value.type;
					needs_capturing = 0; // just in case
				}
				else{
					snprintf(err, ERROR_BUFFER, " [!] Binding '%s' is not defined in current scope\n", expr->data.binding.name.string);
					return expected_type;
				}
			}
		}
		if (expected_type.tag == NONE_TYPE){
			expr->data.binding.type = *bound_type;
			if (needs_capturing == 1){
				push_capture_binding(roll, expr->data.binding);
			}
			return expr->data.binding.type;
		}
		if (type_cmp(&expected_type, bound_type, FOR_APPLICATION) != 0){
			type_ast expected_alias = expected_type;
			type_ast bound_alias = *bound_type;
			reduce_aliases(tree, &expected_alias, &bound_alias);
			if (type_cmp(&expected_alias, &bound_alias, FOR_APPLICATION) != 0){
				snprintf(err, ERROR_BUFFER, " [!] Binding '%s' was not the expected type\n", expr->data.binding.name.string);
				return expected_type;
			}
		}
		expr->data.binding.type = expected_type;
		if (needs_capturing == 1){
			push_capture_binding(roll, expr->data.binding);
		}
		return expected_type;

	case VALUE_EXPRESSION:
		if (expr->data.binding.type.tag == NONE_TYPE){
			snprintf(err, ERROR_BUFFER, " [!] Untyped value expression encountered\n");
			return expected_type;
		}
		if (expected_type.tag == NONE_TYPE){
			return expr->data.binding.type;
		}
		if (expected_type.tag == USER_TYPE){
			type_ast descended_type = resolve_type_or_alias(tree, expected_type, err);
			if (*err != 0){
				return descended_type;
			}
			if (type_cmp(&descended_type, &expr->data.binding.type, FOR_APPLICATION) != 0){
				snprintf(err, ERROR_BUFFER, " [!] Expected user primitive type did not match literal primitive value\n");
				return descended_type;
			}
			expr->data.binding.type = expected_type;
			return expected_type;
		}
		if (type_cmp(&expected_type, &expr->data.binding.type, FOR_APPLICATION) != 0){
			snprintf(err, ERROR_BUFFER, " [!] Primitive type mismatch\n");
		}
		return expr->data.binding.type;

	case LITERAL_EXPRESSION:
		return roll_literal_expression(roll, tree, mem, &expr->data.literal, expected_type, err);

	case DEREF_EXPRESSION:
		expression_ast* location = expr->data.deref;
		if (location->data.block.type.tag != NONE_TYPE){
			return location->data.block.type;
		}
		if (location->data.block.expr_c == 1){
			if (expected_type.tag != NONE_TYPE){
				type_ast wrapped_expected = {.tag=POINTER_TYPE};
				wrapped_expected.data.pointer = &expected_type;
				roll_expression(roll, tree, mem, location, wrapped_expected, 0, NULL, 0, err);
				return expected_type;
			}
			type_ast only = roll_expression(roll, tree, mem, &location->data.block.expr_v[0], expected_type, 0, NULL, 0, err);
			if (only.tag != POINTER_TYPE){
				snprintf(err, ERROR_BUFFER, " [!] Tried to dereference non pointer\n");
				return only;
			}
			location->data.block.type = *only.data.pointer;
			return *only.data.pointer;
		}
		uint32_t d_index = 0;
		if (location->data.block.expr_c > 2){
			expression_ast set = location->data.block.expr_v[1];
			if (set.tag == BINDING_EXPRESSION && set.data.binding.name.type == TOKEN_SET){
				snprintf(err, ERROR_BUFFER, " [!] Expected pointer or buffer to dereference, found mutation expression\n");
				return expected_type;
			}
		}
		expression_ast* d_leftmost = &location->data.block.expr_v[d_index];
		d_index += 1;
		type_ast d_full_type = roll_expression(roll, tree, mem, d_leftmost, infer, location->data.block.expr_c-1, &location->data.block.expr_v[d_index], 0, err);
		if (*err != 0){
			return expected_type;
		}
		for (;d_index < location->data.block.expr_c;++d_index){
			expression_ast* term = &location->data.block.expr_v[d_index];
			if (d_full_type.tag == USER_TYPE){
				d_full_type = resolve_type_or_alias(tree, d_full_type, err);
				if (*err != 0){
					return d_full_type;
				}
			}
			if (d_full_type.tag != POINTER_TYPE && d_full_type.tag != BUFFER_TYPE){
				type_ast left_type = apply_type(&d_full_type, err);
				if (*err != 0){
					return d_full_type;
				}
				roll_expression(roll, tree, mem, term, left_type, 0, NULL, 0, err);
				if (*err != 0){
					return d_full_type;
				}
				continue;
			}
			if (term->tag != BINDING_EXPRESSION){
				type_ast arg_type = roll_expression(roll, tree, mem, term, infer, 0, NULL, 0, err);
				if (*err != 0){
					return d_full_type;
				}
				type_ast arb_int = {
					.tag=PRIMITIVE_TYPE,
					.data.primitive=INT_ANY
				};
				if (type_cmp(&arb_int, &arg_type, FOR_APPLICATION) == 0){
					type_ast base_typ;
					switch(d_full_type.tag){
					case POINTER_TYPE:
						base_typ = *d_full_type.data.pointer;
						break;
					case BUFFER_TYPE:
						base_typ = *d_full_type.data.buffer.base;
						break;
					default:
						snprintf(err, ERROR_BUFFER, " [!] How did you get here in dereference roll\n");
						return d_full_type;
					}
					d_full_type = base_typ;
					continue;
				}
				snprintf(err, ERROR_BUFFER, " [!] Cannot dereference at non integer index\n");
				return d_full_type;
			}
			if (d_full_type.tag == BUFFER_TYPE){
				snprintf(err, ERROR_BUFFER, " [!] Expected a pointer to dereference, was neither pointer, buffer access, pointer buffer access, or struct member dereference\n");
				return d_full_type;
			}
			type_ast matched_type = *d_full_type.data.pointer;
			if (d_full_type.data.pointer->tag == USER_TYPE){
				matched_type = resolve_type_or_alias(tree, *d_full_type.data.pointer, err);
				if (*err != 0){
					return d_full_type;
				}
			}
			if (matched_type.tag != STRUCT_TYPE){
				snprintf(err, ERROR_BUFFER, " [!] Expected structure to dereference member\n");
				return d_full_type;
			}
			structure_ast* target_struct = matched_type.data.structure;
			type_ast accessed_type;
			uint8_t found = 0;
			for (uint32_t k = 0;k<target_struct->binding_c;++k){
				binding_ast target_binding = target_struct->binding_v[k];
				if (strncmp(term->data.binding.name.string, target_binding.name.string, TOKEN_MAX) == 0){
					accessed_type = target_binding.type;
					found = 1;
					break;
				}
			}
			if (found == 0){
				snprintf(err, ERROR_BUFFER, " [!] No member found in dereferenced structure\n");
				return d_full_type;
			}
			d_full_type = accessed_type;
		}
		if (d_full_type.tag == POINTER_TYPE){
			location->data.block.type = *d_full_type.data.pointer;
			return *d_full_type.data.pointer;
		}
		location->data.block.type = d_full_type;
		return d_full_type;

	case ACCESS_EXPRESSION:
		expression_ast* apl = expr->data.deref;
		if (apl->data.block.expr_c == 1){
			snprintf(err, ERROR_BUFFER, " [!] Expected member to access in structure\n");
			return expected_type;
		}
		uint32_t access_index = 0;
		expression_ast* access_leftmost = &apl->data.block.expr_v[access_index];
		access_index += 1;
		type_ast access_full_type = roll_expression(roll, tree, mem, access_leftmost, infer, apl->data.block.expr_c-1, &apl->data.block.expr_v[access_index], 0, err);
		if (*err != 0){
			return expected_type;
		}
		structure_ast target_struct;
		uint8_t found = 1;
		for (;access_index<apl->data.block.expr_c;++access_index){
			if (found == 1){
				found = 0;
				if (access_full_type.tag == STRUCT_TYPE){
					target_struct = *access_full_type.data.structure;
				}
				else if (access_full_type.tag == USER_TYPE){
					type_ast lookup = resolve_type_or_alias(tree, access_full_type, err);
					if (*err != 0){
						return access_full_type;
					}
					if (lookup.tag != STRUCT_TYPE){
						snprintf(err, ERROR_BUFFER, " [!] Target type binding for struct access does not resolve to structure\n");
						return access_full_type;
					}
					target_struct = *lookup.data.structure;
				}
				else{
					snprintf(err, ERROR_BUFFER, " [!] Expected anonymous struct or user struct type literal for struct access target\n");
					return access_full_type;
				}
			}
			expression_ast* term = &apl->data.block.expr_v[access_index];
			if (term->tag != BINDING_EXPRESSION){
				snprintf(err, ERROR_BUFFER, " [!] Expected structure member name\n");
				return access_full_type;
			}
			structure_ast* temp_struct = &target_struct;
			uint32_t next_union = 0;
			do {
				for (uint32_t k = 0;k<temp_struct->binding_c;++k){
					binding_ast target_binding = temp_struct->binding_v[k];
					if (strncmp(term->data.binding.name.string, target_binding.name.string, TOKEN_MAX) == 0){
						if (target_binding.type.tag == STRUCT_TYPE){
							found = 1;
							access_full_type = target_binding.type;
							break;
						}
						else if (target_binding.type.tag == USER_TYPE){
							type_ast resolved_type = resolve_type_or_alias(tree, target_binding.type, err);
							if (*err != 0){
								return access_full_type;
							}
							if (resolved_type.tag == STRUCT_TYPE){
								if (access_index+1<apl->data.block.expr_c){
									found = 1;
									access_full_type = resolved_type;
									break;
								}
								apl->data.block.type = target_binding.type;
								return target_binding.type;
							}
						}
						if ((access_index+1)<apl->data.block.expr_c){
							snprintf(err, ERROR_BUFFER, " [!] Structure access resolved to a type but further arguments were given\n");
							return access_full_type;
						}
						if (expected_type.tag == NONE_TYPE){
							apl->data.block.type = target_binding.type;
							return target_binding.type;
						}
						if (type_cmp(&expected_type, &target_binding.type, FOR_APPLICATION) != 0){
							snprintf(err, ERROR_BUFFER, " [!] Structure access returned unexpected type\n");
							return access_full_type;
						}
						apl->data.block.type = target_binding.type;
						return target_binding.type;
					}
				}
				if (found != 0){
					break;
				}
				temp_struct = &target_struct.union_v[next_union];
				next_union += 1;
			} while (next_union <= target_struct.union_c);
			if (found == 0){
				snprintf(err, ERROR_BUFFER, " [!] Unable to find structure member '%s'\n", term->data.binding.name.string);
				return access_full_type;
			}
		}
		if (expected_type.tag == NONE_TYPE){
			apl->data.block.type = access_full_type;
			return access_full_type;
		}
		if (expected_type.tag == USER_TYPE){
			type_ast descended_type = resolve_type_or_alias(tree, expected_type, err);
			if (*err != 0){
				return descended_type;
			}
			if (type_cmp(&descended_type, &access_full_type, FOR_APPLICATION) != 0){
				snprintf(err, ERROR_BUFFER, " [!] Expected user primitive type did not match structure access\n");
				return descended_type;
			}
		}
		else if (type_cmp(&expected_type, &access_full_type, FOR_APPLICATION) != 0){
			snprintf(err, ERROR_BUFFER, " [!] Structure access returned unexpected type\n");
			return access_full_type;
		}
		apl->data.block.type = access_full_type;
		return access_full_type;

	case LAMBDA_EXPRESSION:
		if (expr->data.lambda.type.tag != NONE_TYPE){
			return expr->data.lambda.type;
		}
		push_frame(roll);
		type_ast outer_copy = expected_type;
		if (expected_type.tag != NONE_TYPE){
			if (prevent_lift == 0){
				push_capture_frame(roll, mem);
			}
			for (uint32_t i = 0;i<expr->data.lambda.argc;++i){
				token candidate = expr->data.lambda.argv[i];
				type_ast declared_type = apply_type(&expected_type, err);
				if (*err != 0){
					pop_frame(roll);
					return declared_type;
				}
				binding_ast scope_item = {
					.type=declared_type,
					.name=candidate
				};
				if (scope_contains(roll, &scope_item, NULL) != NULL){
					pop_frame(roll);
					snprintf(err, ERROR_BUFFER, " [!] Binding with name '%s' already in scope\n", scope_item.name.string);
					return expected_type;
				}
				push_binding(roll, scope_item);
			}
			type_ast constructed = roll_expression(roll, tree, mem, expr->data.lambda.expression, expected_type, 0, NULL, 0, err);
			pop_frame(roll);
			if (*err != 0){
				return constructed;
			}
			if (type_cmp(&expected_type, &constructed, FOR_APPLICATION) != 0){
				snprintf(err, ERROR_BUFFER, " [!] Lambda expression returned unexpected type\n");
				return constructed;
			}
			if (prevent_lift == 0){
				binding_ast* captured_bindings = NULL;
				uint16_t total_captures = pop_capture_frame(roll, &captured_bindings);
				type_ast captured_type = prepend_captures(outer_copy, captured_bindings, total_captures, mem);
				lift_lambda(tree, expr, captured_type, captured_bindings, total_captures, mem);
				expr->data.block.type = outer_copy;
			}
			else{
				expr->data.lambda.type = outer_copy;
			}
			return outer_copy;
		}
		if (argc < expr->data.lambda.argc){
			snprintf(err, ERROR_BUFFER, " [!] Too few arguments for lambda expression\n");
			pop_frame(roll);
			return expected_type;
		}
		type_ast constructed;
		type_ast* focus = &constructed;
		if (prevent_lift == 0){
			push_capture_frame(roll, mem);
		}
		for (uint32_t i = 0;i<expr->data.lambda.argc;++i){
			expression_ast* arg_expr = &argv[i];
			type_ast arg_type = roll_expression(roll, tree, mem, arg_expr, expected_type, 0, NULL, 0, err);
			if (*err != 0){
				pop_frame(roll);
				return expected_type;
			}
			binding_ast scope_item = {
				.type=arg_type,
				.name=expr->data.lambda.argv[i]
			};
			if (scope_contains(roll, &scope_item, NULL) != NULL){
				snprintf(err, ERROR_BUFFER, " [!] Binding with name '%s' already in scope\n", scope_item.name.string);
				pop_frame(roll);
				return expected_type;
			}
			push_binding(roll, scope_item);
			focus->tag = FUNCTION_TYPE;
			focus->data.function.left = pool_request(mem, sizeof(type_ast));
			focus->data.function.right = pool_request(mem, sizeof(type_ast));
			*focus->data.function.left = arg_type;
			focus = focus->data.function.right;
		}
		type_ast defin = roll_expression(roll, tree, mem, expr->data.lambda.expression, expected_type, 0, NULL, 0, err);
		pop_frame(roll);
		if (*err != 0){
			return expected_type;
		}
		*focus = defin;
		if (prevent_lift == 0){
			binding_ast* captured_bindings = NULL;
			uint16_t total_captures = pop_capture_frame(roll, &captured_bindings);
			type_ast captured_type = prepend_captures(constructed, captured_bindings, total_captures, mem);
			lift_lambda(tree, expr, captured_type, captured_bindings, total_captures, mem);
			expr->data.block.type = constructed;
		}
		else{
			expr->data.lambda.type = constructed;
		}
		return constructed;

	case RETURN_EXPRESSION:
		type_ast result = roll_expression(roll, tree, mem, expr->data.deref, expected_type, 0, NULL, 0, err);
		if (*err != 0){
			return result;
		}
		return result;

	case REF_EXPRESSION:
		if (expected_type.tag == NONE_TYPE){
			type_ast unref = roll_expression(roll, tree, mem, expr->data.deref, expected_type, 0, NULL, 0, err);
			if (*err != 0){
				return unref;
			}
			type_ast temp = unref;
			unref.tag = POINTER_TYPE;
			unref.data.pointer = pool_request(mem, sizeof(type_ast));
			*unref.data.pointer = temp;
			return unref;
		}
		if (expected_type.tag != POINTER_TYPE){
			snprintf(err, ERROR_BUFFER, " [!] Reference taken where non pointer was expected\n");
			return expected_type;
		}
		type_ast unref = roll_expression(roll, tree, mem, expr->data.deref, *expected_type.data.pointer, 0, NULL, 0, err);
		if (*err != 0){
			return unref;
		}
		return expected_type;

	case CAST_EXPRESSION:
		type_ast cast_left_type = roll_expression(roll, tree, mem, expr->data.cast.target, infer, 0, NULL, 0, err);
		if (*err != 0){
			return cast_left_type;
		}
		if (cast_left_type.tag == POINTER_TYPE){
			cast_left_type.data.pointer = pool_request(mem, sizeof(type_ast));
			*cast_left_type.data.pointer = expr->data.cast.type;
		}
		else if (cast_left_type.tag == BUFFER_TYPE){
			cast_left_type.data.buffer.base = pool_request(mem, sizeof(type_ast));
			*cast_left_type.data.buffer.base = expr->data.cast.type;
		}
		else{
			snprintf(err, ERROR_BUFFER, " [!] Both sides of cast must be of pointer or buffer type\n");
			return cast_left_type;
		}
		if (expected_type.tag == NONE_TYPE){
			return cast_left_type;
		}
		if (type_cmp(&expected_type, &cast_left_type, FOR_APPLICATION) != 0){
			snprintf(err, ERROR_BUFFER, " [!] Pointed type cast to mismatched type\n");
			return cast_left_type;
		}
		return cast_left_type;

	case SIZEOF_EXPRESSION:
		if (expr->data.size_of.target != NULL){
			expr->data.size_of.type = roll_expression(roll, tree, mem, expr->data.size_of.target, infer, 0, NULL, 0, err);
			if (*err != 0){
				return expected_type;
			}
		}
		uint64_t target_size = 64; // TODO placeholder for size function
		expr->data.size_of.size = target_size;
		return (type_ast){
			.tag=PRIMITIVE_TYPE,
			.data.primitive=INT_ANY
		};

	default:
		snprintf(err, ERROR_BUFFER, " [!] Unexpected expression type\n");
	}
	return expected_type;
}

void lift_lambda(ast* const tree, expression_ast* expr, type_ast captured_type, binding_ast* captured_bindings, uint16_t total_captures, pool* const mem){
	expr->data.lambda.type = captured_type;
	expression_ast save_lambda = {
		.tag=LAMBDA_EXPRESSION,
		.data.lambda=expr->data.lambda
	};
	for (uint32_t i = 0;i<save_lambda.data.lambda.argc;++i){
		uint32_t index = save_lambda.data.lambda.argc-(i+1);
		save_lambda.data.lambda.argv[index+total_captures] = save_lambda.data.lambda.argv[index];
	}
	for (uint32_t i = 0;i<total_captures;++i){
		save_lambda.data.lambda.argv[total_captures-(1+i)] = captured_bindings[i].name;
	}
	save_lambda.data.lambda.argc += total_captures;
	token new_token = {
		.type=TOKEN_IDENTIFIER,
		.string={0}
	};
	snprintf(new_token.string, TOKEN_MAX, ":LAMBDA_%u", tree->lifted_lambdas);
	new_token.len=strlen(new_token.string);
	tree->lifted_lambdas += 1;
	expression_ast repl_lambda_binding = {
		.tag=BINDING_EXPRESSION,
		.data.binding.name=new_token,
		.data.binding.type=captured_type
	};
	expr->tag = APPLICATION_EXPRESSION;
	uint32_t repl_size = total_captures+1;
	expr->data.block.expr_c = repl_size;
	expr->data.block.expr_v = pool_request(mem, sizeof(expression_ast)*repl_size);
	expr->data.block.expr_v[0] = repl_lambda_binding;
	for (uint32_t repl_term = 1;repl_term < repl_size;++repl_term){
		expression_ast repl_binding = {
			.tag=BINDING_EXPRESSION,
			.data.binding=captured_bindings[repl_term-1]
		};
		expr->data.block.expr_v[repl_size-repl_term] = repl_binding;
	}
	function_ast f = {
		.type=captured_type,
		.enclosing=0,
		.name=new_token,
		.expression=save_lambda
	};
	tree->func_v[tree->func_c] = f;
	function_ast_map_insert(&tree->functions, tree->func_v[tree->func_c].name.string, &tree->func_v[tree->func_c]);
	tree->func_c += 1;
}

type_ast prepend_captures(type_ast start, binding_ast* captures, uint16_t total_captures, pool* const mem){
	for (uint16_t i = 0;i<total_captures;++i){
		binding_ast binding = captures[i];
		type_ast outer = {
			.tag=FUNCTION_TYPE
		};
		outer.data.function.left = pool_request(mem, sizeof(type_ast));
		outer.data.function.right = pool_request(mem, sizeof(type_ast));
		*outer.data.function.left = binding.type;
		*outer.data.function.right = start;
		start = outer;
	}
	return start;
}

void push_capture_frame(scope* const roll, pool* const mem){
	capture_stack* target = roll->captures;
	roll->capture_frame += 1;
	if (target->next != NULL){
		target = target->next;
		target->size = 0;
		target->binding_count_point = roll->binding_count;
		roll->captures = target;
		return;
	}
	target->next = pool_request(mem, sizeof(capture_stack));
	target = target->next;
	target->prev = roll->captures;
	target->next = NULL;
	target->size = 0;
	target->binding_count_point = roll->binding_count;
	roll->captures = target;
}

uint16_t pop_capture_frame(scope* const roll, binding_ast** list_result){
	if (list_result != NULL){
		*list_result = roll->captures->binding_list;
	}
	uint16_t size = roll->captures->size;
	roll->captures = roll->captures->prev;
	return size;
}

void push_capture_binding(scope* const roll, binding_ast binding){
	if (roll->captures->size >= MAX_CAPTURES){
		fprintf(stderr, "Capture limit exceeded in stack frame\n");
		return;
	}
	roll->captures->binding_list[roll->captures->size] = binding;
	roll->captures->size += 1;
}

void reduce_aliases(ast* const tree, type_ast* left, type_ast* right){
	while (left->tag == USER_TYPE || right->tag == USER_TYPE){
		new_type_ast* left_alias = alias_ast_map_access(&tree->aliases, left->data.user.string);
		new_type_ast* right_alias = alias_ast_map_access(&tree->aliases, right->data.user.string);
		if (left_alias != NULL){
			*left = left_alias->type;
			if (right_alias != NULL){
				*right = right_alias->type;
			}
		}
		else if (right_alias != NULL){
			*right = right_alias->type;
		}
		else{
			return;
		}
	}
}

type_ast resolve_alias(ast* const tree, type_ast root, char* err){
	uint8_t found = 0;
	while (root.tag == USER_TYPE){
		new_type_ast* primitive_alias = alias_ast_map_access(&tree->aliases, root.data.user.string);
		if (primitive_alias == NULL){
			if (found == 0){
				snprintf(err, ERROR_BUFFER, " [!] Unknown user type or alias\n");
			}
			return root;
		}
		found = 1;
		root = primitive_alias->type;
	}
	return root;
}

type_ast resolve_type_or_alias(ast* const tree, type_ast root, char* err){
	while (root.tag == USER_TYPE){
		new_type_ast* primitive_new_type = new_type_ast_map_access(&tree->types, root.data.user.string);
		if (primitive_new_type != NULL){
			root = primitive_new_type->type;
			continue;
		}
		root = resolve_alias(tree, root, err);
		if (*err != 0){
			return root;
		}
	}
	return root;
}

type_ast apply_type(type_ast* const func, char* err){
	type_ast left = {.tag=NONE_TYPE};
	if (func->tag != FUNCTION_TYPE){
		snprintf(err, ERROR_BUFFER, " [!] Tried to set lambda to non function binding\n");
		return left;
	}
	left = *func->data.function.left;
	*func = *func->data.function.right;
	return left;
}

uint8_t type_cmp(type_ast* const a, type_ast* const b, TYPE_CMP_PURPOSE purpose){
	if (a->tag == INTERNAL_ANY_TYPE || b->tag == INTERNAL_ANY_TYPE){
		return 0;
	}
	switch(purpose){
	case FOR_MUTATION:
		if (a->mut == 0 || a->tag == PROCEDURE_TYPE){
			return 1;
		}
		break;
	case FOR_APPLICATION:
		if (a->tag == POINTER_TYPE && b->tag == BUFFER_TYPE){
			return type_cmp(a->data.pointer, b->data.buffer.base, FOR_EQUALITY);
		}
		break;
	default:
	case FOR_EQUALITY:
		//TODO im not fully convinced nothing should go here for mutation's sake
	}
	if (a->tag != b->tag){
		return 1;
	}
	switch (a->tag){
	case FUNCTION_TYPE:
		return type_cmp(a->data.function.left, b->data.function.left, FOR_EQUALITY)
			 + type_cmp(a->data.function.right, b->data.function.right, FOR_EQUALITY);
	case PRIMITIVE_TYPE:
		PRIMITIVE_TAGS a_cast = a->data.primitive;
		PRIMITIVE_TAGS b_cast = b->data.primitive;
		if ((a_cast == INT_ANY && b_cast <= a_cast) || (b_cast == INT_ANY && a_cast <= b_cast)){
			return 0;
		}
		if (a_cast > INT_ANY){
			return 0;
		}
		return (a->data.primitive != b->data.primitive);//TODO this might need more nuance
	case PROCEDURE_TYPE:
	case POINTER_TYPE:
		return type_cmp(a->data.pointer, b->data.pointer, FOR_EQUALITY);
	case BUFFER_TYPE:
		return (a->data.buffer.count != b->data.buffer.count)
			 + type_cmp(a->data.buffer.base, b->data.buffer.base, FOR_EQUALITY);
	case USER_TYPE:
		return strncmp(a->data.user.string, b->data.user.string, TOKEN_MAX);
	case STRUCT_TYPE:
		return struct_cmp(a->data.structure, b->data.structure);
	case NONE_TYPE:
	default:
	}
	return 1;
}



uint8_t struct_cmp(structure_ast* const a, structure_ast* const b){
	if (a->union_c != b->union_c || a->binding_c != b->binding_c){
		return 1;
	}
	for (uint32_t i = 0;i<a->binding_c;++i){
		if ((type_cmp(&a->binding_v[i].type, &b->binding_v[i].type, FOR_APPLICATION) != 0)
		 || (strncmp(a->binding_v[i].name.string, b->binding_v[i].name.string, TOKEN_MAX) != 0)){
			return 1;
		}
	}
	for (uint32_t i = 0;i<a->union_c;++i){
		if ((strncmp(a->tag_v[i].string, b->tag_v[i].string, TOKEN_MAX) != 0)
		 || (struct_cmp(&a->union_v[i], &b->union_v[i]) != 0)){
			return 1;
		}
	}
	return 0;
}

type_ast* scope_contains(scope* const roll, binding_ast* const binding, uint8_t* needs_capturing){
	for (uint16_t i = 0;i<roll->binding_count;++i){
		uint16_t index = roll->binding_count - (i+1);
		if (strcmp(roll->binding_stack[index].name.string, binding->name.string) == 0){
			if ((needs_capturing != NULL)
			 && (index < roll->captures->binding_count_point)
			 && (index >= roll->builtin_stack_frame)){
				*needs_capturing = 1;
			}
			return &roll->binding_stack[index].type;
		}
	}
	return NULL;
}

type_ast roll_statement_expression(
	scope* const roll,
	ast* const tree,
	pool* const mem,
	statement_ast* const statement,
	type_ast expected_type,
	uint8_t as_expression,
	char* err
){
	if (statement->type.tag != NONE_TYPE){
		return statement->type;
	}
	switch (statement->tag){

	case IF_STATEMENT:
		if (as_expression == 0){
			type_ast temp = expected_type;
			expected_type.tag = PROCEDURE_TYPE;
			expected_type.data.pointer = pool_request(mem, sizeof(type_ast));
			*expected_type.data.pointer = temp;
		}
		type_ast pred = {
			.tag=PRIMITIVE_TYPE,
			.data.primitive=INT_ANY,
			.mut=0
		};
		roll_expression(roll, tree, mem, statement->data.if_statement.predicate, pred, 0, NULL, 0, err);
		if (*err != 0){
			*err = 0;
			pred.mut = 1;
			roll_expression(roll, tree, mem, statement->data.if_statement.predicate, pred, 0, NULL, 0, err);
			if (*err != 0){
				snprintf(err, ERROR_BUFFER, " [!] If statement predicate needs to return integral type\n");
				return pred;
			}
		}
		type_ast btype = roll_expression(roll, tree, mem, statement->data.if_statement.branch, expected_type, 0, NULL, 0, err);
		if (*err != 0){
			return expected_type;
		}
		if (statement->data.if_statement.alternate == NULL){
			if (as_expression == 0){
				return expected_type;
			}
			snprintf(err, ERROR_BUFFER, " [!] Expression conditional statement must have alternate branch\n");
			return expected_type;
		}
		type_ast atype = roll_expression(roll, tree, mem, statement->data.if_statement.alternate, expected_type, 0, NULL, 0, err);
		if (*err != 0){
			return expected_type;
		}
		if (type_cmp(&btype, &atype, FOR_EQUALITY) != 0){
			snprintf(err, ERROR_BUFFER, " [!] branch and alternate types in conditional statement expression do not match\n");
			return expected_type;
		}
		statement->type = btype;
		return btype;

	default:
		snprintf(err, ERROR_BUFFER, " [!] Unknown statement type %u\n", statement->tag);
	}
	return expected_type;
}

type_ast roll_literal_expression(
	scope* const roll,
	ast* const tree,
	pool* const mem,
	literal_ast* const lit,
	type_ast expected_type,
	char* err
){
	if (lit->type.tag != NONE_TYPE){
		return lit->type;
	}
	type_ast inner;
	switch(lit->tag){

	case STRING_LITERAL:
		if (expected_type.tag == NONE_TYPE){
			inner = (type_ast){
				.tag=POINTER_TYPE,
				.data.pointer=pool_request(mem, sizeof(type_ast))
			};
			*inner.data.pointer = (type_ast){
				.tag=PRIMITIVE_TYPE,
				.data.primitive=I8_TYPE
			};
			lit->type = inner;
			return inner;
		}
		type_ast inner_type = expected_type;
		if (expected_type.tag == USER_TYPE){
			inner_type = resolve_type_or_alias(tree, expected_type, err);
			if (*err != 0){
				return expected_type;
			}
		}
		if (inner_type.tag == POINTER_TYPE){
			inner = *inner_type.data.pointer;
		}
		else if (inner_type.tag == BUFFER_TYPE){
			inner = *inner_type.data.buffer.base;
		}
		else{
			snprintf(err, ERROR_BUFFER, " [!] String literal used where pointer or buffer was not expected\n");
			return expected_type;
		}
		if (inner.tag != PRIMITIVE_TYPE || inner.data.primitive != I8_TYPE){
			snprintf(err, ERROR_BUFFER, " [!] Expected (i8 *) for string literal\n");
			return expected_type;
		}
		lit->type = expected_type;
		return expected_type;

	case ARRAY_LITERAL:
		if (expected_type.tag == NONE_TYPE){
			if (lit->data.array.member_c == 0){
				snprintf(err, ERROR_BUFFER, " [!] Cannot infer type of empty buffer\n");
				return expected_type;
			}
			uint32_t index = 0;
			expression_ast* expr = &lit->data.array.member_v[index];
			inner = roll_expression(roll, tree, mem, expr, expected_type, 0, NULL, 0, err);
			if (*err != 0){
				return expected_type;
			}
			if (inner.tag == PROCEDURE_TYPE){
				snprintf(err, ERROR_BUFFER, " [!] Procedures cannot be stored in data structures\n");
				return expected_type;
			}
			index += 1;
			for (;index<lit->data.array.member_c;++index){
				expr = &lit->data.array.member_v[index];
				roll_expression(roll, tree, mem, expr, inner, 0, NULL, 0, err);
				if (*err != 0){
					return inner;
				}
			}
			lit->type = (type_ast){
				.tag=POINTER_TYPE,
				.data.pointer=pool_request(mem, sizeof(type_ast))
			};
			*lit->type.data.pointer = inner;
			return lit->type;
		}
		type_ast container = expected_type;
		if (expected_type.tag == USER_TYPE){
			container = resolve_type_or_alias(tree, expected_type, err);
			if (*err != 0){
				return expected_type;
			}
		}
		if (container.tag == POINTER_TYPE){
			inner = *container.data.pointer;
		}
		else if (container.tag == BUFFER_TYPE){
			inner = *container.data.buffer.base;
		}
		else{
			snprintf(err, ERROR_BUFFER, " [!] Array literal used where pointer or buffer was not expected\n");
			return expected_type;
		}
		if (inner.tag == PROCEDURE_TYPE){
			snprintf(err, ERROR_BUFFER, " [!] Procedures cannot be stored in data structures\n");
			return expected_type;
		}
		for (uint32_t i = 0;i<lit->data.array.member_c;++i){
			expression_ast* expr = &lit->data.array.member_v[i];
			roll_expression(roll, tree, mem, expr, inner, 0, NULL, 0, err);
			if (*err != 0){
				return expected_type;
			}
		}
		lit->type = expected_type;
		return expected_type;

	case STRUCT_LITERAL:
		if (expected_type.tag == NONE_TYPE){
			snprintf(err, ERROR_BUFFER, " [!] Cannot typecheck anonymous structure with no indication about its potential type to match against (for now? anonymous bindings in the future?)\n");
			return expected_type;
		}
		structure_ast target_struct;
		if (expected_type.tag == USER_TYPE){
			type_ast inner_struct_type = resolve_type_or_alias(tree, expected_type, err);
			if (*err != 0){
				return expected_type;
			}
			if (inner_struct_type.tag != STRUCT_TYPE){
				snprintf(err, ERROR_BUFFER, " [!] Struct literal cannot be created for non struct type '%s'\n", expected_type.data.user.string);
				return expected_type;
			}
			target_struct = *inner_struct_type.data.structure;
		}
		else if (expected_type.tag == STRUCT_TYPE){
			target_struct = *expected_type.data.structure;
		}
		else{
			snprintf(err, ERROR_BUFFER, " [!] Struct literal cannot be created for type which is not user defined struct, or anonymous struct\n");
			return expected_type;
		}
		type_ast infer = {
			.tag=NONE_TYPE
		};
		structure_ast nest[MAX_STRUCT_NESTING];
		uint32_t stack[MAX_STRUCT_NESTING];
		uint32_t nest_level = 0;
		uint32_t data_member = 0;
		for (uint32_t index = 0;index<lit->data.array.member_c;++index){
			expression_ast* term = &lit->data.array.member_v[index];
			if ((term->tag == APPLICATION_EXPRESSION)
			 && (term->data.block.expr_c == 1)
			 && (term->data.block.expr_v[0].tag == BINDING_EXPRESSION)){
				uint8_t skip = 0;
				for (uint32_t union_index = 0;union_index<target_struct.union_c;++union_index){
					if (strncmp(term->data.block.expr_v[0].data.binding.name.string, target_struct.tag_v[union_index].string, TOKEN_MAX) == 0){
						nest[nest_level] = target_struct;
						target_struct = target_struct.union_v[union_index];
						stack[nest_level] = data_member;
						data_member = 0;
						nest_level += 1;
						skip = 1;
						break;
					}
				}
				if (skip == 1){
					continue;
				}
			}
			if (data_member >= target_struct.binding_c){
				if (nest_level == 0){
					snprintf(err, ERROR_BUFFER, " [!] Struct literal of expected type required at most %u members but too many were given\n", target_struct.binding_c);
					return infer;
				}
				nest_level -= 1;
				data_member = stack[nest_level];
				target_struct = nest[nest_level];
			}
			type_ast member_deduced_type = roll_expression(roll, tree, mem, term, target_struct.binding_v[data_member].type, 0, NULL, 0, err);
			if (*err != 0){
				return infer;
			}
			if (member_deduced_type.tag == PROCEDURE_TYPE){
				snprintf(err, ERROR_BUFFER, " [!] Procedures cannot be stored in data structures\n");
				return expected_type;
			}
			data_member += 1;
		}
		lit->type = expected_type;
		return expected_type;

	default:
		snprintf(err, ERROR_BUFFER, " [!] Unknown literal expression type %u\n", lit->tag);
	}
	return expected_type;
}

int compile_from_file(char* filename){
	FILE* fd = fopen(filename, "r");
	if (fd == NULL){
		fprintf(stderr, "File not found: %s\n", filename);
		return 1;
	}
	pool mem = pool_alloc(POOL_SIZE, POOL_STATIC);
	printf("%lu bytes left\n", mem.left);
	char err[ERROR_BUFFER] = "\0";
	ast tree = parse(fd, &mem, err);
	if (err[0] != '\0'){
		fprintf(stderr, "Could not compile from source file: %s\n", filename);
		fprintf(stderr, err);
		show_ast(&tree);
		fclose(fd);
		pool_dealloc(&mem);
		return 1;
	}
	show_ast(&tree);
	printf("Parsed\n");
	printf("%lu bytes left\n", mem.left);
	fclose(fd);
	scope roll = {
		.binding_stack = pool_request(&mem, MAX_STACK_MEMBERS*sizeof(binding_ast)),
		.frame_stack = pool_request(&mem, MAX_STACK_MEMBERS*sizeof(uint16_t)),
		.binding_count=0,
		.binding_capacity=MAX_STACK_MEMBERS,
		.frame_count=0,
		.frame_capacity=MAX_STACK_MEMBERS,
		.captures=pool_request(&mem, sizeof(capture_stack)),
		.capture_frame=0
	};
	*roll.captures = (capture_stack){
		.prev=NULL,
		.next=NULL,
		.size=0,
		.binding_count_point=0
	};
	push_builtins(&roll, &mem);
	roll.builtin_stack_frame = roll.binding_count;
	transform_ast(&roll, &tree, &mem, err);
	if (*err != 0){
		fprintf(stderr, "Could not compile from source file: %s\n", filename);
		fprintf(stderr, err);
		show_ast(&tree);
		pool_dealloc(&mem);
		return 1;
	}
	show_ast(&tree);
	printf("Compiled\n");
	printf("%lu bytes left\n", mem.left);
	pool_dealloc(&mem);
	return 0;
}

void binary_int_builtin(scope* const roll, pool* const mem, token name){
	binding_ast builtin = {
		.name=name,
		.type={.tag=FUNCTION_TYPE}
	};
	builtin.type.data.function.left = pool_request(mem, sizeof(type_ast));
	builtin.type.data.function.right = pool_request(mem, sizeof(type_ast));
	*builtin.type.data.function.left = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	*builtin.type.data.function.right = (type_ast){.tag=FUNCTION_TYPE};
	builtin.type.data.function.right->data.function.left = pool_request(mem, sizeof(type_ast));
	builtin.type.data.function.right->data.function.right = pool_request(mem, sizeof(type_ast));
	*builtin.type.data.function.right->data.function.left = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	*builtin.type.data.function.right->data.function.right = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	push_binding(roll, builtin);
}

void binary_float_builtin(scope* const roll, pool* const mem, token name){
	binding_ast builtin = {
		.name=name,
		.type={.tag=FUNCTION_TYPE}
	};
	builtin.type.data.function.left = pool_request(mem, sizeof(type_ast));
	builtin.type.data.function.right = pool_request(mem, sizeof(type_ast));
	*builtin.type.data.function.left = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=FLOAT_ANY};
	*builtin.type.data.function.right = (type_ast){.tag=FUNCTION_TYPE};
	builtin.type.data.function.right->data.function.left = pool_request(mem, sizeof(type_ast));
	builtin.type.data.function.right->data.function.right = pool_request(mem, sizeof(type_ast));
	*builtin.type.data.function.right->data.function.left = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=FLOAT_ANY};
	*builtin.type.data.function.right->data.function.right = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=FLOAT_ANY};
	push_binding(roll, builtin);
}

void unary_int_builtin(scope* const roll, pool* const mem, token name){
	binding_ast builtin = {
		.name=name,
		.type={.tag=FUNCTION_TYPE}
	};
	builtin.type.data.function.left = pool_request(mem, sizeof(type_ast));
	builtin.type.data.function.right = pool_request(mem, sizeof(type_ast));
	*builtin.type.data.function.left = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	*builtin.type.data.function.right = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	push_binding(roll, builtin);
}

void push_builtins(scope* const roll, pool* const mem){
	binary_int_builtin(roll, mem, (token){ .len=1, .string="+", .type=TOKEN_ADD });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="-", .type=TOKEN_SUB });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="/", .type=TOKEN_DIV });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="*", .type=TOKEN_MUL });
	binary_int_builtin(roll, mem, (token){ .len=1, .string=".+", .type=TOKEN_FLADD });
	binary_int_builtin(roll, mem, (token){ .len=1, .string=".-", .type=TOKEN_FLSUB });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="./", .type=TOKEN_FLDIV });
	binary_int_builtin(roll, mem, (token){ .len=1, .string=".*", .type=TOKEN_FLMUL });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="%", .type=TOKEN_MOD });
	binary_int_builtin(roll, mem, (token){ .len=2, .string="<<", .type=TOKEN_SHL });
	binary_int_builtin(roll, mem, (token){ .len=2, .string=">>", .type=TOKEN_SHR });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="<", .type=TOKEN_ANGLE_OPEN });
	binary_int_builtin(roll, mem, (token){ .len=1, .string=">", .type=TOKEN_ANGLE_CLOSE });
	binary_int_builtin(roll, mem, (token){ .len=2, .string="<=", .type=TOKEN_LESS_EQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string=">=", .type=TOKEN_GREATER_EQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string="==", .type=TOKEN_EQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string="!=", .type=TOKEN_NOT_EQ });
	binary_int_builtin(roll, mem, (token){ .len=1, .string=".<", .type=TOKEN_FLANGLE_OPEN });
	binary_int_builtin(roll, mem, (token){ .len=1, .string=".>", .type=TOKEN_FLANGLE_CLOSE });
	binary_int_builtin(roll, mem, (token){ .len=2, .string=".<=", .type=TOKEN_FLLESS_EQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string=".>=", .type=TOKEN_FLGREATER_EQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string=".==", .type=TOKEN_FLEQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string=".!=", .type=TOKEN_FLNOT_EQ });
	binary_int_builtin(roll, mem, (token){ .len=2, .string="&&", .type=TOKEN_BOOL_AND });
	binary_int_builtin(roll, mem, (token){ .len=2, .string="||", .type=TOKEN_BOOL_OR });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="&", .type=TOKEN_BIT_AND });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="|", .type=TOKEN_BIT_OR });
	binary_int_builtin(roll, mem, (token){ .len=1, .string="^", .type=TOKEN_BIT_XOR });
	unary_int_builtin(roll, mem, (token){ .len=1, .string="~", .type=TOKEN_BIT_COMP });
	unary_int_builtin(roll, mem, (token){ .len=1, .string="!", .type=TOKEN_BOOL_NOT });
	//alloc builtin
	binding_ast alloc = {
		.name.len=strlen("alloc"),
		.name.string="alloc",
		.name.type=TOKEN_IDENTIFIER,
		.type={.tag=FUNCTION_TYPE}
	};
	alloc.type.data.function.left = pool_request(mem, sizeof(type_ast));
	alloc.type.data.function.right = pool_request(mem, sizeof(type_ast));
	*alloc.type.data.function.left = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	*alloc.type.data.function.right = (type_ast){.tag=POINTER_TYPE, .data.pointer=pool_request(mem, sizeof(type_ast))};
	type_ast bytes = {
		.tag=PRIMITIVE_TYPE,
		.data.primitive=U8_TYPE
	};
	*alloc.type.data.function.right->data.pointer = bytes;
	push_binding(roll, alloc);
	//free builtin
	binding_ast dealloc = {
		.name.len=strlen("free"),
		.name.string="free",
		.name.type=TOKEN_IDENTIFIER,
		.type={.tag=FUNCTION_TYPE}
	};
	dealloc.type.data.function.left = pool_request(mem, sizeof(type_ast));
	dealloc.type.data.function.right = pool_request(mem, sizeof(type_ast));
	*dealloc.type.data.function.left = (type_ast){.tag=POINTER_TYPE, .data.pointer=pool_request(mem, sizeof(type_ast))};
	*dealloc.type.data.function.right = (type_ast){.tag=PRIMITIVE_TYPE, .data.primitive=INT_ANY};
	bytes.tag=INTERNAL_ANY_TYPE;
	*dealloc.type.data.function.left->data.pointer = bytes;
	push_binding(roll, dealloc);
}

void show_token(const token* const tok){
	printf("%.*s ", TOKEN_MAX, tok->string);
	fflush(stdout);
}

void show_ast(const ast* const tree){
	for (size_t i = 0;i<tree->import_c;++i){
		printf("\033[1;34mimport\033[0m ");
		show_token(&tree->import_v[i]);
		printf("\n");
	}
	printf("\n");
	for (size_t i = 0;i<tree->const_c;++i){
		printf("\033[1;34mconstant\033[0m ");
		show_constant(&tree->const_v[i]);
		printf("\n");
	}
	printf("\n");
	for (size_t i = 0;i<tree->new_type_c;++i){
		printf("\033[1;33mtype\033[0m ");
		show_new_type(&tree->new_type_v[i]);
		printf("\n");
	}
	printf("\n");
	for (size_t i = 0;i<tree->alias_c;++i){
		printf("\033[1;33malias\033[0m ");
		show_alias(&tree->alias_v[i]);
		printf("\n");
	}
	printf("\n");
	for (size_t i = 0;i<tree->func_c;++i){
		printf("\033[1;31mfunction \033[0m");
		show_function(&tree->func_v[i]);
		printf("\n");
		printf("\n");
	}
	printf("\n");
}

void indent_n(uint8_t n){
	for (uint8_t i = 0;i<n;++i){
		printf("  ");
	}
}

void show_structure(const structure_ast* const structure, uint8_t indent){
	for (size_t i = 0;i<structure->binding_c;++i){
		show_binding(&structure->binding_v[i]);
	}
	for (size_t i = 0;i<structure->union_c;++i){
		indent_n(indent);
		show_token(&structure->tag_v[i]);
		printf("{ ");
		show_structure(&structure->union_v[i], indent+1);
		printf("} ");
	}
}

void show_lambda(const lambda_ast* const lambda){
	for (size_t i = 0;i<lambda->argc;++i){
		show_token(&lambda->argv[i]);
	}
	printf("<- ");
	show_expression(lambda->expression, 1);
}

void show_type(const type_ast* const type){
	switch (type->tag){
	case FUNCTION_TYPE:
		printf("\033[1;36m( \033[0m");
		show_type(type->data.function.left);
		printf("\033[1;36m-> \033[0m");
		show_type(type->data.function.right);
		printf("\033[1;36m) \033[0m");
		break;
	case BUFFER_TYPE:
		printf("\033[1;36m(%u of \033[0m", type->data.buffer.count);
		show_type(type->data.buffer.base);
		printf("\033[1;36m) \033[0m");
		break;
	case PROCEDURE_TYPE:
		printf("\033[1;36m procedure \033[0m");
		show_type(type->data.pointer);
		break;
	case POINTER_TYPE:
		show_type(type->data.pointer);
		printf("\033[1;36m* \033[0m");
		break;
	case USER_TYPE:
		printf("\033[1;36m");
		show_token(&type->data.user);
		printf("\033[0m");
		break;
	case STRUCT_TYPE:
		printf("\033[1;36m{ \033[0m");
		show_structure(type->data.structure, 1);
		printf("\033[1;36m} \033[0m");
		break;
	case NONE_TYPE:
		printf("\033[1;36mNON \033[0m");
		break;
	case PRIMITIVE_TYPE:
		printf("\033[1;36m");
		switch(type->data.primitive){
		case U8_TYPE:
			printf("U8 ");
			break;
		case U16_TYPE:
			printf("U16 ");
			break;
		case U32_TYPE:
			printf("U32 ");
			break;
		case U64_TYPE:
			printf("U64 ");
			break;
		case I8_TYPE:
			printf("I8 ");
			break;
		case I16_TYPE:
			printf("I16 ");
			break;
		case I32_TYPE:
			printf("I32 ");
			break;
		case I64_TYPE:
			printf("I64 ");
			break;
		case INT_ANY:
			printf("INT ");
			break;
		case F32_TYPE:
			printf("F32 ");
			break;
		case F64_TYPE:
			printf("F64 ");
			break;
		case FLOAT_ANY:
			printf("FLT ");
			break;
		}
		printf("\033[0m");
		break;
	default:
		printf("\033[1;36mUNK \033[0m");
		break;
	}
	if (type->mut != 0){
		printf("\033[1;36mVAR \033[0m");
	}
}

void show_binding(const binding_ast* const binding){
	show_token(&binding->name);
}

void show_statement(const statement_ast* const statement, uint8_t indent){
	switch(statement->tag){
	case IF_STATEMENT:
		printf("\033[1;31mif \033[0m");
		show_expression(statement->data.if_statement.predicate, indent);
		printf("\033[1;31mthen \033[0m");
		show_expression(statement->data.if_statement.branch, indent+1);
		if (statement->data.if_statement.alternate != NULL){
			printf("\033[1;31melse \033[0m");
			show_expression(statement->data.if_statement.alternate, indent+1);
		}
		break;
	default:
		printf("<UNKNOWN STATEMENT> ");
		break;
	}
}

void show_literal(const literal_ast* const lit){
	switch (lit->tag){
	case STRING_LITERAL:
		printf("\"%s\" ", lit->data.string.content);
		break;
	case ARRAY_LITERAL:
		printf("[ ");
		for (size_t i = 0;i<lit->data.array.member_c;++i){
			show_expression(&lit->data.array.member_v[i], 1);
			printf(", ");
		}
		printf("] ");
		break;
	case STRUCT_LITERAL:
		printf("{ ");
		for (size_t i = 0;i<lit->data.array.member_c;++i){
			show_expression(&lit->data.array.member_v[i], 1);
			printf(", ");
		}
		printf("} ");
		break;
	}
}

void show_expression(const expression_ast* const expr, uint8_t indent){
	size_t i;
	switch(expr->tag){
	case BLOCK_EXPRESSION:
		show_type(&expr->data.block.type);
		printf("{\n ");
		for (i = 0;i<expr->data.block.expr_c;++i){
			show_expression(&expr->data.block.expr_v[i], indent+1);
			printf("\n");
		}
		printf("} ");
		break;
	case CLOSURE_EXPRESSION:
		printf("CAPTURING< ");
		for (i = 0;i<expr->data.closure.capture_c;++i){
			show_binding(&expr->data.closure.capture_v[i]);
		}
		printf("> ");
		show_function(expr->data.closure.func);
		break;
	case APPLICATION_EXPRESSION:
		show_type(&expr->data.block.type);
		printf("( ");
		for (i = 0;i<expr->data.block.expr_c;++i){
			show_expression(&expr->data.block.expr_v[i], indent);
		}
		printf(") ");
		break;
	case STATEMENT_EXPRESSION:
		show_statement(&expr->data.statement, indent);
		break;
	case BINDING_EXPRESSION:
		if (expr->data.binding.name.type==TOKEN_RETURN){
			printf("\033[1;31m");
			show_binding(&expr->data.binding);
			printf("\033[0m");
			break;
		}
		show_binding(&expr->data.binding);
		break;
	case VALUE_EXPRESSION:
		printf("\033[1;35m");
		show_binding(&expr->data.binding);
		printf("\033[0m");
		break;
	case DEREF_EXPRESSION:
		printf("[ ");
		show_expression(expr->data.deref, indent);
		printf("] ");
		break;
	case ACCESS_EXPRESSION:
		printf("{ ");
		show_expression(expr->data.deref, indent);
		printf("} ");
		break;
	case LAMBDA_EXPRESSION:
		printf("\\ ");
		show_lambda(&expr->data.lambda);
		break;
	case LITERAL_EXPRESSION:
		show_literal(&expr->data.literal);
		break;
	case RETURN_EXPRESSION:
		printf("\033[1;31mreturn \033[0m");
		show_expression(expr->data.deref, indent);
		break;
	case REF_EXPRESSION:
		printf("\033[1;31mptr \033[0m");
		show_expression(expr->data.deref, indent);
		break;
	case CAST_EXPRESSION:
		show_expression(expr->data.cast.target, indent);
		printf("\033[1;31mcast as (pointer to) \033[0m");
		show_type(&expr->data.cast.type);
		break;
	case SIZEOF_EXPRESSION:
		printf("\033[1;31m size of \033[0m");
		if (expr->data.size_of.target != NULL){
			show_expression(expr->data.size_of.target, indent);
		}
		else{
			show_type(&expr->data.size_of.type);
		}
		printf("\033[1;31m(%lu bytes) \033[0m", expr->data.size_of.size);
		break;
	default:
		printf("UNKNOWN EXPRESSION ");
		break;
	}
}

void show_new_type(const new_type_ast* const new_type){
	show_type(&new_type->type);
	show_token(&new_type->name);
	printf("\n");
}

void show_constant(const constant_ast* const cnst){
	show_token(&cnst->name);
	printf(": ");
	show_binding(&cnst->value);
	printf("\n");
}

void show_alias(const alias_ast* const alias){
	show_type(&alias->type);
	show_token(&alias->name);
	printf("\n");
}

void show_function(const function_ast* const func){
	show_type(&func->type);
	show_token(&func->name);
	if (func->enclosing){
		printf("# ");
	}
	else{
		printf("= ");
	}
	show_expression(&func->expression, 1);
}

int main(int argc, char** argv){
	compile_from_file("correct.ka");
	return 0;
	if (argc < 2){
		fprintf(stderr, "No arguments provided, use -h or -help for a list of options\n\n");
		return 1;
	}
	if (strncmp(argv[1], "-h", TOKEN_MAX) == 0 || strncmp(argv[1], "-help", TOKEN_MAX) == 0){
		printf("-h, -help    :  Display this list\n");
		printf("\n");
		return 0;
	}
	compile_from_file(argv[1]);
	return 0;
}
