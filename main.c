#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Anti-plagiarism note:
// Dear Professor,
// I have uploaded my code onto my personal github repository, which is
// avaliable under the url: github.com/Wasymir/ini-files. If any of the
// anti-plagiarsm software, you use, shows a match with that repository, please
// be aware this is an false-positive.

#if __STDC_VERSION__ < 202311L
#error "This program requires c23 standard"
#endif

#define PANIC                                                            \
    {                                                                    \
	fprintf(stderr, "The program encounted unrecoverable error.\n"); \
	exit(EXIT_FAILURE);                                              \
    }

#define STRLEN(s) (sizeof(s) - 1)

typedef enum {
    ERR_NIL = 0,
    ERR_INTERNAL,
    FILE_ERROR_START,
    ERR_FILE_SECT_BAD_CHAR,
    ERR_FILE_SECT_UNCLOSED,
    ERR_FILE_SECT_EMPTY,
    ERR_FILE_SECT_DUP,
    ERR_FILE_ASSIGN_NO_OP,
    ERR_FILE_ASSIGN_NO_VAL,
    ERR_FILE_ASSIGN_NO_SECT,
    ERR_FILE_ASSIGN_NO_KEY,
    ERR_FILE_ASSIGN_KEY_DUP,
    ARGUMENT_ERROR_START,
    ERR_ARG_SECT_NOT_FOUND,
    ERR_ARG_SECT_BAD_CHAR,
    ERR_ARG_KEY_NOT_FOUND,
    ERR_ARG_NO_SECT,
    ERR_ARG_NO_KEY,
    ERR_ARG_NO_OP,
    ERR_ARG_BAD_OP,
    ERR_ARG_TYPE,
    ERR_ARG_ILLEGAL_OP,
    ERR_ARG_DIV_ZERO,
} Error;

void printErr(Error err, size_t line_no)
{
    if (err == ERR_NIL)
	return;

    char *prefix;
    if (err > ARGUMENT_ERROR_START) {
	prefix = "Argument error";
    } else if (err > FILE_ERROR_START) {
	prefix = "File error";
    } else {
	prefix = "Application error";
    }

    char *msg;
    // clang-format off
    switch (err) {
    case ERR_INTERNAL: msg = "internal"; break;
    case ERR_FILE_SECT_BAD_CHAR: msg = "illegal character in section name"; break;
    case ERR_FILE_SECT_UNCLOSED: msg = "unclosed section name"; break;
    case ERR_FILE_SECT_EMPTY: msg = "section name cannot be empty"; break;
    case ERR_FILE_SECT_DUP: msg = "section with such name already exists"; break;
    case ERR_FILE_ASSIGN_NO_OP: msg = "missing assignment operator"; break;
    case ERR_FILE_ASSIGN_NO_VAL: msg = "missing value"; break;
    case ERR_FILE_ASSIGN_NO_KEY: msg = "missing key"; break;
    case ERR_FILE_ASSIGN_NO_SECT: msg = "cannot define value without section"; break;
    case ERR_FILE_ASSIGN_KEY_DUP: msg = "value with such key already exists"; break;
    case ERR_ARG_SECT_NOT_FOUND: msg = "such section does not exist"; break;
    case ERR_ARG_KEY_NOT_FOUND: msg = "value with such key does not exist"; break;
    case ERR_ARG_NO_SECT: msg = "missing section name"; break;
    case ERR_ARG_NO_KEY: msg = "missing key"; break;
    case ERR_ARG_NO_OP: msg = "missing operator"; break;
    case ERR_ARG_BAD_OP: msg = "such operator does not exist"; break;
    case ERR_ARG_TYPE: msg = "type mismatch"; break;
    case ERR_ARG_ILLEGAL_OP: msg = "such operator cannot be applied"; break;
    case ERR_ARG_DIV_ZERO: msg = "attempted division by zero"; break;
    default: PANIC;
    }
    // clang-format on

    fputs(prefix, stderr);
    if (err > FILE_ERROR_START && err < ARGUMENT_ERROR_START) {
	fprintf(stderr, " at line %zu: ", line_no);
    } else {
	fputs(": ", stderr);
    }
    fputs(msg, stderr);
    fputc('\n', stderr);
}

#define TRY(v)           \
    {                    \
	Error err = (v); \
	if (err)         \
	    return err;  \
    }

#define TRY_CATCH(v, s)  \
    {                    \
	Error err = (v); \
	if (err) {       \
	    { s };       \
	    return err;  \
	}                \
    }

Error scaleArray(void **arr, size_t *cap, const size_t el_size)
{
    const size_t new_cap = 2 * *cap + 1;
    void *new_arr = realloc(*arr, new_cap * el_size);
    if (new_arr == nullptr)
	return ERR_INTERNAL;
    *cap = new_cap;
    *arr = new_arr;
    return ERR_NIL;
}

typedef struct {
    char *arr;
    size_t len;
    size_t cap;
} Buffer;

Error BufAppend(Buffer *buf, const char c)
{
    if (buf->len == buf->cap)
	TRY(scaleArray((void **)&buf->arr, &buf->cap, sizeof(char)));
    buf->arr[buf->len++] = c;
    return ERR_NIL;
}

void BufClear(Buffer *buf)
{
    buf->len = 0;
}

void BufFree(Buffer *buf)
{
    free(buf->arr);
    buf->arr = nullptr;
    buf->len = 0;
    buf->cap = 0;
}

typedef enum {
    PAIR_TXT,
    PAIR_NUM,
} PairType;

typedef struct {
    char *key;
    PairType type;
    union {
	char *val_txt;
	long val_num;
    };
} Pair;

typedef struct {
    char *name;
    Pair *pairs;
    size_t len;
    size_t cap;
} Section;

typedef struct {
    Section *sections;
    size_t len;
    size_t cap;
} Registry;

Error RegNewSection(Registry *reg, char *name)
{
    for (size_t i = 0; i < reg->len; i++)
	if (strcmp(reg->sections[i].name, name) == 0)
	    return ERR_FILE_SECT_DUP;

    if (reg->len == reg->cap)
	TRY(scaleArray((void **)&reg->sections, &reg->cap, sizeof(Section)));

    reg->sections[reg->len++] = (Section){
	.name = name,
	.pairs = nullptr,
	.len = 0,
	.cap = 0,
    };
    return ERR_NIL;
}

Error RegNewTextValue(Registry *reg, char *key, char *val)
{
    if (reg->len == 0)
	return ERR_FILE_ASSIGN_NO_SECT;

    Section *section = &reg->sections[reg->len - 1];

    for (size_t i = 0; i < section->len; i++)
	if (strcmp(section->pairs[i].key, key) == 0)
	    return ERR_FILE_ASSIGN_KEY_DUP;

    if (section->len == section->cap)
	TRY(scaleArray((void **)&section->pairs, &section->cap, sizeof(Pair)));

    section->pairs[section->len++] = (Pair){
	.key = key,
	.type = PAIR_TXT,
	.val_txt = val,
    };

    return ERR_NIL;
}

Error RegNewNumberValue(Registry *reg, char *key, long val)
{
    if (reg->len == 0)
	return ERR_FILE_ASSIGN_NO_SECT;

    Section *section = &reg->sections[reg->len - 1];

    for (size_t i = 0; i < section->len; i++)
	if (strcmp(section->pairs[i].key, key) == 0)
	    return ERR_FILE_ASSIGN_KEY_DUP;

    if (section->len == section->cap)
	TRY(scaleArray((void **)&section->pairs, &section->cap, sizeof(Pair)));

    section->pairs[section->len++] = (Pair){
	.key = key,
	.type = PAIR_NUM,
	.val_num = val,
    };

    return ERR_NIL;
}

Pair *RegGetPair(const Registry *reg, const char *section_name, const char *key)
{
    Section *section = nullptr;
    for (size_t i = 0; i < reg->len; i++)
	if (strcmp(reg->sections[i].name, section_name) == 0) {
	    section = &reg->sections[i];
	    break;
	}

    if (section == nullptr)
	return nullptr;

    for (size_t i = 0; i < section->len; i++)
	if (strcmp(section->pairs[i].key, key) == 0)
	    return &section->pairs[i];

    return nullptr;
}

void RegFree(Registry *reg)
{
    for (size_t i = 0; i < reg->len; i++) {
	Section *section = &reg->sections[i];
	for (size_t j = 0; j < section->len; j++) {
	    Pair *pair = &section->pairs[j];
	    free(pair->key);
	    if (pair->type == PAIR_TXT)
		free(pair->val_txt);
	}
	free(section->name);
	free(section->pairs);
    }
    free(reg->sections);
    reg->sections = nullptr;
    reg->len = 0;
    reg->cap = 0;
}

typedef enum {
    STMT_NIL = 0,
    STMT_SECTION,
    STMT_PAIR,
} StatementType;

typedef struct {
    StatementType type;
    union {
	struct {
	    const char *section_name;
	    size_t section_len;
	};
	struct {
	    const char *key;
	    size_t key_len;
	    const char *value;
	    size_t value_len;
	};
    };
} Statement;

Error parseStmt(const Buffer *buf, Statement *stmt)
{
    const char *c = buf->arr;
    const char *e = buf->arr + buf->len;

    stmt->type = STMT_NIL;

    while (c != e) {
	if (!isspace(*c))
	    break;
	c++;
    }

    if (c == e || *c == ';')
	return ERR_NIL;

    if (*c == '[') {
	const char *s = ++c;
	while (c != e) {
	    if (*c == ']')
		break;
	    if (isalnum(*c) || *c == '-') {
		c++;
		continue;
	    }
	    return ERR_FILE_SECT_BAD_CHAR;
	}

	if (c == e)
	    return ERR_FILE_SECT_UNCLOSED;

	if (s == c)
	    return ERR_FILE_SECT_EMPTY;

	stmt->type = STMT_SECTION;
	stmt->section_name = s;
	stmt->section_len = c - s;

    } else {
        if (*c == '=')
            return ERR_FILE_ASSIGN_NO_KEY;

	const char *ks = c;

	while (c != e) {
	    if (isspace(*c) || *c == '=')
		break;
	    c++;
	}

	const char *ke = c;

	while (c != e) {
	    if (!isspace(*c))
		break;
	    c++;
	}

	if (c == e || *c != '=')
	    return ERR_FILE_ASSIGN_NO_OP;

	c++;
	while (c != e) {
	    if (!isspace(*c))
		break;
	    c++;
	}

	if (c == e)
	    return ERR_FILE_ASSIGN_NO_VAL;

	const char *vs = c;

	while (c != e) {
	    if (isspace(*c))
		break;
	    c++;
	}

	const char *ve = c;

	stmt->type = STMT_PAIR;
	stmt->key = ks;
	stmt->key_len = ke - ks;
	stmt->value = vs;
	stmt->value_len = ve - vs;
    }
    return ERR_NIL;
}

// Don't disturb him,
// 	this ferret is hunting for bugs
//
//                                _,-/"---,
//         ;"""""""""";         _/;; ""  <@`---v
//       ; :::::  ::  "\      _/ ;;  "    _.../
//      ;"     ;;  ;;;  \___/::    ;;,'""""
//     ;"          ;;;;.  ;;  ;;;  ::/
//    ,/ / ;;  ;;;______;;;  ;;; ::,/
//    /;;V_;;   ;;;       \       /
//    | :/ / ,/            \_ "")/
//    | | / /"""=            \;;\""=
//    ; ;{::""""""=            \"""=
// ;"""";
// \/"""

Error RegEvalStmt(Registry *reg, const Statement *stmt)
{
    switch (stmt->type) {
    case STMT_SECTION:
	char *name = strndup(stmt->section_name, stmt->section_len);
	if (name == nullptr)
	    return ERR_INTERNAL;
	TRY_CATCH(RegNewSection(reg, name), { free(name); });
	break;
    case STMT_PAIR:
	char *key = strndup(stmt->key, stmt->key_len);
	if (key == nullptr)
	    return ERR_INTERNAL;
	char *value = strndup(stmt->value, stmt->value_len);
	if (value == nullptr) {
	    free(key);
	    return ERR_INTERNAL;
	}
	char *end_ptr;
	const long n = strtol(value, &end_ptr, 10);
	if (end_ptr == value + stmt->value_len) {
	    free(value);
	    TRY_CATCH(RegNewNumberValue(reg, key, n), { free(key); })
	} else {
	    TRY_CATCH(RegNewTextValue(reg, key, value), {
		free(key);
		free(value);
	    });
	}
	break;
    case STMT_NIL:
	break;
    default:
	PANIC;
    }
    return ERR_NIL;
}

void RegPrint(const Registry *reg)
{
    for (size_t i = 0; i < reg->len; i++) {
	const Section *section = &reg->sections[i];
	for (size_t j = 0; j < section->len; j++) {
	    const Pair *pair = &section->pairs[j];
	    printf("%s.%s = ", section->name, pair->key);
	    switch (pair->type) {
	    case PAIR_TXT:
		printf("%s (TXT)\n", pair->val_txt);
		break;
	    case PAIR_NUM:
		printf("%ld (NUM)\n", pair->val_num);
		break;
	    default:
		PANIC;
	    }
	}
    }
}

typedef struct {
    char *section_name;
    char *key;
} Retrival;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
} Operation;

typedef struct {
    Operation op;
    char *lhs_section_name;
    char *lhs_key;
    char *rhs_section_name;
    char *rhs_key;
} Expression;

Error parseRet(const char *str, Retrival *ret)
{
    const char *c = str;
    const char *e = str + strlen(str);

    while (c != e) {
	if (!isspace(*c))
	    break;
	c++;
    }

    if (c == e || *c == '.')
	return ERR_ARG_NO_SECT;

    const char *ss = c;

    while (c != e) {
	if (*c == '.')
	    break;
	if (isalnum(*c) || *c == '-') {
	    c++;
	    continue;
	}

	return ERR_ARG_SECT_BAD_CHAR;
    }

    if (c == e)
	return ERR_ARG_NO_KEY;

    const char *se = c;
    const char *ks = ++c;

    if (c == e || isspace(*c))
	return ERR_ARG_NO_KEY;

    while (c != e) {
	if (isspace(*c))
	    break;
	c++;
    }

    const char *ke = c;

    ret->section_name = strndup(ss, se - ss);
    if (ret->section_name == nullptr)
	return ERR_INTERNAL;
    ret->key = strndup(ks, ke - ks);
    if (ret->key == nullptr) {
	free(ret->section_name);
	return ERR_INTERNAL;
    }

    return ERR_NIL;
}

void RetFree(Retrival *ret)
{
    free(ret->section_name);
    ret->section_name = nullptr;
    free(ret->key);
    ret->key = nullptr;
}


Error parseExpr(const char *str, Expression *expr)
{
    const char *c = str;
    const char *e = str + strlen(str);

    while (c != e) {
	if (!isspace(*c))
	    break;
	c++;
    }

    if (c == e || *c == '.')
	return ERR_ARG_NO_SECT;

    const char *lhs_ss = c;

    while (c != e) {
	if (*c == '.')
	    break;
	if (isalnum(*c) || *c == '-') {
	    c++;
	    continue;
	}

	return ERR_ARG_SECT_BAD_CHAR;
    }

    if (c == e)
	return ERR_ARG_NO_KEY;

    const char *lhs_se = c;
    const char *lhs_ks = ++c;

    if (c == e || isspace(*c))
	return ERR_ARG_NO_KEY;

    while (c != e) {
	if (isspace(*c))
	    break;
	c++;
    }

    const char *lhs_ke = c;

    while (c != e) {
	if (!isspace(*c))
	    break;
	c++;
    }

    if (c == e)
	PANIC;

    // clang-format off
    switch (*c) {
    case '+': expr->op = OP_ADD; break;
    case '-': expr->op = OP_SUB; break;
    case '*': expr->op = OP_MUL; break;
    case '/': expr->op = OP_DIV; break;
    default: return ERR_ARG_BAD_OP;
    }

    c++;
    while (c != e) {
        if (!isspace(*c))
            break;
        c++;
    }

    if (c == e || *c == '.')
        return ERR_ARG_NO_SECT;

    const char *rhs_ss = c;

    while (c != e) {
        if (*c == '.')
            break;
        if (isalnum(*c) || *c == '-') {
            c++;
            continue;
        }
        return ERR_ARG_SECT_BAD_CHAR;
    }

    if (c == e)
        return ERR_ARG_NO_KEY;

    const char *rhs_se = c;
    const char* rhs_ks = ++c;

    if (c == e || isspace(*c))
        return ERR_ARG_NO_KEY;

    while (c != e) {
        if (isspace(*c))
            break;
        c++;
    }

    const char *rhs_ke = c;

    expr->lhs_section_name = nullptr;
    expr->lhs_key = nullptr;
    expr->rhs_section_name = nullptr;
    expr->rhs_key = nullptr;

    expr->lhs_section_name = strndup(lhs_ss, lhs_se - lhs_ss);
    if (expr->lhs_section_name == nullptr)
        goto error;

    expr->lhs_key = strndup(lhs_ks, lhs_ke - lhs_ks);
    if (expr->lhs_key == nullptr)
        goto error;

    expr->rhs_section_name = strndup(rhs_ss, rhs_se - rhs_ss);
    if (expr->rhs_section_name == nullptr)
        goto error;

    expr->rhs_key = strndup(rhs_ks, rhs_ke - rhs_ks);
    if (expr->rhs_key == nullptr)
        goto error;

    return ERR_NIL;

    error:
    free(expr->lhs_section_name );
    free(expr->lhs_key);
    free(expr->rhs_section_name);
    free(expr->rhs_key);
    return ERR_INTERNAL;
}

void ExprFree(Expression *expr)
{
    free(expr->lhs_section_name);
    expr->lhs_section_name = nullptr;
    free(expr->lhs_key);
    expr->lhs_key = nullptr;
    free(expr->rhs_section_name);
    expr->rhs_section_name = nullptr;
    free(expr->rhs_key);
    expr->rhs_key = nullptr;
}

Error RegEvalRet(const Registry *reg, const Retrival *ret)
{
    const Pair *pair = RegGetPair(reg, ret->section_name, ret->key);
    if (pair == nullptr)
        return ERR_ARG_KEY_NOT_FOUND;

    switch (pair->type) {
    case PAIR_TXT:
        printf("%s\n", pair->val_txt);
        break;
    case PAIR_NUM:
        printf("%ld\n", pair->val_num);
        break;
    default:
        PANIC;
    }
    return ERR_NIL;
}

Error RegEvalExpr(const Registry *reg, const Expression *expr)
{
    const Pair *lhs = RegGetPair(reg, expr->lhs_section_name, expr->lhs_key);
    if (lhs == nullptr)
        return ERR_ARG_KEY_NOT_FOUND;

    const Pair *rhs = RegGetPair(reg, expr->rhs_section_name, expr->rhs_key);
    if (rhs == nullptr)
        return ERR_ARG_KEY_NOT_FOUND;

    if (lhs->type != rhs->type)
        return ERR_ARG_TYPE;

    switch (lhs->type) {
    case PAIR_TXT:
        if (expr->op != OP_ADD)
            return ERR_ARG_ILLEGAL_OP;
        printf("%s%s\n", lhs->val_txt, rhs->val_txt);
        break;
    case PAIR_NUM:
        long result;
        switch (expr->op) {
        case OP_ADD: result = lhs->val_num + rhs->val_num; break;
        case OP_SUB: result = lhs->val_num - rhs->val_num; break;
        case OP_MUL: result = lhs->val_num * rhs->val_num; break;
        case OP_DIV:
            if (rhs->val_num == 0)
                return ERR_ARG_DIV_ZERO;
            result = lhs->val_num / rhs->val_num;
            break;
        default:
            PANIC;
        }
        printf("%ld\n", result);
        break;
    default:
        PANIC;
    }

    return ERR_NIL;
}

void processLine(Registry *registry, const Buffer *buffer, size_t line_no)
{
    Statement statement = {};

    Error err = parseStmt(buffer, &statement);

    if (!err)
        err = RegEvalStmt(registry, &statement);


    if (err) {
        printErr(err, line_no);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Missing arguments, use --help for more details.\n");
	return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--help") == 0) {
	printf("Usage: program FILENAME SECTION.KEY\n"
	       "   or: program FILENAME expression \"SECTION1.KEY1 {+-/*} SECTION2.KEY2\"\n"
	       "   or: program --help\n");
	return EXIT_SUCCESS;
    }

    if (argc < 3) {
	fprintf(stderr, "Missing arguments, use --help for more details.\n");
	return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == nullptr) {
	fprintf(stderr, "Cannot open the %s file: %s\n", argv[1], strerror(errno));
	return EXIT_FAILURE;
    }

    Registry registry = {};
    Buffer buffer = {};

    int c;

    size_t line_no = 1;
    while ((c = fgetc(file)) != EOF) {
	if (c != '\n') {
	    if (BufAppend(&buffer, c) != ERR_NIL) {
		fprintf(stderr, "Unexpected internal error.\n");
		return EXIT_FAILURE;
	    }
	    continue;
	}

	processLine(&registry, &buffer, line_no);
	BufClear(&buffer);
	line_no++;
    }

    if (ferror(file)) {
	fprintf(stderr, "Encountered error while reading the file: %s", strerror(errno));
	return EXIT_FAILURE;
    }

    processLine(&registry, &buffer, line_no);

    if (strcmp(argv[2], "expression") == 0) {
	if (argc < 4) {
	    fprintf(stderr, "Missing arguments, use --help for more details.\n");
	    return EXIT_FAILURE;
	}

	Expression expression = {};
	Error err = parseExpr(argv[3], &expression);

        if (!err)
	    err = RegEvalExpr(&registry, &expression);


        if (err) {
            printErr(err, 0);
            return EXIT_FAILURE;
        }

	ExprFree(&expression);
    } else {
	Retrival retrival = {};
	Error err = parseRet(argv[2], &retrival);

        if (!err)
	    err = RegEvalRet(&registry, &retrival);

        if (err) {
            printErr(err, 0);
            return EXIT_FAILURE;
        }

	RetFree(&retrival);
    }

    RegFree(&registry);
    BufFree(&buffer);
    fclose(file);
    return EXIT_SUCCESS;
}
