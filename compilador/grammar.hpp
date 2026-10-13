/* =====================================================================
 * grammar.hpp
 *
 * Gramatica efetivamente usada pelo compilador (gramatica oficial da SSL
 * + marcadores semanticos + comando 'return'). As regras sao declaradas
 * uma unica vez, na lista SSL_RULES, de onde saem o enum Rule (usado no
 * 'switch' de Semantics) e a tabela de producoes (usada para construir
 * a tabela la-LR(1)).
 * ===================================================================== */

#ifndef SSL_GRAMMAR_HPP
#define SSL_GRAMMAR_HPP

#include "lexer.hpp"

#include <string>
#include <vector>

namespace ssl {

/* Nao terminais. START e o simbolo inicial aumentado; os de NB em diante
 * sao os marcadores semanticos (regras epsilon). */
enum Nt {
    NT_START, NT_P, NT_LDE, NT_DE, NT_T, NT_DT, NT_DC, NT_DF, NT_LP, NT_B,
    NT_LDV, NT_LS, NT_DV, NT_LI, NT_S, NT_E, NT_L, NT_R, NT_Y, NT_F, NT_LE,
    NT_LV, NT_IDD, NT_IDU, NT_ID, NT_TRUE, NT_FALSE, NT_CHR, NT_STR, NT_NUM,
    NT_NB, NT_NF, NT_MF, NT_MT, NT_ME, NT_MW, NT_MJ, NT_MD, NT_MK, NT_MC,
    NT_COUNT
};

/* Simbolo: terminal = valor do enum Token; nao terminal = kNumTokens + Nt */
typedef int Sym;

inline bool isTerminal(Sym s) { return s < kNumTokens; }
inline Sym ntSym(Nt n) { return kNumTokens + static_cast<int>(n); }
inline Nt symNt(Sym s) { return static_cast<Nt>(s - kNumTokens); }
inline int numSyms() { return kNumTokens + static_cast<int>(NT_COUNT); }

#define TK(x) static_cast<ssl::Sym>(ssl::Token::x)
#define NN(x) static_cast<ssl::Sym>(ssl::kNumTokens + ssl::NT_##x)
#define EPS

/* X(nome da regra, lado esquerdo, lado direito...) */
#define SSL_RULES(X) \
    X(R_START,      START, NN(P)) \
    X(R_P,          P,     NN(LDE)) \
    X(R_LDE_REC,    LDE,   NN(LDE), NN(DE)) \
    X(R_LDE,        LDE,   NN(DE)) \
    X(R_DE_DF,      DE,    NN(DF)) \
    X(R_DE_DT,      DE,    NN(DT)) \
    X(R_T_INTEGER,  T,     TK(INTEGER)) \
    X(R_T_CHAR,     T,     TK(CHAR)) \
    X(R_T_BOOLEAN,  T,     TK(BOOLEAN)) \
    X(R_T_STRING,   T,     TK(STRING)) \
    X(R_T_IDU,      T,     NN(IDU)) \
    X(R_DT_ARRAY,   DT,    TK(TYPE), NN(IDD), TK(EQUALS), TK(ARRAY), TK(LEFT_SQUARE), NN(NUM), TK(RIGHT_SQUARE), TK(OF), NN(T)) \
    X(R_DT_STRUCT,  DT,    TK(TYPE), NN(IDD), TK(EQUALS), TK(STRUCT), NN(NB), TK(LEFT_BRACES), NN(DC), TK(RIGHT_BRACES)) \
    X(R_DT_ALIAS,   DT,    TK(TYPE), NN(IDD), TK(EQUALS), NN(T)) \
    X(R_DC_REC,     DC,    NN(DC), TK(SEMI_COLON), NN(LI), TK(COLON), NN(T)) \
    X(R_DC,         DC,    NN(LI), TK(COLON), NN(T)) \
    X(R_DF,         DF,    TK(FUNCTION), NN(IDD), NN(NF), TK(LEFT_PARENTHESIS), NN(LP), TK(RIGHT_PARENTHESIS), TK(COLON), NN(T), NN(MF), NN(B)) \
    X(R_LP_REC,     LP,    NN(LP), TK(COMMA), NN(IDD), TK(COLON), NN(T)) \
    X(R_LP,         LP,    NN(IDD), TK(COLON), NN(T)) \
    X(R_B,          B,     TK(LEFT_BRACES), NN(LDV), NN(LS), TK(RIGHT_BRACES)) \
    X(R_LDV_REC,    LDV,   NN(LDV), NN(DV)) \
    X(R_LDV,        LDV,   NN(DV)) \
    X(R_LS_REC,     LS,    NN(LS), NN(S)) \
    X(R_LS,         LS,    NN(S)) \
    X(R_DV,         DV,    TK(VAR), NN(LI), TK(COLON), NN(T), TK(SEMI_COLON)) \
    X(R_LI_REC,     LI,    NN(LI), TK(COMMA), NN(IDD)) \
    X(R_LI,         LI,    NN(IDD)) \
    X(R_S_IF,       S,     TK(IF), TK(LEFT_PARENTHESIS), NN(E), TK(RIGHT_PARENTHESIS), NN(MT), NN(S)) \
    X(R_S_IFELSE,   S,     TK(IF), TK(LEFT_PARENTHESIS), NN(E), TK(RIGHT_PARENTHESIS), NN(MT), NN(S), TK(ELSE), NN(ME), NN(S)) \
    X(R_S_WHILE,    S,     TK(WHILE), NN(MW), TK(LEFT_PARENTHESIS), NN(E), TK(RIGHT_PARENTHESIS), NN(MJ), NN(S)) \
    X(R_S_DO,       S,     TK(DO), NN(MD), NN(S), NN(MK), TK(WHILE), TK(LEFT_PARENTHESIS), NN(E), TK(RIGHT_PARENTHESIS), TK(SEMI_COLON)) \
    X(R_S_BLOCK,    S,     NN(NB), NN(B)) \
    X(R_S_ASSIGN,   S,     NN(LV), TK(EQUALS), NN(E), TK(SEMI_COLON)) \
    X(R_S_BREAK,    S,     TK(BREAK), TK(SEMI_COLON)) \
    X(R_S_CONTINUE, S,     TK(CONTINUE), TK(SEMI_COLON)) \
    X(R_S_RETURN,   S,     TK(RETURN), NN(E), TK(SEMI_COLON)) \
    X(R_E_AND,      E,     NN(E), TK(AND), NN(L)) \
    X(R_E_OR,       E,     NN(E), TK(OR), NN(L)) \
    X(R_E,          E,     NN(L)) \
    X(R_L_LT,       L,     NN(L), TK(LESS_THAN), NN(R)) \
    X(R_L_GT,       L,     NN(L), TK(GREATER_THAN), NN(R)) \
    X(R_L_LE,       L,     NN(L), TK(LESS_OR_EQUAL), NN(R)) \
    X(R_L_GE,       L,     NN(L), TK(GREATER_OR_EQUAL), NN(R)) \
    X(R_L_EQ,       L,     NN(L), TK(EQUAL_EQUAL), NN(R)) \
    X(R_L_NE,       L,     NN(L), TK(NOT_EQUAL), NN(R)) \
    X(R_L,          L,     NN(R)) \
    X(R_R_ADD,      R,     NN(R), TK(PLUS), NN(Y)) \
    X(R_R_SUB,      R,     NN(R), TK(MINUS), NN(Y)) \
    X(R_R,          R,     NN(Y)) \
    X(R_Y_MUL,      Y,     NN(Y), TK(TIMES), NN(F)) \
    X(R_Y_DIV,      Y,     NN(Y), TK(DIVIDE), NN(F)) \
    X(R_Y,          Y,     NN(F)) \
    X(R_F_LV,       F,     NN(LV)) \
    X(R_F_PREINC,   F,     TK(PLUS_PLUS), NN(LV)) \
    X(R_F_PREDEC,   F,     TK(MINUS_MINUS), NN(LV)) \
    X(R_F_POSTINC,  F,     NN(LV), TK(PLUS_PLUS)) \
    X(R_F_POSTDEC,  F,     NN(LV), TK(MINUS_MINUS)) \
    X(R_F_PAREN,    F,     TK(LEFT_PARENTHESIS), NN(E), TK(RIGHT_PARENTHESIS)) \
    X(R_F_CALL,     F,     NN(IDU), NN(MC), TK(LEFT_PARENTHESIS), NN(LE), TK(RIGHT_PARENTHESIS)) \
    X(R_F_NEG,      F,     TK(MINUS), NN(F)) \
    X(R_F_NOT,      F,     TK(NOT), NN(F)) \
    X(R_F_TRUE,     F,     NN(TRUE)) \
    X(R_F_FALSE,    F,     NN(FALSE)) \
    X(R_F_CHR,      F,     NN(CHR)) \
    X(R_F_STR,      F,     NN(STR)) \
    X(R_F_NUM,      F,     NN(NUM)) \
    X(R_LE_REC,     LE,    NN(LE), TK(COMMA), NN(E)) \
    X(R_LE,         LE,    NN(E)) \
    X(R_LV_FIELD,   LV,    NN(LV), TK(DOT), NN(ID)) \
    X(R_LV_INDEX,   LV,    NN(LV), TK(LEFT_SQUARE), NN(E), TK(RIGHT_SQUARE)) \
    X(R_LV_IDU,     LV,    NN(IDU)) \
    X(R_IDD,        IDD,   TK(ID)) \
    X(R_IDU,        IDU,   TK(ID)) \
    X(R_ID,         ID,    TK(ID)) \
    X(R_TRUE,       TRUE,  TK(TRUE_)) \
    X(R_FALSE,      FALSE, TK(FALSE_)) \
    X(R_CHR,        CHR,   TK(CHARACTER)) \
    X(R_STR,        STR,   TK(STRINGVAL)) \
    X(R_NUM,        NUM,   TK(NUMERAL)) \
    X(R_NB,         NB,    EPS) \
    X(R_NF,         NF,    EPS) \
    X(R_MF,         MF,    EPS) \
    X(R_MT,         MT,    EPS) \
    X(R_ME,         ME,    EPS) \
    X(R_MW,         MW,    EPS) \
    X(R_MJ,         MJ,    EPS) \
    X(R_MD,         MD,    EPS) \
    X(R_MK,         MK,    EPS) \
    X(R_MC,         MC,    EPS)

enum Rule {
#define SSL_X(name, lhs, ...) name,
    SSL_RULES(SSL_X)
#undef SSL_X
    R_COUNT
};

struct Production {
    Nt lhs;
    std::vector<Sym> rhs;
    const char *name;
};

const std::vector<Production> &grammar();

/* Nome de um simbolo (terminal como aparece na gramatica; nao terminal pelo nome) */
std::string symName(Sym s);
/* "S -> 'if' '(' E ')' MT S" */
std::string ruleText(int rule);

} // namespace ssl

#endif // SSL_GRAMMAR_HPP
