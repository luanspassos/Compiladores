/* =====================================================================
 * semantics.hpp
 *
 * Gramatica de atributos (cap. 4.1): pilha semantica paralela a pilha
 * sintatica e funcao Semantics(rule), chamada a cada reducao (figura 4.6).
 * Implementa, na mesma passada, a analise de escopo (cap. 5), a analise
 * de tipos (cap. 6) e a geracao de codigo (cap. 7).
 * ===================================================================== */

#ifndef SSL_SEMANTICS_HPP
#define SSL_SEMANTICS_HPP

#include "grammar.hpp"
#include "symtab.hpp"

#include <string>
#include <vector>

namespace ssl {

enum ErrorCode {
    ERR_LEXICAL, ERR_SYNTAX,
    /* escopo */
    ERR_REDECL, ERR_NOT_DECL,
    /* tipos */
    ERR_TYPE_EXPECTED, ERR_BOOL_TYPE_EXPECTED, ERR_TYPE_MISMATCH,
    ERR_INVALID_TYPE, ERR_KIND_NOT_STRUCT, ERR_FIELD_NOT_DECL,
    ERR_KIND_NOT_ARRAY, ERR_INVALID_INDEX_TYPE, ERR_KIND_NOT_VAR,
    ERR_KIND_NOT_FUNC, ERR_TOO_MANY_ARGS, ERR_TOO_FEW_ARGS, ERR_PARAM_TYPE,
    ERR_RETURN_TYPE, ERR_BREAK_OUTSIDE, ERR_CONTINUE_OUTSIDE
};

struct Diagnostic {
    ErrorCode code;
    int line, col;
    std::string msg;
    const char *phase() const;   /* "lexico", "sintatico", "escopo" ou "tipo" */
};

/* No da pilha semantica: atributos de um nao terminal. Todo nao terminal
 * (inclusive marcadores) empilha um no, para que as distancias ate o topo
 * nao mudem (convencao recomendada na secao 4.1). */
struct Attr {
    Object *obj = nullptr;          /* IDD, IDU */
    int name = -1;                  /* IDD, IDU, ID */
    Object *type = nullptr;         /* T, E, L, R, Y, F, LV, literais, MC */
    int size = 0;                   /* T, LP, DC */
    std::vector<Object *> list;     /* LI, LP, DC */
    int val = 0, pos = -1;          /* NUM / constantes: valor e indice no pool */
    int label = -1;                 /* MT, ME, MW, MD */
    int offset = -1;                /* MF: posicao do BEGIN_FUNC no codigo */
    Object *func = nullptr;         /* MC, LE: funcao chamada */
    int n = 0;                      /* LE: argumentos ja vistos */
    bool err = false;               /* MC, LE */
    bool strChar = false;           /* LV: caractere de string (usa CALL_LIB) */
};

class Semantics {
public:
    explicit Semantics(const Lexer &lex);

    void shifted(const Tok &t) { last_ = t; }   /* ultimo terminal empilhado */
    void run(int rule);                          /* Semantics(r) */

    void addError(ErrorCode code, int line, int col, const std::string &msg);
    const std::vector<Diagnostic> &diagnostics() const { return diags_; }
    bool hasErrors() const { return !diags_.empty(); }

    /* Codigo gerado (so e valido se !hasErrors(), equivale ao ERROR_FLAG) */
    std::string assembly() const;

private:
    struct Loop { int brk, cont; bool isDo; };

    /* pilha semantica */
    void push(const Attr &a) { sem_.push_back(a); }
    void pop(int n) { sem_.resize(sem_.size() - static_cast<size_t>(n)); }
    const Attr &top(int k) const { return sem_[sem_.size() - 1 - static_cast<size_t>(k)]; }

    /* tipos */
    Object *base(Object *t) const;
    bool check(Object *a, Object *b) const;           /* CheckTypes */
    bool isScalar(Object *t) const;
    bool checkBool(Object *t);
    void error(ErrorCode code, const std::string &detail = std::string());

    /* codigo */
    void emit(const std::string &instr) { code_.push_back("\t" + instr); }
    void emit(const std::string &instr, int n);
    void label(int l);
    int newLabel() { return labelNo_++; }

    void incDec(Attr &res, bool pre, bool inc);
    void binary(Attr &res, Rule rule);
    void checkArg(Attr &le, Object *argType);

    const Lexer &lex_;
    Tok last_;
    SymbolTable st_;
    std::vector<Attr> sem_;
    std::vector<Diagnostic> diags_;
    std::vector<std::string> code_;
    std::vector<Loop> loops_;
    Object *cur_ = nullptr;       /* funcao corrente (curFunction) */
    int nFuncs_ = 0;
    int labelNo_ = 0;
};

} // namespace ssl

#endif // SSL_SEMANTICS_HPP
