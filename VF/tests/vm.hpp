/* Interpretador minimo da maquina virtual (figuras 7.1 a 7.3), usado so nos
 * testes para executar o assembly gerado e conferir o resultado dos programas.
 * Convencoes (o livro nao fixa estes detalhes):
 *   CALL n : os argumentos ja estao no topo; bp passa a apontar para o primeiro
 *            e o espaco das variaveis locais e reservado acima dos parametros;
 *   RET    : o valor devolvido (retSize celulas) substitui o registro de ativacao;
 *   CALL_LIB 0,0 = char_at(ref, i); CALL_LIB 0,1 = set_char(ref, i, c). */
#ifndef SSL_TEST_VM_HPP
#define SSL_TEST_VM_HPP

#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Cell {
    long n;
    bool isStr;
    std::string s;
    Cell() : n(0), isStr(false) {}
};

struct VM {
    struct Instr { std::string op; bool hasArg; long arg; std::string target; };
    struct Func { int p, v, ip; };

    std::vector<Instr> code;
    std::map<std::string, int> labels;
    std::map<int, Func> funcs;
    std::vector<Cell> consts;
    std::vector<Cell> st;
    std::string error;
    std::vector<Cell> lastResult;   /* celulas devolvidas pela chamada de nivel mais externo */

    static bool startsWith(const std::string &s, const char *p) { return s.compare(0, std::string(p).size(), p) == 0; }

    bool load(const std::string &text) {
        std::istringstream in(text);
        std::string line;
        int cur = -1;
        while (std::getline(in, line)) {
            if (startsWith(line, ";   ")) {                      /* ;   i: tipo valor */
                std::istringstream ls(line.substr(4));
                std::string idx, type;
                ls >> idx >> type;
                std::string rest;
                std::getline(ls, rest);
                rest = rest.substr(1);
                Cell c;
                if (type == "integer") c.n = std::atol(rest.c_str());
                else if (type == "boolean") c.n = rest == "true";
                else if (type == "string") { c.isStr = true; c.s = rest.substr(1, rest.size() - 2); }
                else c.n = rest[1];                              /* char 'c' */
                consts.push_back(c);
                continue;
            }
            if (line.empty() || line[0] == ';') continue;
            if (line[line.size() - 1] == ':') {                  /* rotulo */
                labels[line.substr(0, line.size() - 1)] = static_cast<int>(code.size());
                continue;
            }
            if (startsWith(line, "BEGIN_FUNC")) {
                int n, p, v;
                char c1, c2;
                std::istringstream ls(line.substr(10));
                ls >> n >> c1 >> p >> c2 >> v;
                Func f = {p, v, static_cast<int>(code.size())};
                funcs[n] = f;
                cur = n;
                continue;
            }
            if (line == "END_FUNC") { (void)cur; continue; }
            std::istringstream ls(line);
            Instr in1;
            in1.hasArg = false;
            in1.arg = 0;
            ls >> in1.op;
            std::string a;
            if (ls >> a) {
                if (!a.empty() && a[a.size() - 1] == ',') a.erase(a.size() - 1);
                if (a[0] == 'L') in1.target = a;
                else { in1.hasArg = true; in1.arg = std::atol(a.c_str()); }
                std::string b;
                if (ls >> b) in1.target = b;                     /* CALL_LIB l, f */
            }
            code.push_back(in1);
        }
        return true;
    }

    /* Executa a funcao 'f' com os argumentos dados (uma celula escalar cada). */
    bool call(int f, const std::vector<Cell> &args, Cell &result, int retSize = 1, long maxSteps = 2000000) {
        error.clear();
        st.assign(4096, Cell());
        const Func &fn = funcs[f];
        long sp = -1, bp = 0;
        for (size_t i = 0; i < args.size(); ++i) st[static_cast<size_t>(++sp)] = args[i];
        sp = bp + fn.p + fn.v - 1;
        std::vector<long> retIp, retBp, retSize2, retSp;
        long ip = fn.ip;
        retIp.push_back(-1); retBp.push_back(0); retSize2.push_back(retSize);
        for (long steps = 0; steps < maxSteps; ++steps) {
            if (ip < 0 || ip >= static_cast<long>(code.size())) { error = "ip fora do codigo"; return false; }
            const Instr &i = code[static_cast<size_t>(ip)];
            const std::string &op = i.op;
            Cell *s = &st[0];
            if (op == "LOAD_CONST") { s[++sp] = consts[static_cast<size_t>(i.arg)]; }
            else if (op == "LOAD_REF") { s[++sp] = Cell(); s[sp].n = i.arg; }
            else if (op == "DE_REF") {
                long a = bp + s[sp].n;
                for (long k = 0; k < i.arg; ++k) s[sp + k] = s[a + k];
                sp += i.arg - 1;
            } else if (op == "STORE_REF") {
                long a = bp + s[sp - i.arg].n;
                for (long k = 0; k < i.arg; ++k) s[a + k] = s[sp - i.arg + 1 + k];
                sp -= i.arg + 1;
            } else if (op == "ADD" && i.hasArg) { s[sp].n += i.arg; }
            else if (op == "MUL" && i.hasArg) { s[sp].n *= i.arg; }
            else if (op == "ADD" || op == "SUB" || op == "MUL" || op == "DIV" || op == "AND" || op == "OR" ||
                     op == "EQ" || op == "NEQ" || op == "LT" || op == "LE" || op == "GT" || op == "GE") {
                Cell &a = s[sp - 1], &b = s[sp];
                if (a.isStr || b.isStr) {
                    if (op == "ADD") a.s += b.s;
                    else {
                        int c = a.s.compare(b.s);
                        bool r = op == "EQ" ? c == 0 : op == "NEQ" ? c != 0 : op == "LT" ? c < 0 :
                                 op == "LE" ? c <= 0 : op == "GT" ? c > 0 : c >= 0;
                        a = Cell(); a.n = r;
                    }
                } else {
                    long x = a.n, y = b.n, r = 0;
                    if (op == "ADD") r = x + y; else if (op == "SUB") r = x - y; else if (op == "MUL") r = x * y;
                    else if (op == "DIV") { if (y == 0) { error = "divisao por zero"; return false; } r = x / y; }
                    else if (op == "AND") r = x && y; else if (op == "OR") r = x || y;
                    else if (op == "EQ") r = x == y; else if (op == "NEQ") r = x != y; else if (op == "LT") r = x < y;
                    else if (op == "LE") r = x <= y; else if (op == "GT") r = x > y; else r = x >= y;
                    a.n = r;
                }
                --sp;
            }
            else if (op == "INC") ++s[sp].n;
            else if (op == "DEC") --s[sp].n;
            else if (op == "NEG") s[sp].n = -s[sp].n;
            else if (op == "NOT") s[sp].n = !s[sp].n;
            else if (op == "DUP") { s[sp + 1] = s[sp]; ++sp; }
            else if (op == "POP") --sp;
            else if (op == "JMP_FW" || op == "JMP_BW") { ip = labels[i.target]; continue; }
            else if (op == "TJMP_FW" || op == "TJMP_BW") {
                bool jump = !s[sp--].n;
                if (jump) { ip = labels[i.target]; continue; }
            }
            else if (op == "CALL") {
                const Func &g = funcs[static_cast<int>(i.arg)];
                retIp.push_back(ip + 1); retBp.push_back(bp); retSize2.push_back(1);
                bp = sp - g.p + 1;
                sp = bp + g.p + g.v - 1;
                ip = g.ip;
                continue;
            }
            else if (op == "RET") {
                int rs = static_cast<int>(retSize2.back());
                for (int k = 0; k < rs; ++k) s[bp + k] = s[sp - rs + 1 + k];
                sp = bp + rs - 1;
                long back = retIp.back();
                lastResult.assign(st.begin() + bp, st.begin() + bp + rs);
                bp = retBp.back();
                retIp.pop_back(); retBp.pop_back(); retSize2.pop_back();
                if (back < 0) { result = s[sp]; return true; }
                ip = back;
                continue;
            }
            else if (op == "CALL_LIB") {
                if (i.target == "0") {                            /* char_at(ref, idx) */
                    long idx = s[sp].n;
                    Cell &str = s[bp + s[sp - 1].n];
                    --sp;
                    s[sp] = Cell();
                    s[sp].n = static_cast<unsigned char>(str.s[static_cast<size_t>(idx)]);
                } else {                                          /* set_char(ref, idx, c) */
                    Cell c = s[sp];
                    long idx = s[sp - 1].n;
                    Cell &str = s[bp + s[sp - 2].n];
                    str.s[static_cast<size_t>(idx)] = static_cast<char>(c.n);
                    sp -= 2;
                    s[sp] = c;
                }
            }
            else { error = "instrucao desconhecida: " + op; return false; }
            ++ip;
        }
        error = "limite de passos excedido";
        return false;
    }
};

#endif
