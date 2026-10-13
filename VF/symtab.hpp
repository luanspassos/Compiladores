/* =====================================================================
 * symtab.hpp
 *
 * Tabela de simbolos (cap. 5) com a estrutura 'object' estendida para
 * tipos (cap. 6) e geracao de codigo (cap. 7).
 *
 * Adaptacoes a C++ em relacao as figuras 5.3, 6.2 e 7.11:
 *   - os objetos ficam num std::deque (enderecos estaveis; EndBlock nao
 *     libera nada, pois campos e parametros sobrevivem ao seu bloco);
 *   - cada nivel e um std::vector<Object*> em vez de lista encadeada
 *     (pNext); assim parametros/campos formam listas proprias, sem se
 *     misturar com as variaveis locais do mesmo nivel;
 *   - a uniao discriminada virou uma struct com os campos de todos os kinds.
 * ===================================================================== */

#ifndef SSL_SYMTAB_HPP
#define SSL_SYMTAB_HPP

#include <cstddef>
#include <deque>
#include <vector>

namespace ssl {

enum Kind {
    NO_KIND_DEF_ = -1, VAR_, PARAM_, FUNCTION_, FIELD_, ARRAY_TYPE_,
    STRUCT_TYPE_, ALIAS_TYPE_, SCALAR_TYPE_, UNIVERSAL_
};

inline bool isTypeKind(Kind k) {
    return k == ARRAY_TYPE_ || k == STRUCT_TYPE_ || k == ALIAS_TYPE_ || k == SCALAR_TYPE_;
}

struct Object {
    int name = -1;              /* token secundario do identificador */
    Kind kind = NO_KIND_DEF_;

    /* Var/Param/Field: tipo; Function: tipo de retorno;
     * Array: tipo do elemento; Alias: tipo base */
    Object *type = nullptr;
    int size = 0;               /* tipos, vars, params, campos: tamanho em celulas */
    int index = 0;              /* Var/Param/Field: endereco; Function: no pool de funcoes */
    int numElems = 0;           /* Array */
    int nParams = 0, nVars = 0; /* Function: celulas de parametros e de locais */
    std::vector<Object *> list; /* Function: parametros; Struct: campos */
};

class SymbolTable {
public:
    SymbolTable();

    int newBlock();                    /* figura 5.4 */
    int endBlock();
    Object *define(int name);          /* figura 5.5: insere no nivel corrente */
    Object *search(int name) const;    /* figura 5.6: so o nivel corrente */
    Object *find(int name) const;      /* figura 5.7: todos os niveis visiveis */
    Object *detached(int name);        /* objeto fora da tabela (redeclaracao) */
    int level() const { return static_cast<int>(levels_.size()) - 1; }

    /* tipos pre-definidos (figura 6.3) */
    Object *pInt, *pChar, *pBool, *pString, *pUniversal;

private:
    Object *make(int name, Kind kind, int size);

    std::deque<Object> pool_;
    std::vector<std::vector<Object *> > levels_;
};

} // namespace ssl

#endif // SSL_SYMTAB_HPP
