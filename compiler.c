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
MAP_IMPL(TOKEN_TYPE_TAG)

uint8_t
issymbol(char c){
	return (c > 32 && c < 48)
		|| (c > 122 && c < 127)
		|| (c > 90 && c < 97)
		|| (c > 57 && c < 65);
}

ast
parse(token* const tokens, pool* const mem, uint64_t token_count, char* string_content_buffer, char* err){
	lexer lex = {
		.tokens=tokens,
		.token_count=token_count,
		.index=0
	};
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
		.lifted_lambdas=0,
		.string_buffer=string_content_buffer
	};
	tree.import_v = pool_request(mem, sizeof(token)*MAX_IMPORTS);
	tree.func_v = pool_request(mem, sizeof(function_ast)*MAX_FUNCTIONS);
	tree.new_type_v = pool_request(mem, sizeof(new_type_ast)*MAX_ALIASES);
	tree.alias_v = pool_request(mem, sizeof(alias_ast)*MAX_ALIASES);
	tree.const_v = pool_request(mem, sizeof(constant_ast)*MAX_ALIASES);
	add_to_tree(&tree, &lex, mem, err);
	return tree;
}

void
add_to_tree(ast* const tree, lexer* const lex, pool* const mem, char* err){
	token tok;
	// parse imports
	for (;lex->index < lex->token_count;++lex->index){
		tok = lex->tokens[lex->index];
		if (tok.type != TOKEN_IMPORT){
			break;
		}
		if (tree->import_c >= MAX_IMPORTS){
			snprintf(err, ERROR_BUFFER, "too many imports\n");
			return;
		}
		parse_import(tree, lex, mem, err);
		if (*err != 0){
			return;
		}
	}
	if (tok.type == TOKEN_EOF){
		return;
	}
	// parse actual code
	for (;lex->index < lex->token_count;tok=lex->tokens[++lex->index]){
		if (tok.type == TOKEN_EOF){
			return;
		}
		if (tok.type == TOKEN_TYPE){
			new_type_ast a = parse_new_type(lex, mem, err);
			if (*err != 0){
				return;
			}
			tree->new_type_v[tree->new_type_c] = a;
			uint8_t collision = new_type_ast_map_insert(&tree->types, tree->new_type_v[tree->new_type_c].name.string, &tree->new_type_v[tree->new_type_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined multiple times\n", tree->new_type_v[tree->new_type_c].name.string);
			}
			else if (function_ast_map_access(&tree->functions, tree->new_type_v[tree->new_type_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined prior as function\n", tree->new_type_v[tree->new_type_c].name.string);
			}
			else if (alias_ast_map_access(&tree->aliases, tree->new_type_v[tree->new_type_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined prior as alias\n", tree->new_type_v[tree->new_type_c].name.string);
			}
			else if (constant_ast_map_access(&tree->constants, tree->new_type_v[tree->new_type_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Type '%s' defined prior as constant\n", tree->new_type_v[tree->new_type_c].name.string);
			}
			tree->new_type_c += 1;
		}
		else if (tok.type == TOKEN_ALIAS){
			alias_ast a = parse_alias(lex, mem, err);
			if (*err != 0){
				return;
			}
			tree->alias_v[tree->alias_c] = a;
			uint8_t collision = alias_ast_map_insert(&tree->aliases, tree->alias_v[tree->alias_c].name.string, &tree->alias_v[tree->alias_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined multiple times\n", tree->alias_v[tree->alias_c].name.string);
			}
			else if (function_ast_map_access(&tree->functions, tree->alias_v[tree->alias_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined prior as function\n", tree->alias_v[tree->alias_c].name.string);
			}
			else if (new_type_ast_map_access(&tree->types, tree->alias_v[tree->alias_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined prior as type\n", tree->alias_v[tree->alias_c].name.string);
			}
			else if (constant_ast_map_access(&tree->constants, tree->alias_v[tree->alias_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Alias '%s' defined prior as constant\n", tree->alias_v[tree->alias_c].name.string);
			}
			tree->alias_c += 1;
		}
		else if (tok.type == TOKEN_CONST){
			constant_ast cnst = parse_constant(lex, mem, err);
			if (*err != 0){
				return;
			}
			tree->const_v[tree->const_c] = cnst;
			uint8_t collision = constant_ast_map_insert(&tree->constants, tree->const_v[tree->const_c].name.string, &tree->const_v[tree->const_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' was defined multiple times\n", tree->const_v[tree->const_c].name.string);
			}
			else if (function_ast_map_access(&tree->functions, tree->const_v[tree->const_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' defined prior as function\n", tree->const_v[tree->const_c].name.string);
			}
			else if (new_type_ast_map_access(&tree->types, tree->const_v[tree->const_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' defined prior as type\n", tree->const_v[tree->const_c].name.string);
			}
			else if (alias_ast_map_access(&tree->aliases, tree->const_v[tree->const_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Constant '%s' defined prior as alias\n", tree->const_v[tree->const_c].name.string);
			}
			tree->const_c += 1;
		}
		else{
			function_ast f = parse_function(lex, mem, err, 0);
			if (*err != 0){
				return;
			}
			tree->func_v[tree->func_c] = f;
			uint8_t collision = function_ast_map_insert(&tree->functions, tree->func_v[tree->func_c].name.string, &tree->func_v[tree->func_c]);
			if (collision == 1){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined multiple times\n", tree->func_v[tree->func_c].name.string);
			}
			else if (new_type_ast_map_access(&tree->types, tree->func_v[tree->func_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined prior as type\n", tree->func_v[tree->func_c].name.string);
			}
			else if (alias_ast_map_access(&tree->aliases, tree->func_v[tree->func_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined prior as alias\n", tree->func_v[tree->func_c].name.string);
			}
			else if (constant_ast_map_access(&tree->constants, tree->func_v[tree->func_c].name.string) != NULL){
				snprintf(err, ERROR_BUFFER, " <!> Function '%s' defined prior as constant\n", tree->func_v[tree->func_c].name.string);
			}
			tree->func_c += 1;
		}
	}
}

void
parse_import(ast* const tree, lexer* const lex, pool* const mem, char* err){
	token filename = lex->tokens[++lex->index];
	if (filename.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error : Tried to import non identifier: '%s'\n", filename.string);
		return;
	}
	token semi = lex->tokens[++lex->index];
	if (semi.type != TOKEN_SEMI){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected ; after import %s, found '%s'\n", filename.string, semi.string);
		return;
	}
	if (already_imported(tree, filename) == 1){
		return;
	}
	tree->import_v[tree->import_c] = filename;
	tree->import_c += 1;
	char file_cstr[TOKEN_MAX+3];
	strncpy(file_cstr, filename.string, filename.len);
	strcat(file_cstr, ".ka");
	FILE* fd = fopen(file_cstr, "r");
	if (fd == NULL){
		snprintf(err, ERROR_BUFFER, "Could not find module with name '%s'\n", file_cstr);
		return;
	}
	uint64_t read_bytes = fread(mem->buffer, sizeof(char), READ_BUFFER_SIZE, fd);
	fclose(fd);
	if (read_bytes == READ_BUFFER_SIZE){
		snprintf(err, ERROR_BUFFER, "file larger than allowed read buffer\n");
		return;
	}
	uint64_t token_count = 0;
	token* tokens = lex_cstr(mem->buffer, read_bytes, mem, &token_count, &tree->string_buffer, err);
	if (*err != 0){
		return;
	}
	lexer nested_lex = {
		.tokens=tokens,
		.token_count = token_count,
		.index=0
	};
	add_to_tree(tree, &nested_lex, mem, err);
}

uint8_t
already_imported(ast* const tree, token filename){
	for (uint32_t i = 0;i<tree->import_c;++i){
		if (strncmp(tree->import_v[i].string, filename.string, TOKEN_MAX) == 0){
			return 1;
		}
	}
	return 0;
}

structure_ast
parse_struct(lexer* const lex, pool* const mem, char* err){
	structure_ast outer = {
		.binding_v=pool_request(mem, sizeof(binding_ast)*MAX_MEMBERS),
		.union_v=NULL,
		.encoding=NULL,
		.tag_v=NULL,
		.binding_c=0,
		.union_c=0
	};
	for (token tok = lex->tokens[++lex->index];
		 lex->index<lex->token_count;
		 tok=lex->tokens[++lex->index]
	){
		if (tok.type == TOKEN_BRACE_CLOSE){
			return outer;
		}
		uint64_t copy = parse_save(lex, mem);
		type_ast type = parse_type(lex, mem, err, TOKEN_IDENTIFIER, 0);
		if (*err != 0){
			parse_load(lex, mem, copy);
			*err = 0;
			if (tok.type != TOKEN_IDENTIFIER){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected identifier for enumerated union member or type for struct member, found '%s'\n", tok.string);
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
			token open = lex->tokens[++lex->index];
			switch(open.type){
			case TOKEN_SEMI:
				break;
			case TOKEN_BRACE_OPEN:
				structure_ast s = parse_struct(lex, mem, err);
				if (*err != 0){
					return outer;
				}
				outer.union_v[outer.union_c-1] = s;
				token semi = lex->tokens[++lex->index];
				if (semi.type == TOKEN_SEMI){
					break;
				}
				if (semi.type != TOKEN_SET){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected ';' to end union or '=' to set enumerated encoding, found '%s'\n", semi.string);
					return outer;
				}
			case TOKEN_SET:
				token encoding = lex->tokens[++lex->index];
				if (encoding.type != TOKEN_INTEGER){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected enumerator encoding, found '%s'\n", encoding.string);
					return outer;
				}
				token true_semi = lex->tokens[++lex->index];
				if (true_semi.type == TOKEN_SEMI){
					outer.encoding[outer.union_c-1] = atoi(encoding.string);
					break;
				}
			default:
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected '{', for union data or '=' for enum encoding, found '%s'\n", open.string);
				return outer;
			}
		}
		else{
			tok = lex->tokens[++lex->index];
			token semi = lex->tokens[++lex->index];
			if (tok.type != TOKEN_IDENTIFIER || semi.type != TOKEN_SEMI){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected 'identifier ;' for struct member name, found '%s' '%s'\n", tok.string, semi.string);
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
	snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : How did you get here in struct parse\n");
	return outer;
}

type_ast
parse_type(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t consume){
	token name = lex->tokens[lex->index];
	type_ast outer = (type_ast){
		.tag=USER_TYPE,
		.data.user=name,
		.mut=0
	};
	switch (name.type){
	case TOKEN_BRACK_OPEN:
		lex->index += 1;
		outer = parse_type(lex, mem, err, TOKEN_BRACK_CLOSE, 1);
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
		lex->index += 1;
		outer = parse_type(lex, mem, err, TOKEN_PAREN_CLOSE, 1);
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
		lex->index += 1;
		type_ast procedure_optional = parse_type(lex, mem, err, end_token, 0);
		if (*err != 0){
			return outer;
		}
		outer.data.pointer = pool_request(mem, sizeof(type_ast));
		*outer.data.pointer = procedure_optional;
		break;
	default:
		if (name.type!=TOKEN_IDENTIFIER){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected type name, found non identifier '%s'\n", name.string);
			return (type_ast){.tag=NONE_TYPE};
		}
		break;
	}
	while (lex->index < lex->token_count){
		uint64_t copy = parse_save(lex, mem);
		token tok = lex->tokens[++lex->index];
		if (tok.type == end_token || (end_token == TOKEN_IDENTIFIER && tok.type == TOKEN_SYMBOL)){
			if (consume == 0){
				parse_load(lex, mem, copy);
			}
			return outer;
		}
		switch(tok.type){
		case TOKEN_MUTABLE:
			if (outer.mut == 1){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error at : Type has two mutable tags\n");
				return outer;
			}
			outer.mut = 1;
			break;
		case TOKEN_IDENTIFIER:
			if (end_token != TOKEN_BRACK_CLOSE){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error at : Type given buffer size when not buffer or pointer\n");
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
				snprintf(err, ERROR_BUFFER, " <!> Parser Error at : Type given buffer size when not buffer or pointer\n");
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
			lex->index += 1;
			*outer.data.function.right = parse_type(lex, mem, err, end_token, consume);
			return outer;
		default:
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Unexpected token in or after type: '%s'\n", tok.string);
			return outer;
		}
	}
	snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Reached end of file while parsing type\n");
	return (type_ast){.tag=NONE_TYPE};
}

new_type_ast
parse_new_type(lexer* const lex, pool* const mem, char* err){
	token name = lex->tokens[++lex->index];
	if (name.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected identifier for type new_type name, found '%s'\n", name.string);
		return (new_type_ast){};
	}
	lex->index += 1;
	type_ast type = parse_type(lex, mem, err, TOKEN_SEMI, 1);
	if (*err != 0){
		return (new_type_ast){};
	}
	return (new_type_ast){
		.type=type,
		.name=name
	};
}

constant_ast
parse_constant(lexer* const lex, pool* const mem, char* err){
	token name = lex->tokens[++lex->index];
	constant_ast constant = {
		.value.type.tag=PRIMITIVE_TYPE,
		.value.type.data.primitive=INT_ANY
	};
	if (name.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected name for constant identifier\n");
		return constant;
	}
	constant.name=name;
	token eq = lex->tokens[++lex->index];
	if (eq.type != TOKEN_SET){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected token '=' to set constant value\n");
	}
	token val = lex->tokens[++lex->index];
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
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Constants can currently only evaluate to integers and floats, in the future this may just be turned into a compile time expression evaluation feature\n");
		return constant;
	}
	token semi = lex->tokens[++lex->index];
	if (semi.type != TOKEN_SEMI){
		snprintf(err, ERROR_BUFFER, " <!> Parsing ERror at : Constants definitions must end with token ';'\n");
		return constant;
	}
	return constant;
}

alias_ast
parse_alias(lexer* const lex, pool* const mem, char* err){
	token name = lex->tokens[++lex->index];
	if (name.type != TOKEN_IDENTIFIER){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected identifier for type new_type name, found '%s'\n", name.string);
		return (new_type_ast){};
	}
	lex->index += 1;
	type_ast type = parse_type(lex, mem, err,  TOKEN_SEMI, 1);
	if (*err != 0){
		return (new_type_ast){};
	}
	return (new_type_ast){
		.type=type,
		.name=name
	};
}

function_ast
parse_function(lexer* const lex, pool* const mem, char* err, uint8_t allowed_enclosing){
	type_ast type = parse_type(lex, mem, err, TOKEN_IDENTIFIER, 0);
	if (*err != 0){
		return (function_ast){
			.type=type
		};
	}
	token name = lex->tokens[++lex->index];
	if (name.type != TOKEN_IDENTIFIER && name.type != TOKEN_SYMBOL){
		snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected function name, found '%s'\n", name.string);
		return (function_ast){};
	}
	token tok = lex->tokens[++lex->index];
	uint8_t enclosing = 0;
	if (tok.type != TOKEN_SET){
		if (allowed_enclosing != 1 || tok.type != TOKEN_ENCLOSE){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected '=' to set function to value, found %s\n", tok.string);
			return (function_ast){};
		}
		enclosing = 1;
	}
	lex->index += 1;
	expression_ast expr = parse_application_expression(lex, mem, err, TOKEN_SEMI, 0, -1);
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

expression_ast
parse_lambda(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t* simple){
	expression_ast outer = {
		.tag=LAMBDA_EXPRESSION,
		.data.lambda={
			.argv=pool_request(mem, sizeof(token)*MAX_ARGS),
			.argc=0,
			.expression=NULL,
			.type.tag=NONE_TYPE
		}
	};
	for (token tok = lex->tokens[++lex->index];
		 lex->index<lex->token_count;
		 tok=lex->tokens[++lex->index]
	){
		expression_ast build;
		uint64_t copy;
		switch (tok.type){
		case TOKEN_IDENTIFIER:
			outer.data.lambda.argv[outer.data.lambda.argc] = tok;
			outer.data.lambda.argc += 1;
			if (outer.data.lambda.argc >= MAX_ARGS){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Too many args for arena allocated buffer\n");
				return outer;
			}
			break;
		case TOKEN_BRACK_OPEN:
			copy = parse_save(lex, mem);
			literal_ast lit = parse_array_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
				*outer.data.lambda.expression = build;
				return outer;
			}
			parse_load(lex, mem, copy);
			*err = 0;
			lex->index += 1;
			build = parse_application_expression(lex, mem, err, TOKEN_BRACK_CLOSE, 1, -1);
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
			copy = parse_save(lex, mem);
			lit = parse_struct_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
				*outer.data.lambda.expression = build;
				return outer;
			}
			parse_load(lex, mem, copy);
			*err = 0;
			lex->index += 1;
			build = parse_application_expression(lex, mem, err, TOKEN_BRACE_CLOSE, 1, -1);
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
			lex->index += 1;
			build = parse_application_expression(lex, mem, err, TOKEN_PAREN_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			outer.data.lambda.expression = pool_request(mem, sizeof(expression_ast));
			*outer.data.lambda.expression = build;
			return outer;
		default:
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Unexpected token provided as function argument: '%s'\n", tok.string);
			return outer;
		}
	}
	snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Reached unexpected case in lambda parsing\n");
	return outer;
}

function_ast
try_function(lexer* const lex, pool* const mem, char* err){
	token expr = lex->tokens[lex->index];
	function_ast func;
	switch(expr.type){
	case TOKEN_PROC:
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
		uint64_t copy = parse_save(lex, mem);
		func = parse_function(lex, mem, err, 1);
		if (*err == 0){
			return func;
		}
		parse_load(lex, mem, copy);
		return func;
	default:
		break;
	}
	*err = 1;
	return func;
}

expression_ast
unwrap_single_application(expression_ast single){
	while (single.tag == APPLICATION_EXPRESSION && single.data.block.expr_c == 1){
		single = single.data.block.expr_v[0];
	}
	return single;
}

expression_ast
parse_block_expression(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, expression_ast first){
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
	for (token tok = lex->tokens[++lex->index];
		 lex->index < lex->token_count;
		 tok = lex->tokens[++lex->index]
	){
		expression_ast build = {
			.tag=APPLICATION_EXPRESSION
		};
		if (tok.type == end_token){
			return outer;
		}
		build = parse_application_expression(lex, mem, err, TOKEN_SEMI, 2, -1);
		if (*err != 0){
			return outer;
		}
		build = unwrap_single_application(build);
		outer.data.block.expr_v[outer.data.block.expr_c] = build;
		outer.data.block.expr_c += 1;
	}
	snprintf(err, ERROR_BUFFER, " <!> Parser Error : how did you get to this condition in the block expression parser\n");
	return outer;
}

expression_ast
parse_application_expression(lexer* const lex, pool* const mem, char* err, TOKEN_TYPE_TAG end_token, uint8_t allow_block, int8_t limit){
	token expr = lex->tokens[lex->index];
	expression_ast outer = {
		.tag=APPLICATION_EXPRESSION,
		.data.block.type={.tag=NONE_TYPE},
		.data.block.expr_c=0
	};
	outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	if (allow_block != 0){
		function_ast func = try_function(lex, mem, err);
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
	uint64_t limit_copy;
	if (limit != -1){
		if (allow_block != 0){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Assertion error : blocks cannot be allowed when applications are limit requested\n");
			return outer;
		}
		limit_copy = parse_save(lex, mem);
		expr = lex->tokens[++lex->index];
	}
	LABEL_REQUEST label_req = LABEL_FULFILLED;
	LABEL_REQUEST jump_req = LABEL_FULFILLED;
	for (;lex->index < lex->token_count;expr=lex->tokens[++lex->index]){
		expression_ast build = {
			.tag=BINDING_EXPRESSION
		};
		literal_ast lit;
		if (expr.type == end_token){
			if (limit != -1){
				parse_load(lex, mem, limit_copy);
			}
			if (outer.data.block.expr_c == 0 && end_token == TOKEN_SEMI){
				lex->index += 1;
				return parse_application_expression(lex, mem, err, end_token, allow_block, -1);
			}
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		}
		if (limit == 0 || (limit != -1 && expr.type == TOKEN_SEMI)){
			parse_load(lex, mem, limit_copy);
			if (pass == 0){
				return outer;
			}
			last_pass->data.block.expr_v[last_pass->data.block.expr_c] = outer;
			last_pass->data.block.expr_c += 1;
			return pass_expression;
		}
		uint8_t simple = 0;
		uint64_t copy;
		switch (expr.type){
		case TOKEN_RETURN:
			if (outer.data.block.expr_c != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Unexpected token 'return'\n");
				return outer;
			}
			build.tag = RETURN_EXPRESSION;
			build.data.deref = pool_request(mem, sizeof(expression_ast));
			lex->index += 1;
			expression_ast retexpr = parse_application_expression(lex, mem, err, end_token, allow_block, -1);
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
			lex->index += 1;
			*build.data.deref = parse_application_expression(lex, mem, err, end_token, allow_block, -1);
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
			copy = parse_save(lex, mem);
			lex->index += 1;
			build.data.size_of.type = parse_type(lex, mem, err, end_token, 0);
			if (*err == 0){
				build.data.size_of.target = NULL;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				break;
			}
			parse_load(lex, mem, copy);
			*err = 0;
			build.data.size_of.target = pool_request(mem, sizeof(expression_ast));
			lex->index += 1;
			*build.data.size_of.target = parse_application_expression(lex, mem, err, end_token, allow_block, -1);
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
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Mutation with '=' requires 1 left operand, was operand number %u\n", outer.data.block.expr_c);
				return outer;
			}
			uint8_t tag = outer.data.block.expr_v[0].tag;
			if (tag != DEREF_EXPRESSION && tag != BINDING_EXPRESSION && tag != ACCESS_EXPRESSION){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : First operand of mutation '=' must be either a dereference or a bound variable name\n");
				return outer;
			}
			if (pass != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Errpr at : Mutation must be dominant expression\n");
				return outer;
			}
			build.tag=BINDING_EXPRESSION;
			build.data.binding.type.tag=NONE_TYPE;
			build.data.binding.name=expr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			pass = 1;
			pass_expression = outer;
			outer.data.block.type.tag=NONE_TYPE;
			outer.data.block.expr_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
			outer.data.block.expr_c = 0;
			last_pass = &pass_expression;
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
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Pointer cast requires left argument\n");
			}
			token open_br = lex->tokens[++lex->index];
			if (open_br.type != TOKEN_BRACK_OPEN){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Attempted to cast to non pointer type\n");
				return outer;
			}
			lex->index += 1;
			type_ast cast_target = parse_type(lex, mem, err, TOKEN_BRACK_CLOSE, 1);
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
		case TOKEN_LABEL:
			label_req = LABEL_REQUESTED;
		case TOKEN_IDENTIFIER:
			build.data.binding.type.tag=NONE_TYPE;
			build.data.binding.name=expr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_BRACK_OPEN:
			copy = parse_save(lex, mem);
			lit = parse_array_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				break;
			}
			parse_load(lex, mem, copy);
			*err = 0;
			lex->index += 1;
			build = parse_application_expression(lex, mem, err, TOKEN_BRACK_CLOSE, 1, -1);
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
			lex->index += 1;
			build = parse_application_expression(lex, mem, err, TOKEN_PAREN_CLOSE, 1, -1);
			if (*err != 0){
				return outer;
			}
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_BRACE_OPEN:
			copy = parse_save(lex, mem);
			lit = parse_struct_literal(lex, mem, err);
			if (*err == 0){
				build.tag = LITERAL_EXPRESSION;
				build.data.literal = lit;
				outer.data.block.expr_v[outer.data.block.expr_c] = build;
				outer.data.block.expr_c += 1;
				break;
			}
			parse_load(lex, mem, copy);
			*err = 0;
			lex->index += 1;
			build = parse_application_expression(lex, mem, err, TOKEN_BRACE_CLOSE, 1, -1);
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
		case TOKEN_CHAR:
			binding_ast char_lit = parse_char_literal(lex, mem, err);
			if (*err != 0){
				return outer;
			}
			build.tag = VALUE_EXPRESSION;
			build.data.binding = char_lit;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_STRING:
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
			build = parse_application_expression(lex, mem, err, end_token, 0, 3);
			if (*err != 0){
				return outer;
			}
			if (build.data.block.expr_c < 2){
				snprintf(err, ERROR_BUFFER, " <!> Parser Error : Require at least one consequent for if special form\n");
				return outer;
			}
			statement_ast iff = {
				.tag=IF_STATEMENT,
				.data.if_statement.predicate = pool_request(mem, sizeof(expression_ast)),
				.data.if_statement.branch = pool_request(mem, sizeof(expression_ast)),
				.data.if_statement.alternate = NULL,
				.type.tag=NONE_TYPE,
				.labeled=0
			};
			*iff.data.if_statement.predicate = build.data.block.expr_v[0];
			*iff.data.if_statement.branch = build.data.block.expr_v[1];
			if (build.data.block.expr_c == 3){
				iff.data.if_statement.alternate = pool_request(mem, sizeof(expression_ast));
				*iff.data.if_statement.alternate = build.data.block.expr_v[2];
			}
			if (label_req == LABEL_WAITING){
				if (outer.data.block.expr_c != 0 && outer.data.block.expr_v[outer.data.block.expr_c-1].tag != BINDING_EXPRESSION){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error : no previous label but one was requested\n");
					return outer;
				}
				label_req = LABEL_FULFILLED;
				iff.labeled = 1;
				iff.label = outer.data.block.expr_v[outer.data.block.expr_c-1].data.binding;
				outer.data.block.expr_c -= 1;
			}
			build.tag = STATEMENT_EXPRESSION;
			build.data.statement = iff;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_FOR:
			build = parse_application_expression(lex, mem, err, end_token, 0, 4);
			if (*err != 0){
				return outer;
			}
			if (build.data.block.expr_c < 4){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : 'for' special form required 4 positional arguments (start -> end -> inc -> procedure), %u provided\n", build.data.block.expr_c);
				return outer;
			}
			statement_ast forr = {
				.tag=FOR_STATEMENT,
				.data.for_statement.start = pool_request(mem, sizeof(expression_ast)),
				.data.for_statement.end = pool_request(mem, sizeof(expression_ast)),
				.data.for_statement.inc = pool_request(mem, sizeof(expression_ast)),
				.data.for_statement.procedure = pool_request(mem, sizeof(expression_ast)),
				.type.tag=NONE_TYPE,
				.labeled=0
			};
			*forr.data.for_statement.start = build.data.block.expr_v[0];
			*forr.data.for_statement.end = build.data.block.expr_v[1];
			*forr.data.for_statement.inc = build.data.block.expr_v[2];
			*forr.data.for_statement.procedure = build.data.block.expr_v[3];
			if (label_req == LABEL_WAITING){
				if (outer.data.block.expr_c != 0 && outer.data.block.expr_v[outer.data.block.expr_c-1].tag != BINDING_EXPRESSION){
					snprintf(err, ERROR_BUFFER, " <!> Parsing Error : no previous label but one was requested\n");
					return outer;
				}
				label_req = LABEL_FULFILLED;
				forr.labeled = 1;
				forr.label = outer.data.block.expr_v[outer.data.block.expr_c-1].data.binding;
				outer.data.block.expr_c -= 1;
			}
			build.tag = STATEMENT_EXPRESSION;
			build.data.statement = forr;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_BREAK:
			if (outer.data.block.expr_c != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Jump directive 'continue' passed as argument\n");
				return outer;
			}
			statement_ast brk = {
				.tag=BREAK_STATEMENT,
				.type.tag=NONE_TYPE,
				.labeled=0
			};
			jump_req = LABEL_REQUESTED;
			build.tag = STATEMENT_EXPRESSION;
			build.data.statement = brk;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_CONTINUE:
			if (outer.data.block.expr_c != 0){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Jump directive 'continue' passed as argument\n");
				return outer;
			}
			statement_ast cnt = {
				.tag=CONTINUE_STATEMENT,
				.type.tag=NONE_TYPE,
				.labeled=0
			};
			jump_req = LABEL_REQUESTED;
			build.tag = STATEMENT_EXPRESSION;
			build.data.statement = cnt;
			outer.data.block.expr_v[outer.data.block.expr_c] = build;
			outer.data.block.expr_c += 1;
			break;
		case TOKEN_LABEL_JUMP:
			if (jump_req != LABEL_WAITING){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : jump label must succeed jump directive statement\n");
				return outer;
			}
			expression_ast* jump_directive = &outer.data.block.expr_v[outer.data.block.expr_c-1];
			if (jump_directive->tag != STATEMENT_EXPRESSION){
				snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Jump directive was not statement?\n");
				return outer;
			}
			binding_ast directive_binding = {
				.name=expr,
				.type.tag=NONE_TYPE
			};
			jump_directive->data.statement.labeled = 1;
			jump_directive->data.statement.label = directive_binding;
			jump_req = LABEL_EXPIRED;
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
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Unexpected token '%s'\n", expr.string);
			return outer;
		}
		if (limit != -1){
			limit_copy = parse_save(lex, mem);
			if (limit > 0){
				limit -= 1;
			}
		}
		if (label_req == LABEL_WAITING){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : label must precede statement\n");
			return outer;
		}
		if (jump_req == LABEL_WAITING){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : jump label must succeed jump directive statement\n");
			return outer;
		}
		if (jump_req == LABEL_EXPIRED){
			jump_req = LABEL_OVERDUE;
		}
		else if (jump_req == LABEL_OVERDUE){
			snprintf(err, ERROR_BUFFER, " <!> Parsing Error at : Expected end of jump directive statement\n");
			return outer;
		}
		if (label_req == LABEL_REQUESTED){
			label_req = LABEL_WAITING;
		}
		if (jump_req == LABEL_REQUESTED){
			jump_req = LABEL_WAITING;
		}
	}
	snprintf(err, ERROR_BUFFER, " <!> Parser Error at : How did you get here in the application parser\n");
	return outer;
}

literal_ast
parse_struct_literal(lexer* const lex, pool* const mem, char* err){
	literal_ast lit = {
		.tag=STRUCT_LITERAL,
		.type.tag=NONE_TYPE
	};
	lit.data.array.member_v = NULL;
	lit.data.array.member_c = 0;
	token tok = lex->tokens[++lex->index];
	if (tok.type == TOKEN_BRACE_CLOSE){
		return lit;
	}
	lit.data.array.member_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	expression_ast build = parse_application_expression(lex, mem, err, TOKEN_COMMA, 0, -1);
	if (*err != 0){
		return lit;
	}
	lit.data.array.member_v[lit.data.array.member_c] = build;
	lit.data.array.member_c += 1;
	tok = lex->tokens[++lex->index];
	if (tok.type == TOKEN_BRACE_CLOSE){
		return lit;
	}
	while (1){
		uint64_t copy = parse_save(lex, mem);
		expression_ast build = parse_application_expression(lex, mem, err, TOKEN_COMMA, 0, -1);
		if (*err == 0){
			lit.data.array.member_v[lit.data.array.member_c] = build;
			lit.data.array.member_c += 1;
			tok = lex->tokens[++lex->index];
			continue;
		}
		parse_load(lex, mem, copy);
		*err = 0;
		build = parse_application_expression(lex, mem, err, TOKEN_BRACE_CLOSE, 0, -1);
		if (*err != 0){
			return lit;
		}
		lit.data.array.member_v[lit.data.array.member_c] = build;
		lit.data.array.member_c += 1;
		return lit;
	}
}

uint64_t
parse_save(lexer* const lex, pool* const mem){
	pool_save(mem);
	return lex->index;
}

void
parse_load(lexer* const lex, pool* const mem, uint64_t index){
	pool_load(mem);
	lex->index = index;
}

literal_ast
parse_array_literal(lexer* const lex, pool* const mem, char* err){
	literal_ast lit = {
		.tag=ARRAY_LITERAL,
		.type.tag=NONE_TYPE
	};
	lit.data.array.member_v = NULL;
	lit.data.array.member_c = 0;
	token tok = lex->tokens[++lex->index];
	if (tok.type == TOKEN_BRACK_CLOSE){
		return lit;
	}
	lit.data.array.member_v = pool_request(mem, MAX_ARGS*sizeof(expression_ast));
	expression_ast build = parse_application_expression(lex, mem, err, TOKEN_COMMA, 0, -1);
	if (*err != 0){
		return lit;
	}
	lit.data.array.member_v[lit.data.array.member_c] = build;
	lit.data.array.member_c += 1;
	tok = lex->tokens[++lex->index];
	if (tok.type == TOKEN_BRACK_CLOSE){
		return lit;
	}
	while (1){
		uint64_t copy = parse_save(lex, mem);
		build = parse_application_expression(lex, mem, err, TOKEN_COMMA, 0, -1);
		if (*err == 0){
			lit.data.array.member_v[lit.data.array.member_c] = build;
			lit.data.array.member_c += 1;
			tok = lex->tokens[++lex->index];
			continue;
		}
		parse_load(lex, mem, copy);
		*err = 0;
		build = parse_application_expression(lex, mem, err, TOKEN_BRACK_CLOSE, 0, -1);
		if (*err != 0){
			return lit;
		}
		lit.data.array.member_v[lit.data.array.member_c] = build;
		lit.data.array.member_c += 1;
		return lit;
	}
	snprintf(err, ERROR_BUFFER, " <!> Parser Error at : How did you get here in array literal parser\n");
	return lit;
}

binding_ast
parse_char_literal(lexer* const lex, pool* const mem, char* err){
	token tok = lex->tokens[lex->index];
	binding_ast char_lit = {.type.tag=NONE_TYPE};
	//TODO escape sequences
	char target = tok.string[0];
	snprintf(tok.string, 5, "%d", (int8_t)target);
	tok.len = 3;
	tok.type = TOKEN_INTEGER;
	char_lit.type.tag=PRIMITIVE_TYPE;
	char_lit.type.data.primitive=I8_TYPE;
	char_lit.name=tok;
	return char_lit;
}

literal_ast
parse_string_literal(lexer* const lex, pool* const mem, char* err){
	literal_ast lit = {
		.tag=STRING_LITERAL,
		.type.tag=NONE_TYPE
	};
	token current = lex->tokens[lex->index];
	lit.data.string.content = current.string;
	lit.data.string.length = current.len;
	return lit;
}

void
push_frame(scope* const s){
	s->frame_stack[s->frame_count] = s->binding_count;
	s->frame_count += 1;
}

void
pop_frame(scope* const s){
	s->frame_count -= 1;
	s->binding_count = s->frame_stack[s->frame_count];
}

void
push_binding(scope* const s, binding_ast binding){
	s->binding_stack[s->binding_count] = binding;
	s->binding_count += 1;
}

void
pop_binding(scope* const s){
	s->binding_count -= 1;
}

void
push_label_frame(scope* const s){
	s->label_frame_stack[s->label_frame_count] = s->label_count;
	s->label_frame_count += 1;
}

void
pop_label_frame(scope* const s){
	s->label_frame_count -= 1;
	s->label_count = s->label_frame_stack[s->label_frame_count];
}

void
push_label(scope* const s, binding_ast binding){
	s->label_stack[s->label_count] = binding;
	s->label_count += 1;
}

void
push_label_scope(scope* const s){
	s->label_scope_stack[s->label_scope_count] = s->label_count;
	s->label_scope_count += 1;
}

void
pop_label_scope(scope* const s){
	s->label_scope_count -= 1;
}

uint8_t
is_label_valid(scope* const s, binding_ast destination){
	uint16_t end = 0;
	if (s->label_scope_count != 0){
		end = s->label_scope_stack[s->label_scope_count-1];
	}
	for (uint16_t i = s->label_count;i>end;--i){
		const char* a = destination.name.string+1;
		const char* b = s->label_stack[i-1].name.string;
		uint8_t found = 1;
		while (*b != ':' && *a != '\0'){
			if (*b != *a){
				found = 0;
				break;
			}
			a += 1;
			b += 1;
		}
		if (found == 1){
			return 1;
		}
	}
	return 0;
}

void
transform_ast(ast* const tree, pool* const mem, char* err){
	scope roll = {
		.binding_stack = pool_request(mem, MAX_STACK_MEMBERS*sizeof(binding_ast)),
		.frame_stack = pool_request(mem, MAX_STACK_MEMBERS*sizeof(uint16_t)),
		.binding_count=0,
		.binding_capacity=MAX_STACK_MEMBERS,
		.frame_count=0,
		.frame_capacity=MAX_STACK_MEMBERS,
		.captures=pool_request(mem, sizeof(capture_stack)),
		.capture_frame=0,
		.label_stack = pool_request(mem, MAX_STACK_MEMBERS*sizeof(binding_ast)),
		.label_count=0,
		.label_capacity=MAX_STACK_MEMBERS,
		.label_frame_stack = pool_request(mem, MAX_STACK_MEMBERS*sizeof(uint16_t)),
		.label_scope_stack = pool_request(mem, MAX_STACK_MEMBERS*sizeof(uint16_t)),
		.label_frame_count=0,
		.label_frame_capacity=MAX_STACK_MEMBERS,
		.label_scope_count=0
	};
	*roll.captures = (capture_stack){
		.prev=NULL,
		.next=NULL,
		.size=0,
		.binding_count_point=0
	};
	push_builtins(&roll, mem);
	roll.builtin_stack_frame = roll.binding_count;
	for (uint32_t i = 0;i<tree->new_type_c;++i){
		new_type_ast* t = &tree->new_type_v[i];
		if (t->type.tag != STRUCT_TYPE){
			continue;
		}
		roll_data_layout(tree, t->type.data.structure, t->name, err);
		if (*err != 0){
			return;
		}
	}
	for (uint32_t i = 0;i<tree->func_c;++i){
		function_ast* f = &tree->func_v[i];
		roll_type(&roll, tree, mem, &f->type, err);
		if (*err != 0){
			return;
		}
		roll_expression(&roll, tree, mem, &f->expression, f->type, 0, NULL, 1, err);
		if (*err != 0){
			return;
		}
	}
}

void
roll_data_layout(ast* const tree, structure_ast* target, token name, char* err){
	//TODO this traversal probably has a faster memoized traversal
	for (uint32_t i = 0;i<target->binding_c;++i){
		type_ast inner = target->binding_v[i].type;
		if (inner.tag != STRUCT_TYPE){
			if (inner.tag != USER_TYPE){
				continue;
			}
			while (inner.tag == USER_TYPE){
				if (strncmp(name.string, inner.data.user.string, TOKEN_MAX) == 0){
					snprintf(err, ERROR_BUFFER, " Struct nesting error\n");
					return;
				}
				new_type_ast* primitive_new_type = new_type_ast_map_access(&tree->types, inner.data.user.string);
				if (primitive_new_type != NULL){
					inner = primitive_new_type->type;
					continue;
				}
				inner = resolve_alias(tree, inner, err);
				if (*err != 0){
					return;
				}
			}
			if (inner.tag != STRUCT_TYPE){
				continue;
			}
		}
		roll_data_layout(tree, inner.data.structure, name, err);
		if (*err != 0){
			return;
		}
	}
	for (uint32_t i = 0;i<target->union_c;++i){
		roll_data_layout(tree, &target->union_v[i], name, err);
		if (*err != 0){
			return;
		}
	}
}

/*
 * TODO LIST

 	1 procedures can just return, thats all, no other anything, just normal functions otherwise
		procedures dont capture at all, can be invoked with function arguments
	2 parametric types/ buffers/ pointers
	3 Finish semantic pass stuff
		1 all the little todos
		2 fix type equality
		3 struct size determination
	4 IR
		1 group function application into distinct calls for defined top level functions
		2 create structures for partial application cases
	5 determine what code will be used and what code will not be used
		1 monomorphization of parametric types/functions that take them
	6 matches on enumerated struct union, maybe with @ / enum access with tag?
	7 struct ordering
	8 code generation
		1 C proof of concept understanding
		2 maybe a native x86 or arm or risc-V
		3 custom vm for game OS (hla doc)
	9 Good error system
	10 round off parser, floats, ints, chars
	11 memory optimizations
		1 separate buffers for different nodes
		2 node size starts small and can resize, old can be reused for next node
		3 resizing doesnt need a copy if last node in buffer
		4 relative pointers

*/

void
handle_procedural_statement(scope* const roll, ast* const tree, pool* const mem, expression_ast* const line, type_ast expected_type, char* const err){
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
	switch
		(target->tag){
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

void
roll_struct_type(scope* const roll, ast* const tree, pool* const mem, structure_ast* const target, char* err){
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

type_ast
roll_expression(
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
		push_label_scope(roll);
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
		pop_label_scope(roll);
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
			lifted_closure.name.string = pool_request(mem, TOKEN_MAX);
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
		uint64_t target_size = type_size(tree, expr->data.size_of.type, err);
		if (*err != 0){
			return expected_type;
		}
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

uint64_t
struct_size_helper(ast* const tree, structure_ast target_struct, uint64_t rolling_size, char* err){
	return rolling_size;//TODO
}

uint64_t
type_size_helper(ast* const tree, type_ast target_type, uint64_t rolling_size, char* err){
	switch(target_type.tag){
	case FUNCTION_TYPE:
		return rolling_size+8;
	case PRIMITIVE_TYPE:
		if (target_type.data.primitive == U8_TYPE) { return rolling_size+1; };
		if (target_type.data.primitive == U16_TYPE) { return rolling_size+2; };
		if (target_type.data.primitive == U32_TYPE) { return rolling_size+4; };
		if (target_type.data.primitive == U64_TYPE) { return rolling_size+8; };
		if (target_type.data.primitive == I8_TYPE) { return rolling_size+1; };
		if (target_type.data.primitive == I16_TYPE) { return rolling_size+2; };
		if (target_type.data.primitive == I32_TYPE) { return rolling_size+4; };
		if (target_type.data.primitive == I64_TYPE) { return rolling_size+8; };
		if (target_type.data.primitive == INT_ANY) { return rolling_size+8; };
		if (target_type.data.primitive == F32_TYPE) { return rolling_size+4; };
		if (target_type.data.primitive == F64_TYPE) { return rolling_size+8; };
		if (target_type.data.primitive == FLOAT_ANY) { return rolling_size+8; };
	case POINTER_TYPE:
		return rolling_size+8;
	case BUFFER_TYPE:
		return rolling_size+8;
	case USER_TYPE:
		type_ast inner = resolve_type_or_alias(tree, target_type, err);
		if (*err != 0){
			return rolling_size;
		}
		return type_size_helper(tree, inner, rolling_size, err);
	case STRUCT_TYPE:
		return struct_size_helper(tree, *target_type.data.structure, rolling_size, err);
	case PROCEDURE_TYPE:
		return rolling_size+8;
	default:
		snprintf(err, ERROR_BUFFER, " [!] Unable to deduce size of type\n");
		return rolling_size;
	}
	snprintf(err, ERROR_BUFFER, " [!] How did you get here in type size deduction?\n");
	return 0;
}

uint64_t type_size(ast*
		const tree, type_ast target_type, char* err){
	return type_size_helper(tree, target_type, 0, err);
}

void
lift_lambda(ast* const tree, expression_ast* expr, type_ast captured_type, binding_ast* captured_bindings, uint16_t total_captures, pool* const mem){
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
		.string=pool_request(mem, TOKEN_MAX)
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

type_ast
prepend_captures(type_ast start, binding_ast* captures, uint16_t total_captures, pool* const mem){
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

void
push_capture_frame(scope* const roll, pool* const mem){
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

uint16_t
pop_capture_frame(scope* const roll, binding_ast** list_result){
	if (list_result != NULL){
		*list_result = roll->captures->binding_list;
	}
	uint16_t size = roll->captures->size;
	roll->captures = roll->captures->prev;
	return size;
}

void
push_capture_binding(scope* const roll, binding_ast binding){
	if (roll->captures->size >= MAX_CAPTURES){
		fprintf(stderr, "Capture limit exceeded in stack frame\n");
		return;
	}
	for (uint32_t i = 0;i<roll->captures->size;++i){
		if (strncmp(binding.name.string, roll->captures->binding_list[i].name.string, TOKEN_MAX) == 0){
			return;
		}
	}
	roll->captures->binding_list[roll->captures->size] = binding;
	roll->captures->size += 1;
}

void
reduce_aliases(ast* const tree, type_ast* left, type_ast* right){
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

type_ast
resolve_alias(ast* const tree, type_ast root, char* err){
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

type_ast
resolve_type_or_alias(ast* const tree, type_ast root, char* err){
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

type_ast
apply_type(type_ast* const func, char* err){
	type_ast left = {.tag=NONE_TYPE};
	if (func->tag != FUNCTION_TYPE){
		snprintf(err, ERROR_BUFFER, " [!] Tried to set lambda to non function binding\n");
		return left;
	}
	left = *func->data.function.left;
	*func = *func->data.function.right;
	return left;
}

uint8_t
type_cmp(type_ast* const a, type_ast* const b, TYPE_CMP_PURPOSE purpose){
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



uint8_t
struct_cmp(structure_ast* const a, structure_ast* const b){
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

type_ast*
scope_contains(scope* const roll, binding_ast* const binding, uint8_t* needs_capturing){
	for (uint16_t i = 0;i<roll->binding_count;++i){
		uint16_t index = roll->binding_count - (i+1);
		if (strncmp(roll->binding_stack[index].name.string, binding->name.string, TOKEN_MAX) == 0){
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

type_ast
roll_statement_expression(
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
		push_label_frame(roll);
		if (statement->labeled == 1){
			push_label(roll, statement->label);
		}
		type_ast btype = roll_expression(roll, tree, mem, statement->data.if_statement.branch, expected_type, 0, NULL, 0, err);
		if (*err != 0){
			pop_label_frame(roll);
			return expected_type;
		}
		if (statement->data.if_statement.alternate == NULL){
			pop_label_frame(roll);
			if (as_expression == 0){
				return expected_type;
			}
			snprintf(err, ERROR_BUFFER, " [!] Expression conditional statement must have alternate branch\n");
			return expected_type;
		}
		type_ast atype = roll_expression(roll, tree, mem, statement->data.if_statement.alternate, expected_type, 0, NULL, 0, err);
		pop_label_frame(roll);
		if (*err != 0){
			return expected_type;
		}
		if (type_cmp(&btype, &atype, FOR_EQUALITY) != 0){
			snprintf(err, ERROR_BUFFER, " [!] Branch and alternate types in conditional statement expression do not match\n");
			return expected_type;
		}
		statement->type = btype;
		return btype;

	case FOR_STATEMENT:
		if (as_expression == 1){
			snprintf(err, ERROR_BUFFER, " [!] 'for' special form can only be statement for now\n");
			return expected_type;
		}
		type_ast temp = expected_type;
		expected_type.tag = PROCEDURE_TYPE;
		expected_type.data.pointer = pool_request(mem, sizeof(type_ast));
		*expected_type.data.pointer = temp;
		type_ast range_type = {
			.tag = PRIMITIVE_TYPE,
			.data.primitive=INT_ANY
		};
		roll_expression(roll, tree, mem, statement->data.for_statement.start, range_type, 0, NULL, 0, err);
		if (*err != 0){
			return expected_type;
		}
		push_label_frame(roll);
		if (statement->labeled == 1){
			push_label(roll, statement->label);
		}
		roll_expression(roll, tree, mem, statement->data.for_statement.end, range_type, 0, NULL, 0, err);
		if (*err != 0){
			pop_label_frame(roll);
			return expected_type;
		}
		type_ast iter_type = {
			.tag=FUNCTION_TYPE,
			.data.function.left=pool_request(mem, sizeof(type_ast)),
			.data.function.right=pool_request(mem, sizeof(type_ast))
		};
		*iter_type.data.function.left = range_type;
		*iter_type.data.function.right = range_type;
		roll_expression(roll, tree, mem, statement->data.for_statement.inc, iter_type, 0, NULL, 0, err);
		if (*err != 0){
			pop_label_frame(roll);
			return expected_type;
		}
		*iter_type.data.function.left = range_type;
		*iter_type.data.function.right = expected_type;
		roll_expression(roll, tree, mem, statement->data.for_statement.procedure, iter_type, 0, NULL, 0, err);
		pop_label_frame(roll);
		if (*err != 0){
			return expected_type;
		}
		statement->type = expected_type;
		return expected_type;

	case BREAK_STATEMENT:
		if (as_expression == 1){
			snprintf(err, ERROR_BUFFER, " [!] 'break' jump directive can only be statement for now\n");
			return expected_type;
		}
		type_ast brktemp = expected_type;
		expected_type.tag = PROCEDURE_TYPE;
		expected_type.data.pointer = pool_request(mem, sizeof(type_ast));
		*expected_type.data.pointer = brktemp;
		if (statement->labeled == 1){
			if (is_label_valid(roll, statement->label) == 0){
				snprintf(err, ERROR_BUFFER, " [!] 'break' jump directive has invalid destination\n");
				return expected_type;
			}
		}
		statement->type = expected_type;
		return expected_type;

	case CONTINUE_STATEMENT:
		if (as_expression == 1){
			snprintf(err, ERROR_BUFFER, " [!] 'continue' jump directive can only be statement for now\n");
			return expected_type;
		}
		type_ast cnttemp = expected_type;
		expected_type.tag = PROCEDURE_TYPE;
		expected_type.data.pointer = pool_request(mem, sizeof(type_ast));
		*expected_type.data.pointer = cnttemp;
		if (statement->labeled == 1){
			if (is_label_valid(roll, statement->label) == 0){
				snprintf(err, ERROR_BUFFER, " [!] 'continue' jump directive has invalid destination\n");
				return expected_type;
			}
		}
		statement->type = expected_type;
		return expected_type;

	default:
		snprintf(err, ERROR_BUFFER, " [!] Unknown statement type %u\n", statement->tag);
	}
	return expected_type;
}

type_ast
roll_literal_expression(
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

void
hash_keyword(TOKEN_TYPE_TAG_map* keywords, const char* key, TOKEN_TYPE_TAG value){
	TOKEN_TYPE_TAG* persistent_value = pool_request(keywords->mem, sizeof(TOKEN_TYPE_TAG));
	*persistent_value = value;
	TOKEN_TYPE_TAG_map_insert(keywords, key, persistent_value);
}

void
add_keyword_hashes(TOKEN_TYPE_TAG_map* keywords){
	hash_keyword(keywords, "if", TOKEN_IF);
	hash_keyword(keywords, "else", TOKEN_ELSE);
	hash_keyword(keywords, "for", TOKEN_FOR);
	hash_keyword(keywords, "using", TOKEN_IMPORT);
	hash_keyword(keywords, "match", TOKEN_MATCH);
	hash_keyword(keywords, "return", TOKEN_RETURN);
	hash_keyword(keywords, "u8", TOKEN_U8);
	hash_keyword(keywords, "u16", TOKEN_U16);
	hash_keyword(keywords, "u32", TOKEN_U32);
	hash_keyword(keywords, "u64", TOKEN_U64);
	hash_keyword(keywords, "i8", TOKEN_I8);
	hash_keyword(keywords, "i16", TOKEN_I16);
	hash_keyword(keywords, "i32", TOKEN_I32);
	hash_keyword(keywords, "i64", TOKEN_I64);
	hash_keyword(keywords, "f32", TOKEN_F32);
	hash_keyword(keywords, "f64", TOKEN_F64);
	hash_keyword(keywords, "type", TOKEN_TYPE);
	hash_keyword(keywords, "alias", TOKEN_ALIAS);
	hash_keyword(keywords, "var", TOKEN_MUTABLE);
	hash_keyword(keywords, "ptr", TOKEN_REF);
	hash_keyword(keywords, "procedure", TOKEN_PROC);
	hash_keyword(keywords, "constant", TOKEN_CONST);
	hash_keyword(keywords, "as", TOKEN_CAST);
	hash_keyword(keywords, "sizeof", TOKEN_SIZEOF);
	hash_keyword(keywords, "break", TOKEN_BREAK);
	hash_keyword(keywords, "continue", TOKEN_CONTINUE);
	hash_keyword(keywords, "+", TOKEN_ADD);
	hash_keyword(keywords, "-", TOKEN_SUB);
	hash_keyword(keywords, "*", TOKEN_MUL);
	hash_keyword(keywords, "/", TOKEN_DIV);
	hash_keyword(keywords, ".+", TOKEN_FLADD);
	hash_keyword(keywords, ".-", TOKEN_FLSUB);
	hash_keyword(keywords, ".*", TOKEN_FLMUL);
	hash_keyword(keywords, "./", TOKEN_FLDIV);
	hash_keyword(keywords, "%", TOKEN_MOD);
	hash_keyword(keywords, "<<", TOKEN_SHL);
	hash_keyword(keywords, ">>", TOKEN_SHR);
	hash_keyword(keywords, ".<", TOKEN_FLANGLE_OPEN);
	hash_keyword(keywords, ".>", TOKEN_FLANGLE_CLOSE);
	hash_keyword(keywords, ".<=", TOKEN_FLLESS_EQ);
	hash_keyword(keywords, ".>=", TOKEN_FLGREATER_EQ);
	hash_keyword(keywords, "=", TOKEN_SET);
	hash_keyword(keywords, ".==", TOKEN_FLEQ);
	hash_keyword(keywords, ".!=", TOKEN_FLNOT_EQ);
	hash_keyword(keywords, "&&", TOKEN_BOOL_AND);
	hash_keyword(keywords, "||", TOKEN_BOOL_OR);
	hash_keyword(keywords, "&", TOKEN_BIT_AND);
	hash_keyword(keywords, "|", TOKEN_BIT_OR);
	hash_keyword(keywords, "^", TOKEN_BIT_XOR);
	hash_keyword(keywords, "~", TOKEN_BIT_COMP);
	hash_keyword(keywords, "!", TOKEN_BOOL_NOT);
	hash_keyword(keywords, "->", TOKEN_FUNC_IMPL);
	hash_keyword(keywords, "<|", TOKEN_PIPE_LEFT);
	hash_keyword(keywords, "|>", TOKEN_PIPE_RIGHT);
	hash_keyword(keywords, "//", TOKEN_COMMENT);
	hash_keyword(keywords, "/*", TOKEN_MULTI_OPEN);
	hash_keyword(keywords, "*/", TOKEN_MULTI_CLOSE);
}

token*
lex_cstr(const char* const buffer, uint64_t size_bytes, pool* const mem, uint64_t* token_count, char** string_content, char* err){
	TOKEN_TYPE_TAG_map keywords = TOKEN_TYPE_TAG_map_init(mem);
	add_keyword_hashes(&keywords);
	*token_count = 0;
	uint64_t token_capacity = sizeof(token)*READ_TOKEN_CHUNK;
	token* tokens = pool_request(mem, token_capacity);
	token tok = {
		.len=0,
		.string=*string_content
	};
	uint64_t i = 0;
	for (char c = buffer[i];i<size_bytes;c = buffer[++i]){
		*string_content += tok.len+1;
		tok.string = *string_content;
		tok.len = 0;
		if (*token_count == token_capacity){
			token_capacity += sizeof(token)*READ_TOKEN_CHUNK;
			pool_request(mem, sizeof(token)*READ_TOKEN_CHUNK);
		}
		switch (c){
		case '\n':
		case ' ':
		case '\t':
		case '\r':
			continue;
		default:
		}
		if (isdigit(c)){
			uint8_t dec = 0;
			tok.type = TOKEN_INTEGER;
			for (char k = c;i<size_bytes;k = buffer[++i]){
				if (!isdigit(k)){
					if (k == '.' && dec == 0){
						dec = 1;
						tok.type = TOKEN_FLOAT;
					}
					else{
						break;
					}
				}
				tok.string[tok.len] = k;
				tok.len += 1;
			}
			i -= 1;
			tok.string[tok.len] = '\0';
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		}
		else if (isalnum(c) || c == '_'){
			uint32_t identifier_hash = 5381;
			tok.string[tok.len] = c;
			tok.len += 1;
			tok.type = TOKEN_IDENTIFIER;
			identifier_hash = ((identifier_hash<<5)+identifier_hash)+((int16_t)c);
			for (char k = buffer[++i];i<size_bytes;k = buffer[++i]){
				if (!isalnum(k) && k != '_'){
					if (k == ':'){
						tok.string[tok.len] = k;
						tok.len += 1;
						tok.type = TOKEN_LABEL;
						i += 1;
					}
					break;
				}
				tok.string[tok.len] = k;
				tok.len += 1;
				identifier_hash = ((identifier_hash<<5)+identifier_hash)+((int16_t)k);
			}
			i -= 1;
			tok.string[tok.len] = '\0';
			TOKEN_TYPE_TAG* iskeyword = TOKEN_TYPE_TAG_map_access_by_hash(&keywords, identifier_hash, tok.string);
			if (iskeyword != NULL){
				tok.type = *iskeyword;
			}
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		}
		tok.type = TOKEN_SYMBOL;
		uint8_t settled = 1;
		switch (c){
		case '{': tok.type = TOKEN_BRACE_OPEN; break;
		case '}': tok.type = TOKEN_BRACE_CLOSE; break;
		case '[': tok.type = TOKEN_BRACK_OPEN; break;
		case ']': tok.type = TOKEN_BRACK_CLOSE; break;
		case '(': tok.type = TOKEN_PAREN_OPEN; break;
		case ')': tok.type = TOKEN_PAREN_CLOSE; break;
		case '\\': tok.type = TOKEN_ARG; break;
		case ';': tok.type = TOKEN_SEMI; break;
		case '.': tok.type = TOKEN_COMPOSE; break;
		case '$': tok.type = TOKEN_PASS; break;
		case '#': tok.type = TOKEN_ENCLOSE; break;
		case ',': tok.type = TOKEN_COMMA; break;
		case '\'': 
			tok.type = TOKEN_CHAR;
			char char_item = buffer[++i];
			if (i >= size_bytes){
				snprintf(err, ERROR_BUFFER, "Lexing Error, unexpected end of file\n");
				return tokens;
			}
			if (char_item == '\\'){
				char_item = buffer[++i];
				if (i >= size_bytes){
					snprintf(err, ERROR_BUFFER, "Lexing Error, unexpected end of file\n");
					return tokens;
				}
				tok.string[tok.len] = char_item;
				tok.len += 1;
			}
			tok.string[tok.len] = char_item;
			tok.len += 1;
			char_item = buffer[++i];
			if (char_item != '\'' || i >= size_bytes){
				snprintf(err, ERROR_BUFFER, " Lexing error, expected (') to end character literal\n");
				return tokens;
			}
			tok.string[tok.len] = '\0';
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		case '"':
			tok.type = TOKEN_STRING;
			for (char k = buffer[++i];i<size_bytes;k = buffer[++i]){
				if (k == '"'){
					break;
				}
				tok.string[tok.len] = k;
				tok.len += 1;
			}
			if (i == size_bytes){
				snprintf(err, ERROR_BUFFER, " Lexing Error, ended file while parsing string literal, expected '\"'\n");
				return tokens;
			}
			tok.string[tok.len] = '\0';
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		case ':':
			tok.string[tok.len] = c;
			tok.len += 1;
			c = buffer[++i];
			if (isdigit(c)){
				tok.type = TOKEN_SYMBOL;
				break;
			}
			tok.type = TOKEN_LABEL_JUMP;
			for (char k = c;i < size_bytes;k = buffer[++i]){
				if ((!isalnum(k)) && (k != '_')){
					break;
				}
				tok.string[tok.len] = k;
				tok.len += 1;
			}
			i -= 1;
			tok.string[tok.len] = '\0';
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		default:
			settled = 0;
			break;
		}
		if (settled == 1){
			tok.string[tok.len] = c;
			tok.len += 1;
			tok.string[tok.len] = '\0';
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		}
		else if (issymbol(c)){
			uint32_t symbol_hash = 5381;
			char k = c;
			for (;i<size_bytes;k=buffer[++i]){
				if ((!issymbol(k))
				|| (k == '(')
				|| (k == ')')
				|| (k == '[')
				|| (k == ']')
				|| (k == '{')
				|| (k == '}')
				|| (k == ';')){
					break;
				}
				tok.string[tok.len] = k;
				tok.len += 1;
				symbol_hash = ((symbol_hash<<5)+symbol_hash)+((int16_t)k);
			}
			tok.string[tok.len] = '\0';
			TOKEN_TYPE_TAG* iskeyword = TOKEN_TYPE_TAG_map_access_by_hash(&keywords, symbol_hash, tok.string);
			if (iskeyword != NULL){
				tok.type = *iskeyword;
			}
			if (tok.type == TOKEN_COMMENT){
				for (;i<size_bytes&&k != '\n';k=buffer[++i]){}
				i -= 1;
				continue;
			}
			if (tok.type == TOKEN_MULTI_OPEN){
				while (buffer[i++] != '*'
				    && i < size_bytes
					&& buffer[i] != '/'
				){}
				continue;
			}
			i -= 1;
			tokens[*token_count] = tok;
			*token_count += 1;
			continue;
		}
		if (c == EOF){
			tok.type = TOKEN_EOF;
			tokens[*token_count] = tok;
			*token_count += 1;
			break;
		}
	}
	return tokens;
}

int
compile_file(char* filename){
	FILE* fd = fopen(filename, "r");
	if (fd == NULL){
		fprintf(stderr, "File not found '%s'\n", filename);
		return 1;
	}
	pool read_buffer = pool_alloc(STRING_CONTENT_BUFFER+READ_BUFFER_SIZE+POOL_SIZE, POOL_STATIC);
	pool_request(&read_buffer, READ_BUFFER_SIZE);
	uint64_t read_bytes = fread(read_buffer.buffer, sizeof(char), READ_BUFFER_SIZE, fd);
	fclose(fd);
	if (read_bytes == READ_BUFFER_SIZE){
		pool_dealloc(&read_buffer);
		fprintf(stderr, "file larger than allowed read buffer\n");
		return 1;
	}
	int comp = compile_cstr(&read_buffer, read_bytes);
	return comp;
}

int
compile_cstr(pool* const mem, uint64_t read_bytes){
	char err[ERROR_BUFFER] = "\0";
	uint64_t token_count = 0;
	printf("%lu bytes left\n", mem->left);
	char* string_content_buffer = pool_request(mem, STRING_CONTENT_BUFFER);
	token* tokens = lex_cstr(mem->buffer, read_bytes, mem, &token_count, &string_content_buffer, err);
	printf("Lexed\n");
	printf("%lu bytes left\n", mem->left);
	if (*err != 0){
		fprintf(stderr, "Could not compile\n");
		fprintf(stderr, err);
		pool_dealloc(mem);
		return 1;
	}
	ast tree = parse(tokens, mem, token_count, string_content_buffer, err);
	if (err[0] != '\0'){
		fprintf(stderr, "Could not compile\n");
		fprintf(stderr, err);
		pool_dealloc(mem);
		return 1;
	}
	show_ast(&tree);
	printf("Parsed\n");
	printf("%lu bytes left\n", mem->left);
	transform_ast(&tree, mem, err);
	if (*err != 0){
		fprintf(stderr, "Could not compile\n");
		fprintf(stderr, err);
		show_ast(&tree);
		pool_dealloc(mem);
		return 1;
	}
	show_ast(&tree);
	printf("Compiled\n");
	printf("%lu bytes left\n", mem->left);
	pool_dealloc(mem);
	return 0;
}

void
binary_int_builtin(scope* const roll, pool* const mem, token name){
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

void
binary_float_builtin(scope* const roll, pool* const mem, token name){
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

void
unary_int_builtin(scope* const roll, pool* const mem, token name){
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

void
push_builtins(scope* const roll, pool* const mem){
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

void
show_token(const token* const tok){
	printf("%.*s ", TOKEN_MAX, tok->string);
	fflush(stdout);
}

void
show_ast(const ast* const tree){
	return;
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

void
indent_n(uint8_t n){
	for (uint8_t i = 0;i<n;++i){
		printf("  ");
	}
}

void
show_structure(const structure_ast* const structure, uint8_t indent){
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

void
show_lambda(const lambda_ast* const lambda){
	for (size_t i = 0;i<lambda->argc;++i){
		show_token(&lambda->argv[i]);
	}
	printf("<- ");
	show_expression(lambda->expression, 1);
}

void
show_type(const type_ast* const type){
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

void
show_binding(const binding_ast* const binding){
	show_token(&binding->name);
}

void
show_statement(const statement_ast* const statement, uint8_t indent){
	if (statement->labeled == 1){
		printf("\033[1;31mlabel \033[0m");
		show_binding(&statement->label);
	}
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
	case FOR_STATEMENT:
		printf("\033[1;31mfor \033[0m");
		show_expression(statement->data.for_statement.start, indent);
		printf("\033[1;31mto \033[0m");
		show_expression(statement->data.for_statement.end, indent);
		printf("\033[1;31mby \033[0m");
		show_expression(statement->data.for_statement.inc, indent);
		printf("\033[1;31mdo \033[0m");
		show_expression(statement->data.for_statement.procedure, indent);
		break;
	case BREAK_STATEMENT:
		printf("\033[1;31mbreak \033[0m");
		if (statement->labeled == 1){
			show_binding(&statement->label);
		}
		break;
	case CONTINUE_STATEMENT:
		printf("\033[1;31mcontinue \033[0m");
		if (statement->labeled == 1){
			show_binding(&statement->label);
		}
		break;
	default:
		printf("<UNKNOWN STATEMENT> ");
		break;
	}
}

void
show_literal(const literal_ast* const lit){
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

void
show_expression(const expression_ast* const expr, uint8_t indent){
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

void
show_new_type(const new_type_ast* const new_type){
	show_type(&new_type->type);
	show_token(&new_type->name);
	printf("\n");
}

void
show_constant(const constant_ast* const cnst){
	show_token(&cnst->name);
	printf(": ");
	show_binding(&cnst->value);
	printf("\n");
}

void
show_alias(const alias_ast* const alias){
	show_type(&alias->type);
	show_token(&alias->name);
	printf("\n");
}

void
show_function(const function_ast* const func){
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

int
main(int argc, char** argv){
	compile_file("correct.ka");
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
	return 0;
}
