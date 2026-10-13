#include "semantics.hpp"

#include <cctype>
#include <sstream>

namespace ssl {

const char *Diagnostic::phase() const {
    switch (code) {
        case ERR_LEXICAL: return "lexico";
        case ERR_SYNTAX: return "sintatico";
        case ERR_REDECL: case ERR_NOT_DECL: return "escopo";
        default: return "tipo";
    }
}

Semantics::Semantics(const Lexer &lex) : lex_(lex), last_{Token::UNKNOWN, -1, 1, 1, ""} {}

/* ------------------------------------------------------------------ */
/* Erros                                                              */
/* ------------------------------------------------------------------ */

void Semantics::addError(ErrorCode code, int line, int col, const std::string &msg) {
    Diagnostic d = {code, line, col, msg};
    diags_.push_back(d);
}

void Semantics::error(ErrorCode code, const std::string &d) {
    std::string msg;
    switch (code) {
        case ERR_REDECL: msg = "identificador '" + d + "' ja declarado neste escopo"; break;
        case ERR_NOT_DECL: msg = "identificador '" + d + "' nao declarado"; break;
        case ERR_TYPE_EXPECTED: msg = "'" + d + "' nao e um tipo"; break;
        case ERR_BOOL_TYPE_EXPECTED: msg = "expressao do tipo boolean esperada"; break;
        case ERR_TYPE_MISMATCH: msg = "tipos incompativeis"; break;
        case ERR_INVALID_TYPE: msg = "tipo invalido para o operador"; break;
        case ERR_KIND_NOT_STRUCT: msg = "acesso a campo em algo que nao e struct"; break;
        case ERR_FIELD_NOT_DECL: msg = "campo '" + d + "' nao existe na struct"; break;
        case ERR_KIND_NOT_ARRAY: msg = "indexacao de algo que nao e array nem string"; break;
        case ERR_INVALID_INDEX_TYPE: msg = "indice deve ser do tipo integer"; break;
        case ERR_KIND_NOT_VAR: msg = "'" + d + "' nao e variavel nem parametro"; break;
        case ERR_KIND_NOT_FUNC: msg = "'" + d + "' nao e funcao"; break;
        case ERR_TOO_MANY_ARGS: msg = "argumentos demais na chamada de '" + d + "'"; break;
        case ERR_TOO_FEW_ARGS: msg = "argumentos de menos na chamada de '" + d + "'"; break;
        case ERR_PARAM_TYPE: msg = "tipo incompativel com o parametro " + d; break;
        case ERR_RETURN_TYPE: msg = "tipo do return incompativel com o tipo de retorno da funcao"; break;
        case ERR_BREAK_OUTSIDE: msg = "'break' fora de um comando de repeticao"; break;
        case ERR_CONTINUE_OUTSIDE: msg = "'continue' fora de um comando de repeticao"; break;
        default: msg = d; break;
    }
    addError(code, last_.line, last_.col, msg);
}

/* ------------------------------------------------------------------ */
/* Tipos                                                              */
/* ------------------------------------------------------------------ */

/* Sinonimos (alias) sao transparentes: a compatibilidade e por nome entre
 * os tipos base (decisao 7 do README). */
Object *Semantics::base(Object *t) const {
    while (t->kind == ALIAS_TYPE_) t = t->type;
    return t;
}

bool Semantics::check(Object *a, Object *b) const {
    a = base(a);
    b = base(b);
    return a == b || a->kind == UNIVERSAL_ || b->kind == UNIVERSAL_;
}

bool Semantics::isScalar(Object *t) const {
    t = base(t);
    return t->kind == SCALAR_TYPE_ || t->kind == UNIVERSAL_;
}

bool Semantics::checkBool(Object *t) {
    if (check(t, st_.pBool)) return true;
    error(ERR_BOOL_TYPE_EXPECTED);
    return false;
}

/* ------------------------------------------------------------------ */
/* Codigo                                                             */
/* ------------------------------------------------------------------ */

void Semantics::emit(const std::string &instr, int n) {
    emit(instr + " " + std::to_string(n));
}

void Semantics::label(int l) { code_.push_back("L" + std::to_string(l) + ":"); }

std::string Semantics::assembly() const {
    std::ostringstream out;
    out << "; Pool de constantes (LOAD_CONST n)\n";
    for (int i = 0; i < lex_.numConsts(); ++i) {
        out << ";   " << i << ": ";
        switch (lex_.constType(i)) {
            case 0: {
                char c = lex_.getCharConst(i);
                if (std::isprint(static_cast<unsigned char>(c))) out << "char '" << c << "'";
                else out << "char " << static_cast<int>(static_cast<unsigned char>(c));
                break;
            }
            case 1: out << "integer " << lex_.getIntConst(i); break;
            case 2: out << "string \"" << lex_.getStringConst(i) << "\""; break;
            default: out << "boolean " << (lex_.getBoolConst(i) ? "true" : "false"); break;
        }
        out << "\n";
    }
    for (size_t i = 0; i < code_.size(); ++i) out << code_[i] << "\n";
    return out.str();
}

/* ------------------------------------------------------------------ */
/* Pedacos de regras repetidos                                        */
/* ------------------------------------------------------------------ */

/* ++LV, --LV, LV++, LV-- (figura 7.8) */
void Semantics::incDec(Attr &res, bool pre, bool inc) {
    if (!check(top(0).type, st_.pInt)) {
        error(ERR_INVALID_TYPE);
        res.type = st_.pUniversal;
    } else {
        res.type = st_.pInt;
    }
    emit("DUP");
    emit("DUP");
    emit("DE_REF", 1);
    emit(inc ? "INC" : "DEC");
    emit("STORE_REF", 1);
    emit("DE_REF", 1);
    if (!pre) emit(inc ? "DEC" : "INC");
}

/* Operadores binarios: top(1) = operando esquerdo, top(0) = direito */
void Semantics::binary(Attr &res, Rule rule) {
    Object *l = top(1).type, *r = top(0).type;
    const char *op = "";
    switch (rule) {
        case R_E_AND: case R_E_OR: {
            bool a = checkBool(l);
            bool b = checkBool(r);
            (void)a; (void)b;
            res.type = st_.pBool;
            emit(rule == R_E_AND ? "AND" : "OR");
            return;
        }
        case R_L_LT: op = "LT"; break;
        case R_L_GT: op = "GT"; break;
        case R_L_LE: op = "LE"; break;
        case R_L_GE: op = "GE"; break;
        case R_L_EQ: op = "EQ"; break;
        case R_L_NE: op = "NEQ"; break;
        default: break;
    }
    if (op[0] != '\0') {      /* relacionais: operandos escalares compativeis */
        if (!check(l, r)) error(ERR_TYPE_MISMATCH);
        else if (!isScalar(l) || !isScalar(r)) error(ERR_INVALID_TYPE);
        res.type = st_.pBool;
        emit(op);
        return;
    }
    /* aritmeticos: integer; o '+' tambem aceita string (concatenacao) */
    bool add = rule == R_R_ADD;
    bool bad = false;
    if (!check(l, r)) { error(ERR_TYPE_MISMATCH); bad = true; }
    else if (!check(l, st_.pInt) && !(add && check(l, st_.pString))) { error(ERR_INVALID_TYPE); bad = true; }
    res.type = bad ? st_.pUniversal : (base(l)->kind == UNIVERSAL_ ? r : l);
    emit(rule == R_R_ADD ? "ADD" : rule == R_R_SUB ? "SUB" : rule == R_Y_MUL ? "MUL" : "DIV");
}

/* Verifica um argumento de chamada contra o proximo parametro (LE) */
void Semantics::checkArg(Attr &le, Object *argType) {
    if (le.err) return;
    if (static_cast<size_t>(le.n) >= le.func->list.size()) {
        error(ERR_TOO_MANY_ARGS, lex_.getName(le.func->name));
        le.err = true;
        return;
    }
    if (!check(le.func->list[static_cast<size_t>(le.n)]->type, argType))
        error(ERR_PARAM_TYPE, std::to_string(le.n + 1) + " de '" + lex_.getName(le.func->name) + "'");
    ++le.n;
}

/* ------------------------------------------------------------------ */
/* Semantics(r): uma acao por regra da gramatica                      */
/* ------------------------------------------------------------------ */

void Semantics::run(int ruleNo) {
    Rule rule = static_cast<Rule>(ruleNo);
    Attr res;   /* atributos do lado esquerdo, empilhados no final */

    /* numero de nos que a regra consome: um por nao terminal do lado direito */
    int nPop = 0;
    const std::vector<Sym> &rhs = grammar()[static_cast<size_t>(ruleNo)].rhs;
    for (size_t i = 0; i < rhs.size(); ++i)
        if (!isTerminal(rhs[i])) ++nPop;

    switch (rule) {
        /* ---------------- regras sem atributos ---------------- */
        case R_START: case R_P: case R_LDE_REC: case R_LDE: case R_DE_DF:
        case R_DE_DT: case R_B: case R_LDV_REC: case R_LDV: case R_LS_REC:
        case R_LS:
            break;

        /* ---------------- escopo: IDD, IDU, ID (figura 5.8) ---------------- */
        case R_IDD: {
            int name = last_.sec;
            Object *p = st_.search(name);
            if (p != nullptr) {
                error(ERR_REDECL, lex_.getName(name));
                p = st_.detached(name);       /* nao entra na tabela */
            } else {
                p = st_.define(name);
            }
            p->kind = NO_KIND_DEF_;
            res.name = name;
            res.obj = p;
            break;
        }
        case R_IDU: {
            int name = last_.sec;
            Object *p = st_.find(name);
            if (p == nullptr) {
                error(ERR_NOT_DECL, lex_.getName(name));
                p = st_.define(name);         /* evita novos erros iguais */
                p->kind = UNIVERSAL_;
                p->type = st_.pUniversal;
            }
            res.name = name;
            res.obj = p;
            break;
        }
        case R_ID:
            res.name = last_.sec;
            break;

        /* ---------------- escopo: marcadores NB e NF ---------------- */
        case R_NB:
            st_.newBlock();
            break;
        case R_NF: {                          /* figura 7.18 */
            Object *f = top(0).obj;
            f->kind = FUNCTION_;
            f->type = st_.pUniversal;
            f->nParams = f->nVars = 0;
            f->index = nFuncs_++;
            st_.newBlock();                   /* parametros e locais: mesmo nivel */
            break;
        }

        /* ---------------- tipos T (figuras 6.5 e 7.12) ---------------- */
        case R_T_INTEGER: res.type = st_.pInt; res.size = 1; break;
        case R_T_CHAR: res.type = st_.pChar; res.size = 1; break;
        case R_T_BOOLEAN: res.type = st_.pBool; res.size = 1; break;
        case R_T_STRING: res.type = st_.pString; res.size = 1; break;
        case R_T_IDU: {
            Object *p = top(0).obj;
            if (isTypeKind(p->kind)) {
                res.type = p;
                res.size = p->size;
            } else {
                if (p->kind != UNIVERSAL_) error(ERR_TYPE_EXPECTED, lex_.getName(p->name));
                res.type = st_.pUniversal;
                res.size = 0;
            }
            break;
        }

        /* ---------------- declaracao de tipos ---------------- */
        case R_DT_ARRAY: {                    /* figuras 6.8 e 7.13 */
            Object *p = top(2).obj;
            p->kind = ARRAY_TYPE_;
            p->numElems = top(1).val;
            p->type = top(0).type;
            p->size = p->numElems * top(0).size;
            break;
        }
        case R_DT_ALIAS: {                    /* figuras 6.9 e 7.14 */
            Object *p = top(1).obj;
            p->kind = ALIAS_TYPE_;
            p->type = top(0).type;
            p->size = top(0).size;
            break;
        }
        case R_DT_STRUCT: {                   /* figuras 6.11 e 7.16 */
            Object *p = top(2).obj;
            p->kind = STRUCT_TYPE_;
            p->list = top(0).list;
            p->size = top(0).size;
            st_.endBlock();
            break;
        }
        case R_DC: case R_DC_REC: {           /* figuras 6.10 e 7.15 */
            const Attr &li = top(1);
            const Attr &t = top(0);
            int n = 0;
            if (rule == R_DC_REC) { res.list = top(2).list; n = top(2).size; }
            for (size_t i = 0; i < li.list.size(); ++i) {
                Object *p = li.list[i];
                p->kind = FIELD_;
                p->type = t.type;
                p->index = n;
                p->size = t.size;
                n += t.size;
                res.list.push_back(p);
            }
            res.size = n;
            break;
        }

        /* ---------------- listas de identificadores ---------------- */
        case R_LI:
            res.list.push_back(top(0).obj);
            break;
        case R_LI_REC:
            res.list = top(1).list;
            res.list.push_back(top(0).obj);
            break;

        /* ---------------- variaveis (figura 7.19) ---------------- */
        case R_DV: {
            const Attr &li = top(1);
            const Attr &t = top(0);
            int n = cur_->nParams + cur_->nVars;   /* enderecos continuam entre blocos */
            for (size_t i = 0; i < li.list.size(); ++i) {
                Object *p = li.list[i];
                p->kind = VAR_;
                p->type = t.type;
                p->index = n;
                p->size = t.size;
                n += t.size;
            }
            cur_->nVars = n - cur_->nParams;
            break;
        }

        /* ---------------- funcoes (figuras 6.12, 6.13, 7.17, 7.18) ---------------- */
        case R_LP: case R_LP_REC: {
            const Attr &idd = top(1), &t = top(0);
            int n = 0;
            if (rule == R_LP_REC) { res.list = top(2).list; n = top(2).size; }
            Object *p = idd.obj;
            p->kind = PARAM_;
            p->type = t.type;
            p->index = n;
            p->size = t.size;
            res.list.push_back(p);
            res.size = n + t.size;
            break;
        }
        case R_MF: {
            const Attr &t = top(0), &lp = top(1), &idd = top(3);
            Object *f = idd.obj;
            f->type = t.type;
            f->list = lp.list;
            f->nParams = lp.size;
            f->nVars = 0;
            cur_ = f;
            loops_.clear();
            res.offset = static_cast<int>(code_.size());
            code_.push_back("BEGIN_FUNC");    /* completado em R_DF */
            break;
        }
        case R_DF: {
            Object *f = top(5).obj;
            code_[static_cast<size_t>(top(1).offset)] =
                "BEGIN_FUNC " + std::to_string(f->index) + ", " + std::to_string(f->nParams) +
                ", " + std::to_string(f->nVars);
            code_.push_back("END_FUNC");
            st_.endBlock();
            cur_ = nullptr;
            break;
        }

        /* ---------------- comandos (figuras 6.14, 6.15, 7.5, 7.21, 7.22) ---------------- */
        case R_MT: {
            checkBool(top(0).type);
            res.label = newLabel();
            emit("TJMP_FW L" + std::to_string(res.label));
            break;
        }
        case R_S_IF:
            label(top(1).label);
            break;
        case R_ME: {
            res.label = newLabel();
            emit("JMP_FW L" + std::to_string(res.label));
            label(top(1).label);
            break;
        }
        case R_S_IFELSE:
            label(top(1).label);
            break;
        case R_MW: {
            res.label = newLabel();
            label(res.label);
            Loop lp = {-1, res.label, false};
            loops_.push_back(lp);
            break;
        }
        case R_MJ:
            checkBool(top(0).type);
            loops_.back().brk = newLabel();
            emit("TJMP_FW L" + std::to_string(loops_.back().brk));
            break;
        case R_S_WHILE:
            emit("JMP_BW L" + std::to_string(top(3).label));
            label(loops_.back().brk);
            loops_.pop_back();
            break;
        case R_MD: {
            res.label = newLabel();
            label(res.label);
            Loop lp = {-1, -1, true};
            loops_.push_back(lp);
            break;
        }
        case R_MK:                            /* alvo do 'continue': a condicao */
            if (loops_.back().cont >= 0) label(loops_.back().cont);
            break;
        case R_S_DO:
            checkBool(top(0).type);
            emit("NOT");
            emit("TJMP_BW L" + std::to_string(top(3).label));
            if (loops_.back().brk >= 0) label(loops_.back().brk);
            loops_.pop_back();
            break;
        case R_S_BLOCK:
            st_.endBlock();
            break;
        case R_S_ASSIGN: {
            const Attr &lv = top(1);
            if (!check(lv.type, top(0).type)) error(ERR_TYPE_MISMATCH);
            if (lv.strChar) {
                emit("CALL_LIB 0, 1");        /* set_char: devolve o caractere */
                emit("POP");
            } else {
                emit("STORE_REF", lv.type->size);
            }
            break;
        }
        case R_S_BREAK:
            if (loops_.empty()) {
                error(ERR_BREAK_OUTSIDE);
            } else {
                if (loops_.back().brk < 0) loops_.back().brk = newLabel();
                emit("JMP_FW L" + std::to_string(loops_.back().brk));
            }
            break;
        case R_S_CONTINUE:
            if (loops_.empty()) {
                error(ERR_CONTINUE_OUTSIDE);
            } else if (loops_.back().isDo) {  /* do-while: para a condicao, adiante */
                if (loops_.back().cont < 0) loops_.back().cont = newLabel();
                emit("JMP_FW L" + std::to_string(loops_.back().cont));
            } else {                          /* while: para o teste, atras */
                emit("JMP_BW L" + std::to_string(loops_.back().cont));
            }
            break;
        case R_S_RETURN:
            if (!check(cur_->type, top(0).type)) error(ERR_RETURN_TYPE);
            emit("RET");
            break;

        /* ---------------- expressoes ---------------- */
        case R_E: case R_L: case R_R: case R_Y: case R_F_PAREN:
            res.type = top(0).type;
            break;
        case R_E_AND: case R_E_OR: case R_L_LT: case R_L_GT: case R_L_LE:
        case R_L_GE: case R_L_EQ: case R_L_NE: case R_R_ADD: case R_R_SUB:
        case R_Y_MUL: case R_Y_DIV:
            binary(res, rule);
            break;

        case R_F_LV:                          /* DE_REF n (figura 7.8) */
            res.type = top(0).type;
            if (top(0).strChar) emit("CALL_LIB 0, 0");   /* char_at */
            else emit("DE_REF", res.type->size);
            break;
        case R_F_PREINC: incDec(res, true, true); break;
        case R_F_PREDEC: incDec(res, true, false); break;
        case R_F_POSTINC: incDec(res, false, true); break;
        case R_F_POSTDEC: incDec(res, false, false); break;
        case R_F_NEG:
            if (!check(top(0).type, st_.pInt)) { error(ERR_INVALID_TYPE); res.type = st_.pUniversal; }
            else res.type = st_.pInt;
            emit("NEG");
            break;
        case R_F_NOT:
            if (!check(top(0).type, st_.pBool)) { error(ERR_INVALID_TYPE); res.type = st_.pUniversal; }
            else res.type = st_.pBool;
            emit("NOT");
            break;
        case R_F_TRUE: case R_F_FALSE: case R_F_CHR: case R_F_STR: case R_F_NUM:
            res.type = top(0).type;
            emit("LOAD_CONST", top(0).pos);
            break;

        /* literais (figura 6.7); o token secundario e o do ultimo terminal empilhado */
        case R_TRUE: case R_FALSE: res.type = st_.pBool; res.pos = last_.sec; break;
        case R_CHR: res.type = st_.pChar; res.pos = last_.sec; break;
        case R_STR: res.type = st_.pString; res.pos = last_.sec; break;
        case R_NUM:
            res.type = st_.pInt;
            res.pos = last_.sec;
            res.val = lex_.getIntConst(last_.sec);
            break;

        /* ---------------- chamada de funcao (figuras 6.23 e 7.28) ---------------- */
        case R_MC: {
            Object *f = top(0).obj;
            if (f->kind != FUNCTION_) {
                if (f->kind != UNIVERSAL_) error(ERR_KIND_NOT_FUNC, lex_.getName(f->name));
                res.type = st_.pUniversal;
                res.err = true;
            } else {
                res.type = f->type;
                res.func = f;
            }
            break;
        }
        case R_LE: {
            const Attr &mc = top(1);
            res.err = mc.err;
            res.func = mc.func;
            checkArg(res, top(0).type);
            break;
        }
        case R_LE_REC:
            res = top(1);
            checkArg(res, top(0).type);
            break;
        case R_F_CALL: {
            const Attr &mc = top(1), &le = top(0);
            res.type = mc.type;
            if (!mc.err) {
                if (!le.err && static_cast<size_t>(le.n) < mc.func->list.size())
                    error(ERR_TOO_FEW_ARGS, lex_.getName(mc.func->name));
                emit("CALL", mc.func->index);
            }
            break;
        }

        /* ---------------- left-values (figuras 6.22 e 7.27) ---------------- */
        case R_LV_IDU: {
            Object *p = top(0).obj;
            if (p->kind != VAR_ && p->kind != PARAM_) {
                if (p->kind != UNIVERSAL_) error(ERR_KIND_NOT_VAR, lex_.getName(p->name));
                res.type = st_.pUniversal;
            } else {
                res.type = p->type;
                emit("LOAD_REF", p->index);   /* indice da variavel (e nao o tamanho) */
            }
            break;
        }
        case R_LV_FIELD: {
            Object *t = base(top(1).type);
            int name = top(0).name;
            res.type = st_.pUniversal;
            if (t->kind != STRUCT_TYPE_) {
                if (t->kind != UNIVERSAL_) error(ERR_KIND_NOT_STRUCT);
            } else {
                Object *f = nullptr;
                for (size_t i = 0; i < t->list.size() && f == nullptr; ++i)
                    if (t->list[i]->name == name) f = t->list[i];
                if (f == nullptr) {
                    error(ERR_FIELD_NOT_DECL, lex_.getName(name));
                } else {
                    res.type = f->type;
                    emit("ADD", f->index);
                }
            }
            break;
        }
        case R_LV_INDEX: {
            Object *t = base(top(1).type);
            res.type = st_.pUniversal;
            if (t == st_.pString) {
                res.type = st_.pChar;         /* a geracao fica para F -> LV / S -> LV = E */
                res.strChar = true;
            } else if (t->kind != ARRAY_TYPE_) {
                if (t->kind != UNIVERSAL_) error(ERR_KIND_NOT_ARRAY);
            } else {
                res.type = t->type;
                emit("MUL", t->type->size);
                emit("ADD");
            }
            if (!check(top(0).type, st_.pInt)) error(ERR_INVALID_INDEX_TYPE);
            break;
        }
        case R_COUNT:
            break;
    }

    pop(nPop);
    push(res);
}

} // namespace ssl
