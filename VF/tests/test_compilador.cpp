/* =====================================================================
 * Testes da VF: executavel unico, sem dependencias externas.
 *   - lexico (alteracoes em relacao a VC)
 *   - tabela la-LR(1): conflitos e cobertura das regras
 *   - sintatico, escopo, tipos: um caso por erro
 *   - geracao de codigo: saida esperada calculada a mao (figuras 7.4 a 7.10)
 *   - execucao do codigo gerado numa maquina virtual de teste (vm.hpp)
 * ===================================================================== */

#include "../parser.hpp"
#include "vm.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace ssl;

static int g_checks = 0, g_failures = 0;

#define CHECK(cond) \
    do { ++g_checks; if (!(cond)) { ++g_failures; \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #cond << "\n"; } } while (0)

#define CHECK_EQ(a, b) \
    do { ++g_checks; const auto va = (a); const auto vb = (b); if (!(va == vb)) { ++g_failures; \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #a << " == " << #b \
                  << "\n  obtido:   " << va << "\n  esperado: " << vb << "\n"; } } while (0)

/* ------------------------------------------------------------------ */

static std::vector<int> g_reductions(R_COUNT, 0);

struct Result {
    std::vector<LexError> lexErrors;
    bool syntaxOk;
    std::vector<Diagnostic> diags;
    std::string text;
};

static Result compile(const std::string &src) {
    static const ParseTable table = buildTable();
    std::istringstream in(src);
    Lexer lex(in);
    std::vector<Tok> toks;
    do {
        lex.nextToken();
        toks.push_back(lex.current());
    } while (lex.token() != Token::END_OF_FILE);

    Result r;
    r.lexErrors = lex.errors();
    Semantics sem(lex);
    r.syntaxOk = parse(table, toks, sem, &g_reductions);
    r.diags = sem.diagnostics();
    r.text = sem.assembly();
    return r;
}

/* programa com uma funcao f(a: integer): integer, variavel x e os comandos dados */
static std::string wrap(const std::string &stmts, const std::string &before = "", const std::string &decls = "") {
    return before + " function f(a: integer): integer { var x: integer; " + decls + stmts + " return x; }";
}

static std::string codeOnly(const std::string &text) {
    std::istringstream in(text);
    std::string line, out;
    while (std::getline(in, line))
        if (!line.empty() && line[0] != ';') out += line + "\n";
    return out;
}

static std::string describe(const Result &r) {
    std::string s;
    for (size_t i = 0; i < r.diags.size(); ++i)
        s += std::to_string(r.diags[i].line) + ":" + std::to_string(r.diags[i].col) + " " + r.diags[i].msg + "; ";
    return s;
}

/* o programa compila sem erro nenhum */
static void ok(const std::string &src) {
    Result r = compile(src);
    ++g_checks;
    if (!r.lexErrors.empty() || !r.syntaxOk || !r.diags.empty()) {
        ++g_failures;
        std::cerr << "FAIL programa deveria compilar: " << src << "\n  " << describe(r) << "\n";
    }
}

/* o programa tem exatamente um erro, com o codigo dado */
static void oneError(const std::string &src, ErrorCode code) {
    Result r = compile(src);
    ++g_checks;
    if (r.diags.size() != 1 || r.diags[0].code != code) {
        ++g_failures;
        std::cerr << "FAIL esperado um unico erro " << code << " em: " << src << "\n  obtido: " << describe(r) << "\n";
    }
}

static void expectCode(const std::string &src, const std::string &expected) {
    Result r = compile(src);
    ++g_checks;
    if (!r.diags.empty()) {
        ++g_failures;
        std::cerr << "FAIL erro ao compilar: " << src << "\n  " << describe(r) << "\n";
        return;
    }
    if (codeOnly(r.text) != expected) {
        ++g_failures;
        std::cerr << "FAIL codigo gerado difere para: " << src << "\n--- obtido\n" << codeOnly(r.text)
                  << "--- esperado\n" << expected;
    }
}

/* ------------------------------------------------------------------ */
/* Lexico                                                              */
/* ------------------------------------------------------------------ */

static void test_lexer() {
    std::istringstream in("return x1 abc2d true false 123\n  'a' \"hi\" ;");
    Lexer lex(in);
    CHECK(lex.nextToken() == Token::RETURN);
    CHECK_EQ(lex.tokenLine(), 1);
    CHECK_EQ(lex.tokenColumn(), 1);
    CHECK(lex.nextToken() == Token::ID);
    CHECK_EQ(lex.lexeme(), std::string("x1"));      /* digitos em identificadores */
    CHECK_EQ(lex.tokenColumn(), 8);
    CHECK(lex.nextToken() == Token::ID);
    CHECK_EQ(lex.lexeme(), std::string("abc2d"));
    CHECK(lex.nextToken() == Token::TRUE_);
    CHECK(lex.getBoolConst(lex.tokenSecundario()));  /* true/false tem token secundario */
    CHECK(lex.nextToken() == Token::FALSE_);
    CHECK(!lex.getBoolConst(lex.tokenSecundario()));
    CHECK(lex.nextToken() == Token::NUMERAL);
    CHECK_EQ(lex.getIntConst(lex.tokenSecundario()), 123);
    CHECK(lex.nextToken() == Token::CHARACTER);
    CHECK_EQ(lex.tokenLine(), 2);                    /* linha do inicio do token */
    CHECK_EQ(lex.tokenColumn(), 3);
    CHECK(lex.nextToken() == Token::STRINGVAL);
    CHECK_EQ(lex.lexeme(), std::string("\"hi\""));
    CHECK(lex.nextToken() == Token::SEMI_COLON);
    CHECK(lex.nextToken() == Token::END_OF_FILE);
    CHECK(lex.errors().empty());

    std::istringstream bad("x = 99999999999 & @ \"abc");
    Lexer l2(bad);
    while (l2.nextToken() != Token::END_OF_FILE) {}
    CHECK_EQ(l2.errors().size(), static_cast<size_t>(4));   /* numeral, '&', '@', string */
    CHECK_EQ(l2.errors()[0].col, 5);

    Result r = compile("function f(a: integer): integer { var x: integer; x = 1 @; return x; }");
    CHECK_EQ(r.lexErrors.size(), static_cast<size_t>(1));
}

/* ------------------------------------------------------------------ */
/* Tabela la-LR(1)                                                     */
/* ------------------------------------------------------------------ */

static void test_table() {
    ParseTable t = buildTable();
    CHECK(t.numStates() > 100);
    CHECK_EQ(t.countConflicts(true), 1);      /* so o else pendente */
    CHECK_EQ(t.countConflicts(false), 0);
    CHECK(t.conflicts.size() == 1 && t.conflicts[0].token == Token::ELSE);
    CHECK_EQ(t.len.size(), static_cast<size_t>(R_COUNT));
    CHECK_EQ(grammar().size(), static_cast<size_t>(R_COUNT));
    CHECK_EQ(std::string(grammar()[R_S_IFELSE].name), std::string("R_S_IFELSE"));
    CHECK_EQ(std::string(grammar()[R_MC].name), std::string("R_MC"));
    CHECK_EQ(t.len[R_DF], 10);
    CHECK_EQ(t.left[R_DF], ntSym(NT_DF));

    /* else pendente: o else casa com o if mais proximo */
    std::string src = wrap("if (a > 0) if (a > 1) x = 1; else x = 2;");
    Result r = compile(src);
    CHECK(r.diags.empty());
    CHECK_EQ(codeOnly(r.text),
             std::string("BEGIN_FUNC 0, 1, 1\n"
                         "\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 0\n\tGT\n\tTJMP_FW L0\n"
                         "\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 1\n\tGT\n\tTJMP_FW L1\n"
                         "\tLOAD_REF 1\n\tLOAD_CONST 2\n\tSTORE_REF 1\n\tJMP_FW L2\nL1:\n"
                         "\tLOAD_REF 1\n\tLOAD_CONST 3\n\tSTORE_REF 1\nL2:\nL0:\n"
                         "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n"));
}

/* ------------------------------------------------------------------ */
/* Sintatico                                                           */
/* ------------------------------------------------------------------ */

static void test_syntax() {
    /* programa vazio nao e uma LDE */
    Result r = compile("");
    CHECK(!r.syntaxOk);
    CHECK_EQ(r.diags.size(), static_cast<size_t>(1));
    CHECK(r.diags[0].code == ERR_SYNTAX);
    CHECK(r.diags[0].msg.find("fim do arquivo") != std::string::npos);

    /* falta ';': linha, coluna, token encontrado e esperados */
    r = compile("function f(a: integer): integer\n{\n  var x: integer;\n  x = 1\n  return x;\n}");
    CHECK(!r.syntaxOk);
    CHECK_EQ(r.diags.size(), static_cast<size_t>(1));
    CHECK_EQ(r.diags[0].line, 5);
    CHECK_EQ(r.diags[0].col, 3);
    CHECK(r.diags[0].msg.find("encontrado 'return'") != std::string::npos);
    CHECK(r.diags[0].msg.find("';'") != std::string::npos);
    CHECK(r.diags[0].msg.find("'+'") != std::string::npos);

    /* bloco sem declaracao (gramatica oficial: B -> { LDV LS }) */
    r = compile("function f(a: integer): integer { x = 1; return x; }");
    CHECK(!r.syntaxOk);
    CHECK(r.diags.back().msg.find("'var'") != std::string::npos);

    /* declaracao global de variavel nao existe (DE -> DF | DT) */
    CHECK(!compile("var x: integer;").syntaxOk);
    /* chamada sem argumentos e funcao sem parametros nao existem (LE, LP nao vazios) */
    CHECK(!compile("function g(): integer { var x: integer; return x; }").syntaxOk);
    /* expressao nao e comando */
    CHECK(!compile(wrap("x + 1;")).syntaxOk);
    CHECK(!compile(wrap("a++;")).syntaxOk);
    /* bloco vazio */
    CHECK(!compile(wrap("{ var y: integer; }")).syntaxOk);

    /* um erro sintatico para a analise, mas os erros anteriores sao mantidos */
    r = compile("function f(a: integer): integer { var x: integer; x = z; x = ; return x; }");
    CHECK(!r.syntaxOk);
    CHECK_EQ(r.diags.size(), static_cast<size_t>(2));
    CHECK(r.diags[0].code == ERR_NOT_DECL);
    CHECK(r.diags[1].code == ERR_SYNTAX);
}

/* ------------------------------------------------------------------ */
/* Escopo                                                              */
/* ------------------------------------------------------------------ */

static void test_scope() {
    oneError(wrap("x = 1;", "", "var y, y: integer;"), ERR_REDECL);
    oneError("function f(a: integer, a: integer): integer { var x: integer; x = 1; return x; }", ERR_REDECL);
    oneError("function f(a: integer): integer { var a: integer; a = 1; return a; }", ERR_REDECL);  /* param e local: mesmo nivel */
    oneError("function f(a: integer): integer { var x: integer; x = 1; return x; }"
             "function f(b: integer): integer { var x: integer; x = 1; return x; }", ERR_REDECL);
    oneError("type t = integer type t = char", ERR_REDECL);
    oneError("type s = struct { a: integer; a: char }", ERR_REDECL);
    oneError(wrap("x = zz;"), ERR_NOT_DECL);
    oneError(wrap("x = zz + zz * zz;"), ERR_NOT_DECL);                 /* so a primeira ocorrencia e reportada */
    oneError(wrap("x = g(1);"), ERR_NOT_DECL);                         /* funcao nao declarada (sem erro de tipo em cascata) */
    oneError(wrap("x = 1;", "", "var y: tipoInexistente;"), ERR_NOT_DECL);
    oneError("function f(a: integer): integer { var x: integer; x = 1; return x; }"
             "function g(a: integer): integer { var y: integer; y = x; return y; }", ERR_NOT_DECL);  /* locais nao vazam */

    /* ok: sombreamento em bloco, recursao, mesmo nome de campo em structs, nome de parametro igual em funcoes */
    ok(wrap("{ var x: char; x = 'a'; } if (false) x = 1;"));
    ok("function f(a: integer): integer { var x: integer; x = 1; if (a > 0) x = f(a - 1); return x; }");
    ok("type p = struct { x: integer; y: integer } type q = struct { x: char; y: char }"
       "function f(a: p): integer { var x: integer; x = a.x; return x; }");
    ok(wrap("{ var y: integer; y = 1; } { var y: char; y = 'c'; }"));
    ok("function f(a: integer): integer { var x: integer; x = 1; return x; }"
       "function g(a: integer): integer { var x: integer; x = f(a); return x; }");
    /* variavel de bloco interno some ao sair do bloco */
    oneError(wrap("{ var y: integer; y = 1; } y = 2;"), ERR_NOT_DECL);
    /* campo nao e procurado na tabela de simbolos (so no tipo) */
    oneError(wrap("x = p.nada;", "type pt = struct { a: integer }", "var p: pt;"), ERR_FIELD_NOT_DECL);
}

/* ------------------------------------------------------------------ */
/* Tipos                                                               */
/* ------------------------------------------------------------------ */

static void test_types() {
    const std::string types = "type vec = array [3] of integer type pt = struct { a: integer; b: char }";

    oneError(wrap("x = 1;", "", "var y: x;"), ERR_TYPE_EXPECTED);
    oneError(wrap("x = 1;", "", "var y: f;"), ERR_TYPE_EXPECTED);
    oneError(wrap("if (1) x = 1;"), ERR_BOOL_TYPE_EXPECTED);
    oneError(wrap("if (a > 1) x = 1; else x = 2; while (\"s\") x = 1;"), ERR_BOOL_TYPE_EXPECTED);
    oneError(wrap("do x = 1; while (x);"), ERR_BOOL_TYPE_EXPECTED);
    oneError(wrap("if (x && true) x = 1;"), ERR_BOOL_TYPE_EXPECTED);
    oneError(wrap("if (true || 'c') x = 1;"), ERR_BOOL_TYPE_EXPECTED);
    oneError(wrap("x = \"a\";"), ERR_TYPE_MISMATCH);
    oneError(wrap("x = 1 + \"a\";"), ERR_TYPE_MISMATCH);
    oneError(wrap("if (x == 'a') x = 1;"), ERR_TYPE_MISMATCH);
    oneError(wrap("x = x * 'a';"), ERR_TYPE_MISMATCH);
    oneError(wrap("x = -\"a\";"), ERR_INVALID_TYPE);
    oneError(wrap("if (!x) x = 1;"), ERR_INVALID_TYPE);
    oneError(wrap("x = true + true;"), ERR_INVALID_TYPE);
    oneError(wrap("x = 'a' - 'b';"), ERR_INVALID_TYPE);
    oneError(wrap("x = ++y;", "", "var y: boolean;"), ERR_INVALID_TYPE);
    oneError(wrap("if (v == v) x = 1;", types, "var v: vec;"), ERR_INVALID_TYPE);   /* relacional so em escalares */
    oneError(wrap("x = x.a;"), ERR_KIND_NOT_STRUCT);
    oneError(wrap("x = p.c;", types, "var p: pt;"), ERR_FIELD_NOT_DECL);
    oneError(wrap("x = x[1];"), ERR_KIND_NOT_ARRAY);
    oneError(wrap("x = v[\"s\"];", types, "var v: vec;"), ERR_INVALID_INDEX_TYPE);
    oneError(wrap("f = 1;"), ERR_KIND_NOT_VAR);
    oneError(wrap("x = vec;", types), ERR_KIND_NOT_VAR);
    oneError(wrap("x = x(1);"), ERR_KIND_NOT_FUNC);
    oneError(wrap("x = f(1, 2);"), ERR_TOO_MANY_ARGS);
    oneError("function g(a: integer, b: integer): integer { var x: integer; x = 1; return x; }"
             "function f(a: integer): integer { var x: integer; x = g(1); return x; }", ERR_TOO_FEW_ARGS);
    oneError(wrap("x = f('c');"), ERR_PARAM_TYPE);
    oneError("function f(a: integer): integer { var x: integer; x = 1; return \"s\"; }", ERR_RETURN_TYPE);
    oneError(wrap("break;"), ERR_BREAK_OUTSIDE);
    oneError(wrap("continue;"), ERR_CONTINUE_OUTSIDE);
    oneError(wrap("if (a > 0) break;"), ERR_BREAK_OUTSIDE);

    /* erros em tipos de declaracao usam o tipo universal: sem erros em cascata */
    oneError(wrap("y = 1; y = 'c'; x = y + y;", "", "var y: naoExiste;"), ERR_NOT_DECL);
    oneError(wrap("y.k = 1; y[3] = 2; x = y;", "", "var y: x;"), ERR_TYPE_EXPECTED);
    oneError(wrap("x = 1;", "type t = array [2] of naoExiste"), ERR_NOT_DECL);
    oneError(wrap("x = zz(1, 2, 3);"), ERR_NOT_DECL);                   /* chamada de nome nao declarado */

    /* acumula erros sem parar no primeiro; nenhum codigo e necessario */
    Result r = compile(wrap("x = \"a\"; x = zz; if (x) x = 1; break;"));
    CHECK_EQ(r.diags.size(), static_cast<size_t>(4));
    CHECK(r.diags[0].code == ERR_TYPE_MISMATCH);
    CHECK(r.diags[1].code == ERR_NOT_DECL);
    CHECK(r.diags[2].code == ERR_BOOL_TYPE_EXPECTED);
    CHECK(r.diags[3].code == ERR_BREAK_OUTSIDE);
    CHECK_EQ(std::string(r.diags[0].phase()), std::string("tipo"));
    CHECK_EQ(std::string(r.diags[1].phase()), std::string("escopo"));

    /* compatibilidade por nome, com alias transparente */
    ok("type a = integer type b = a function f(p: a): b { var x: b; x = p + 1; return x; }");
    ok(wrap("p = q; x = 1;", "type pt = struct { a: integer } type pt2 = pt", "var p: pt; var q: pt2;"));
    oneError(wrap("v = w; x = 1;", "type v1 = array [3] of integer type v2 = array [3] of integer", "var v: v1; var w: v2;"),
             ERR_TYPE_MISMATCH);
    ok(wrap("v = w; x = 1;", "type v1 = array [3] of integer", "var v: v1; var w: v1;"));
    ok(wrap("s = s + \"b\"; if (s == \"ab\") x = 1;", "", "var s: string;"));
    ok(wrap("c = s[1]; s[0] = c; x = 1;", "", "var s: string; var c: char;"));
    oneError(wrap("s[0] = 1;", "", "var s: string;"), ERR_TYPE_MISMATCH);
    ok("function g(p: integer, q: char, r: string): boolean { var b: boolean; b = true; return b; }"
       "function f(a: integer): integer { var x: integer; if (g(1, 'c', \"s\")) x = 1; return x; }");
}

/* ------------------------------------------------------------------ */
/* Geracao de codigo (esperado calculado a mao)                        */
/* ------------------------------------------------------------------ */

static void test_codegen() {
    /* figura 7.4 + 7.8: atribuicao e retorno */
    expectCode("function f(a: integer): integer { var x: integer; x = a + 1; return x; }",
               "BEGIN_FUNC 0, 1, 1\n"
               "\tLOAD_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 0\n\tADD\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.5: if */
    expectCode(wrap("x = 0; if (a > 0) x = 1;"),
               "BEGIN_FUNC 0, 1, 1\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 0\n\tSTORE_REF 1\n"
               "\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 1\n\tGT\n\tTJMP_FW L0\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 2\n\tSTORE_REF 1\nL0:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.5: if-else */
    expectCode(wrap("if (a != 0) x = 1; else x = 2;"),
               "BEGIN_FUNC 0, 1, 1\n"
               "\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 0\n\tNEQ\n\tTJMP_FW L0\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 1\n\tSTORE_REF 1\n\tJMP_FW L1\nL0:\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 2\n\tSTORE_REF 1\nL1:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.5: while, com break e continue */
    expectCode(wrap("while (x < a) { var t: integer; x = x + 1; if (x == 2) continue; if (x == 5) break; }"),
               "BEGIN_FUNC 0, 1, 2\n"
               "L0:\n\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLT\n\tTJMP_FW L1\n"
               "\tLOAD_REF 1\n\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_CONST 0\n\tADD\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_CONST 1\n\tEQ\n\tTJMP_FW L2\n\tJMP_BW L0\nL2:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_CONST 2\n\tEQ\n\tTJMP_FW L3\n\tJMP_FW L1\nL3:\n"
               "\tJMP_BW L0\nL1:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.5: do-while (NOT + TJMP_BW) */
    expectCode(wrap("x = 0; do x = x + 1; while (x < a);"),
               "BEGIN_FUNC 0, 1, 1\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 0\n\tSTORE_REF 1\n"
               "L0:\n\tLOAD_REF 1\n\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_CONST 1\n\tADD\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLT\n\tNOT\n\tTJMP_BW L0\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* do-while: break vai para depois do laco, continue (JMP_FW) para a condicao */
    expectCode(wrap("do { var t: integer; if (x == 2) continue; if (x > a) break; } while (x < a);"),
               "BEGIN_FUNC 0, 1, 2\n"
               "L0:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_CONST 0\n\tEQ\n\tTJMP_FW L1\n\tJMP_FW L2\nL1:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tGT\n\tTJMP_FW L3\n\tJMP_FW L4\nL3:\n"
               "L2:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLT\n\tNOT\n\tTJMP_BW L0\nL4:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.6/7.7: operadores logicos, relacionais, unarios e constantes booleanas */
    expectCode("function f(a: integer): boolean { var b: boolean; b = !(a < 1) && true || -a >= 2; return b; }",
               "BEGIN_FUNC 0, 1, 1\n"
               "\tLOAD_REF 1\n"
               "\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 0\n\tLT\n\tNOT\n\tLOAD_CONST 1\n\tAND\n"
               "\tLOAD_REF 0\n\tDE_REF 1\n\tNEG\n\tLOAD_CONST 2\n\tGE\n\tOR\n"
               "\tSTORE_REF 1\n\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.8: ++/-- pre e pos */
    expectCode("function f(a: integer): integer { var x: integer; x = ++a; x = a--; return x; }",
               "BEGIN_FUNC 0, 1, 1\n"
               "\tLOAD_REF 1\n\tLOAD_REF 0\n\tDUP\n\tDUP\n\tDE_REF 1\n\tINC\n\tSTORE_REF 1\n\tDE_REF 1\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tLOAD_REF 0\n\tDUP\n\tDUP\n\tDE_REF 1\n\tDEC\n\tSTORE_REF 1\n\tDE_REF 1\n\tINC\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* figura 7.10: chamada de funcao (CALL n, n = ordem de declaracao) */
    expectCode("function g(p: integer, q: integer): integer { var r: integer; r = p - q; return r; }"
               "function f(a: integer): integer { var x: integer; x = g(a, 7) * 2; return x; }",
               "BEGIN_FUNC 0, 2, 1\n"
               "\tLOAD_REF 2\n\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_REF 1\n\tDE_REF 1\n\tSUB\n\tSTORE_REF 1\n"
               "\tLOAD_REF 2\n\tDE_REF 1\n\tRET\nEND_FUNC\n"
               "BEGIN_FUNC 1, 1, 1\n"
               "\tLOAD_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLOAD_CONST 0\n\tCALL 0\n\tLOAD_CONST 1\n\tMUL\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* tamanhos e enderecos: arrays, structs, campos, strings via CALL_LIB;
     * v: 0..2, p: 3..4, s: 5, locais i: 6 e c: 7 */
    expectCode("type vec = array [3] of integer type pt = struct { x: integer; y: integer }"
               "function h(v: vec, p: pt, s: string): integer"
               "{ var i: integer; var c: char; i = 1; v[i] = p.y; c = s[0]; s[1] = c; return v[2]; }",
               "BEGIN_FUNC 0, 6, 2\n"
               "\tLOAD_REF 6\n\tLOAD_CONST 1\n\tSTORE_REF 1\n"
               "\tLOAD_REF 0\n\tLOAD_REF 6\n\tDE_REF 1\n\tMUL 1\n\tADD\n"
               "\tLOAD_REF 3\n\tADD 1\n\tDE_REF 1\n\tSTORE_REF 1\n"
               "\tLOAD_REF 7\n\tLOAD_REF 5\n\tLOAD_CONST 2\n\tCALL_LIB 0, 0\n\tSTORE_REF 1\n"
               "\tLOAD_REF 5\n\tLOAD_CONST 3\n\tLOAD_REF 7\n\tDE_REF 1\n\tCALL_LIB 0, 1\n\tPOP\n"
               "\tLOAD_REF 0\n\tLOAD_CONST 4\n\tMUL 1\n\tADD\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* atribuicao e passagem de agregados: DE_REF/STORE_REF com o tamanho do tipo */
    expectCode("type pt = struct { x: integer; y: integer } type m = array [2] of pt "
               "function h(a: m, q: pt): pt { var r: pt; a[1] = q; r = a[0]; return r; }",
               "BEGIN_FUNC 0, 6, 2\n"
               "\tLOAD_REF 0\n\tLOAD_CONST 1\n\tMUL 2\n\tADD\n\tLOAD_REF 4\n\tDE_REF 2\n\tSTORE_REF 2\n"
               "\tLOAD_REF 6\n\tLOAD_REF 0\n\tLOAD_CONST 2\n\tMUL 2\n\tADD\n\tDE_REF 2\n\tSTORE_REF 2\n"
               "\tLOAD_REF 6\n\tDE_REF 2\n\tRET\nEND_FUNC\n");

    /* numeracao continua das variaveis entre blocos da mesma funcao */
    expectCode(wrap("{ var y: integer; y = 1; } { var z, w: integer; w = 2; }"),
               "BEGIN_FUNC 0, 1, 4\n"
               "\tLOAD_REF 2\n\tLOAD_CONST 0\n\tSTORE_REF 1\n"
               "\tLOAD_REF 4\n\tLOAD_CONST 1\n\tSTORE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\nEND_FUNC\n");

    /* o pool de constantes sai como comentario no cabecalho */
    Result r = compile("function f(a: char): string { var s: string; s = \"oi\"; a = 'z'; return s; }");
    CHECK(r.diags.empty());
    CHECK(r.text.find(";   0: string \"oi\"\n;   1: char 'z'\n") != std::string::npos);
}

/* ------------------------------------------------------------------ */
/* Execucao do codigo gerado                                           */
/* ------------------------------------------------------------------ */

static Cell num(long n) { Cell c; c.n = n; return c; }
static Cell str(const std::string &s) { Cell c; c.isStr = true; c.s = s; return c; }

static long run(VM &vm, int f, std::vector<Cell> args) {
    Cell res;
    bool done = vm.call(f, args, res);
    if (!done) std::cerr << "VM: " << vm.error << "\n";
    CHECK(done);
    return res.n;
}

static void test_execution() {
    /* exemplos 7.4.1 do livro, reescritos para a gramatica (corpo de laco sem bloco) */
    Result r = compile(
        "function fatorial(n: integer): integer"
        "{ var i, f: integer; i = 2; f = 1; while (i <= n) f = f * i++; return f; }"
        "function somaFat(n: integer): integer"
        "{ var i, s: integer; i = 0; s = 0; while (++i <= n) s = s + fatorial(i); return s; }"
        "function fib(n: integer): integer"
        "{ var r: integer; if (n < 2) r = n; else r = fib(n - 1) + fib(n - 2); return r; }");
    CHECK(r.diags.empty());
    VM vm;
    vm.load(r.text);
    CHECK_EQ(run(vm, 0, std::vector<Cell>(1, num(5))), 120L);
    CHECK_EQ(run(vm, 1, std::vector<Cell>(1, num(4))), 33L);     /* 1 + 2 + 6 + 24 */
    CHECK_EQ(run(vm, 2, std::vector<Cell>(1, num(10))), 55L);

    /* pre/pos incremento, aritmetica, unarios e logicos */
    r = compile(
        "function inc(n: integer): integer { var a, b: integer; a = n; b = a++ + ++a; return a * 100 + b; }"
        "function ar(x: integer, y: integer): integer { var z: integer; z = -x * (y - 3) / 2 + 10; return z; }"
        "function lg(x: integer): boolean { var b: boolean; b = !(x < 3) && x != 7 || x == 100; return b; }"
        "function dec(n: integer): integer { var a, b: integer; a = n; b = --a - a--; return a * 100 + b; }");
    CHECK(r.diags.empty());
    VM v2;
    v2.load(r.text);
    CHECK_EQ(run(v2, 0, std::vector<Cell>(1, num(1))), 304L);
    std::vector<Cell> a2; a2.push_back(num(4)); a2.push_back(num(7));
    CHECK_EQ(run(v2, 1, a2), 2L);                                 /* ((-4) * 4) / 2 + 10 */
    CHECK_EQ(run(v2, 2, std::vector<Cell>(1, num(5))), 1L);
    CHECK_EQ(run(v2, 2, std::vector<Cell>(1, num(7))), 0L);
    CHECK_EQ(run(v2, 2, std::vector<Cell>(1, num(2))), 0L);
    CHECK_EQ(run(v2, 2, std::vector<Cell>(1, num(100))), 1L);
    CHECK_EQ(run(v2, 3, std::vector<Cell>(1, num(5))), 300L);     /* --a vale 4, a-- vale 4 e deixa a = 3: b = 0 */

    /* break / continue em while e do-while; if aninhado com else pendente */
    r = compile(
        "function soma(n: integer): integer"
        "{ var i, s: integer; i = 0; s = 0;"
        "  while (true) { var t: integer; i = i + 1; if (i > n) break; if (i / 2 * 2 == i) continue; s = s + i; }"
        "  return s; }"
        "function dw(n: integer): integer"
        "{ var i, s: integer; i = 0; s = 0;"
        "  do { var t: integer; i = i + 1; if (i == 3) continue; if (i > n) break; s = s + i; } while (i < 100);"
        "  return s; }"
        "function dw2(n: integer): integer"
        "{ var i: integer; i = 0; do i = i + 3; while (i < n); return i; }"
        "function el(n: integer): integer"
        "{ var r: integer; r = 0; if (n > 0) if (n > 10) r = 2; else r = 1; return r; }");
    CHECK(r.diags.empty());
    VM v3;
    v3.load(r.text);
    CHECK_EQ(run(v3, 0, std::vector<Cell>(1, num(9))), 25L);      /* 1+3+5+7+9 */
    CHECK_EQ(run(v3, 1, std::vector<Cell>(1, num(5))), 1L + 2 + 4 + 5);
    CHECK_EQ(run(v3, 2, std::vector<Cell>(1, num(10))), 12L);
    CHECK_EQ(run(v3, 3, std::vector<Cell>(1, num(20))), 2L);
    CHECK_EQ(run(v3, 3, std::vector<Cell>(1, num(5))), 1L);
    CHECK_EQ(run(v3, 3, std::vector<Cell>(1, num(-5))), 0L);

    /* arrays e structs: ordenacao (7.4.4), soma de array, troca de campos */
    r = compile(
        "type arr = array [5] of integer "
        "type pt = struct { x: integer; y: integer } "
        "type par = struct { p: pt; q: pt; k: integer } "
        "function sort(a: arr, n: integer): arr"
        "{ var i, j, v: integer;"
        "  i = 0;"
        "  while (i < n) {"
        "    var dummy: integer;"
        "    j = i + 1;"
        "    while (j < n) {"
        "      var dummy2: integer;"
        "      if (a[i] > a[j]) { var dummy3: integer; v = a[i]; a[i] = a[j]; a[j] = v; }"
        "      j = j + 1;"
        "    }"
        "    i = i + 1;"
        "  }"
        "  return a; }"
        "function soma(a: arr): integer { var i, s: integer; i = 0; s = 0; while (i < 5) s = s + a[i++]; return s; }"
        "function troca(p: pt): integer { var t: integer; t = p.x; p.x = p.y; p.y = t; return p.x * 10 + p.y; }"
        "function aninhado(z: par): integer { var w: integer; z.q.y = 7; z.p = z.q; z.k = z.p.y + z.q.y; return z.k + z.p.x; }");
    CHECK(r.diags.empty());
    VM v4;
    v4.load(r.text);
    std::vector<Cell> in5;
    int vals[5] = {5, 3, 9, 1, 4};
    for (int i = 0; i < 5; ++i) in5.push_back(num(vals[i]));
    Cell res;
    std::vector<Cell> args = in5;
    args.push_back(num(5));
    CHECK(v4.call(0, args, res, 5));
    CHECK_EQ(v4.lastResult.size(), static_cast<size_t>(5));
    long sorted[5] = {1, 3, 4, 5, 9};
    for (int i = 0; i < 5; ++i) CHECK_EQ(v4.lastResult[static_cast<size_t>(i)].n, sorted[i]);
    CHECK_EQ(run(v4, 1, in5), 22L);
    std::vector<Cell> pt;
    pt.push_back(num(1)); pt.push_back(num(2));
    CHECK_EQ(run(v4, 2, pt), 21L);
    std::vector<Cell> z(5, num(0));
    CHECK_EQ(run(v4, 3, z), 14L);                                  /* p = q = (0,7), k = 14, p.x = 0 */

    /* strings: leitura, escrita (CALL_LIB) e concatenacao */
    r = compile(
        "function conta(s: string, n: integer, c: char): integer"
        "{ var i, k: integer; i = 0; k = 0; while (i < n) { var d: integer; if (s[i] == c) k = k + 1; i = i + 1; } return k; }"
        "function inverte(s: string, n: integer): string"
        "{ var i, j: integer; var c: char; i = 0; j = n - 1;"
        "  while (i < j) { var d: integer; c = s[i]; s[i++] = s[j]; s[j--] = c; }"
        "  return s; }"
        "function junta(a: string, b: string): string { var r: string; r = a + \"-\" + b; return r; }");
    CHECK(r.diags.empty());
    VM v5;
    v5.load(r.text);
    std::vector<Cell> sa;
    sa.push_back(str("banana")); sa.push_back(num(6)); sa.push_back(num('a'));
    CHECK_EQ(run(v5, 0, sa), 3L);
    std::vector<Cell> sb;
    sb.push_back(str("compilador")); sb.push_back(num(10));
    CHECK(v5.call(1, sb, res));
    CHECK_EQ(res.s, std::string("rodalipmoc"));
    std::vector<Cell> sc;
    sc.push_back(str("ab")); sc.push_back(str("cd"));
    CHECK(v5.call(2, sc, res));
    CHECK_EQ(res.s, std::string("ab-cd"));
}

/* ------------------------------------------------------------------ */
/* Exemplos .ssl (executar a partir de compilador/)                    */
/* ------------------------------------------------------------------ */

static bool readFile(const char *path, std::string &out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static std::vector<Cell> args2(Cell a, Cell b) { std::vector<Cell> v; v.push_back(a); v.push_back(b); return v; }

static void test_examples() {
    std::string src;
    if (!readFile("exemplos/lacos.ssl", src)) { std::cerr << "(exemplos/ nao encontrado: teste pulado)\n"; return; }
    Result r = compile(src);
    CHECK(r.diags.empty());
    VM lacos;
    lacos.load(r.text);
    CHECK_EQ(run(lacos, 0, std::vector<Cell>(1, num(9))), 25L);          /* somaImpares */
    CHECK_EQ(run(lacos, 1, std::vector<Cell>(1, num(20))), 3L);          /* multiplo3 */
    CHECK_EQ(run(lacos, 2, args2(num(3), num(8))), 8L);                  /* maximo */
    CHECK_EQ(run(lacos, 3, std::vector<Cell>(1, num(12))), 144L);        /* fib */

    CHECK(readFile("exemplos/fatorial.ssl", src));
    r = compile(src);
    CHECK(r.diags.empty());
    VM fat;
    fat.load(r.text);
    CHECK_EQ(run(fat, 0, std::vector<Cell>(1, num(6))), 720L);

    CHECK(readFile("exemplos/strings.ssl", src));
    r = compile(src);
    CHECK(r.diags.empty());
    VM s;
    s.load(r.text);
    std::vector<Cell> a;
    a.push_back(str("abcd")); a.push_back(str("abxd")); a.push_back(num(4));
    CHECK_EQ(run(s, 0, a), -1L);
    a[1] = str("abcd");
    CHECK_EQ(run(s, 0, a), 0L);
    a[0] = str("abzd");
    CHECK_EQ(run(s, 0, a), 1L);
    Cell res;
    CHECK(s.call(1, args2(str("sintatico"), num(9)), res));
    CHECK_EQ(res.s, std::string("ocitatnis"));

    CHECK(readFile("exemplos/ordenacao.ssl", src));
    r = compile(src);
    CHECK(r.diags.empty());
    VM o;
    o.load(r.text);
    std::vector<Cell> arr, in2;
    long vals[10] = {9, 8, 7, 1, 2, 3, 6, 0, 5, 4};
    for (int i = 0; i < 10; ++i) arr.push_back(num(vals[i]));
    in2 = arr;
    arr.push_back(num(10));
    CHECK(o.call(0, arr, res, 10));
    for (int i = 0; i < 10; ++i) CHECK_EQ(o.lastResult[static_cast<size_t>(i)].n, static_cast<long>(i));
    CHECK_EQ(run(o, 1, in2), 45L);

    CHECK(readFile("exemplos/structs.ssl", src));
    r = compile(src);
    CHECK(r.diags.empty());
    VM st;
    st.load(r.text);
    /* retangulo = origem(2) + tamanho(2) + nome(1) = 5 celulas */
    std::vector<Cell> ret(5, num(0));
    ret[2] = num(3); ret[3] = num(4);
    CHECK_EQ(run(st, 0, ret), 12L);                                       /* area */
    std::vector<Cell> pts;
    for (int i = 0; i < 4; ++i) { pts.push_back(num(i + 1)); pts.push_back(num(100)); }
    CHECK_EQ(run(st, 2, pts), 10L);                                       /* somaX */
}

/* ------------------------------------------------------------------ */
/* Cobertura: toda regra da gramatica foi reduzida por algum teste       */
/* ------------------------------------------------------------------ */

static void test_rule_coverage() {
    for (int i = 0; i < R_COUNT; ++i) {
        ++g_checks;
        if (g_reductions[static_cast<size_t>(i)] == 0) {
            ++g_failures;
            std::cerr << "FAIL regra nunca exercitada: " << grammar()[static_cast<size_t>(i)].name << " ("
                      << ruleText(i) << ")\n";
        }
    }
}

int main() {
    test_lexer();
    test_table();
    test_syntax();
    test_scope();
    test_types();
    test_codegen();
    test_execution();
    test_examples();
    test_rule_coverage();

    std::cout << g_checks << " verificacoes, " << g_failures << " falhas\n";
    return g_failures == 0 ? 0 : 1;
}
