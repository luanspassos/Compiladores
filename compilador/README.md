# Compilador da Simple Script Language (VF)

Continuação (VF) do trabalho de Compiladores (Prof. Luiz Guedes – IME). A VC
entregou o analisador léxico; esta etapa o reaproveita e acrescenta, na mesma
passada de um analisador ascendente, **análise sintática**, **análise de
escopo**, **análise de tipos** e **geração de código** para a máquina virtual
de pilha do capítulo 7 do livro.

A VC continua intacta na raiz do repositório (`../lexer.cpp`, `../main.cpp`,
`../tests/`); nesta pasta fica uma cópia do léxico com as poucas correções
listadas na seção 7. Tudo é C++11, sem bison/yacc/GOLD: a tabela de análise é
construída pelo próprio programa a partir da gramática.

| Arquivo | Conteúdo | Capítulo |
|---|---|---|
| `lexer.hpp/.cpp` | léxico da VC + posição, lexema, `return`, erros acumulados | 2 |
| `grammar.hpp/.cpp` | regras da gramática (uma lista só gera o `enum Rule` e a tabela de produções) | 3 |
| `lalr.hpp/.cpp` | FIRST, autômato LR(0), propagação de lookaheads, `Action[q][a]`, conflitos | 3.8 |
| `parser.hpp/.cpp` | algoritmo da figura 4.6 e mensagens de erro sintático | 4.1 |
| `symtab.hpp/.cpp` | tabela de símbolos (pilha de níveis) e `object` | 5, 6, 7.5 |
| `semantics.hpp/.cpp` | pilha semântica e `Semantics(rule)`: escopo, tipos e código | 4–7 |
| `main.cpp` | driver: `--tokens`, `--table`, `--grammar`, compilação | – |
| `tests/test_compilador.cpp`, `tests/vm.hpp` | testes e interpretador da VM (só para testes) | – |
| `tests/run_examples.sh` | compila os exemplos e compara com `exemplos/esperado/` | – |
| `exemplos/` | programas válidos, `erros/` e saídas esperadas | – |
| `prompt_segundo_trabalho.txt` | prompt usado para gerar esta etapa | – |

---

## 1. Entendimento

O compilador é um único analisador ascendente dirigido por tabela (figura 4.6)
com uma pilha semântica paralela à pilha sintática. A cada redução chama-se
`Semantics(r)`, um `switch` com um `case` por regra, que consome os nós dos
não terminais do lado direito, calcula os atributos do lado esquerdo e empilha
o nó resultante. As fases estáticas e a geração de código são **atributos
avaliados nesse mesmo `switch`**, como no livro:

```
fonte ──► Lexer ──► tokens ──► Parser (la-LR(1)) ──► Semantics(r) ──► assembly
            │                      │                  ├ escopo  (IDD, IDU, NB, NF)
      erros léxicos          erro sintático           ├ tipos   (kinds, CheckTypes, MC, MF)
      (param a análise)      (para a análise)         └ código  (MT, ME, MW, MJ, MD, MK)
```

* **Léxico (cap. 2)**: reaproveitado. Os erros léxicos são acumulados e, se
  houver algum, a análise sintática nem começa. O programa todo é tokenizado
  antes, o que permite o `--tokens` e mensagens com linha e coluna.
* **Sintático (cap. 3.8 e 4.1)**: tabela la-LR(1) construída em tempo de
  execução (seção 3). Em erro sintático a análise para (não há recuperação),
  mas os erros de escopo e tipo já encontrados são mantidos.
* **Escopo (cap. 5)**: tabela de símbolos como pilha de níveis; `IDD` declara
  (erro se já existe *no nível corrente*), `IDU` usa (erro se não existe em
  *nenhum* nível visível). Os erros de escopo e de tipo são **acumulados**.
* **Tipos (cap. 6)**: kinds dos identificadores, objetos para os tipos
  pré-definidos, tipo universal contra erros em cascata, `CheckTypes`,
  marcadores `MF` e `MC`.
* **Código (cap. 7)**: assembly textual da máquina de pilha, versão com
  left-values (a linguagem tem arrays e structs). O código fica num buffer e só
  é impresso se **nenhum erro** de qualquer fase ocorreu (o `ERROR_FLAG` do livro).

---

## 2. Gramática efetivamente usada

Fonte primária: `Gramática.pdf`. Marcadores semânticos (regras ε) e `return`
foram acrescentados; a tabela abaixo é a saída de `./ssl --grammar` (89 regras,
contando a regra aumentada `START → P`).

```
P    → LDE
LDE  → LDE DE | DE
DE   → DF | DT
T    → 'integer' | 'char' | 'boolean' | 'string' | IDU
DT   → 'type' IDD '=' 'array' '[' NUM ']' 'of' T
     | 'type' IDD '=' 'struct' NB '{' DC '}'
     | 'type' IDD '=' T
DC   → DC ';' LI ':' T | LI ':' T
DF   → 'function' IDD NF '(' LP ')' ':' T MF B
LP   → LP ',' IDD ':' T | IDD ':' T
B    → '{' LDV LS '}'
LDV  → LDV DV | DV
LS   → LS S | S
DV   → 'var' LI ':' T ';'
LI   → LI ',' IDD | IDD
S    → 'if' '(' E ')' MT S
     | 'if' '(' E ')' MT S 'else' ME S
     | 'while' MW '(' E ')' MJ S
     | 'do' MD S MK 'while' '(' E ')' ';'
     | NB B
     | LV '=' E ';'
     | 'break' ';' | 'continue' ';'
     | 'return' E ';'                                   (extensão)
E    → E '&&' L | E '||' L | L
L    → L '<' R | L '>' R | L '<=' R | L '>=' R | L '==' R | L '!=' R | R
R    → R '+' Y | R '-' Y | Y
Y    → Y '*' F | Y '/' F | F
F    → LV | '++' LV | '--' LV | LV '++' | LV '--' | '(' E ')'
     | IDU MC '(' LE ')' | '-' F | '!' F | TRUE | FALSE | CHR | STR | NUM
LE   → LE ',' E | E
LV   → LV '.' ID | LV '[' E ']' | IDU
IDD → Id    IDU → Id    ID → Id
TRUE → 'true'   FALSE → 'false'   CHR → c   STR → s   NUM → n
NB, NF, MF, MT, ME, MW, MJ, MD, MK, MC → ε            (marcadores semânticos)
```

Diferenças em relação à gramática oficial, todas justificadas na seção 7:

| Diferença | Motivo |
|---|---|
| `ID` (sem escopo) em `LV '.' ID` | o campo só se resolve pelo tipo da struct (cap. 5 e 6.22); a gramática oficial diz `IDU` |
| `'T'` entre aspas em `DC` é o não terminal `T` | decisão da VC |
| `return E ';'` e a palavra reservada `return` | sem ele uma função não devolve valor |
| `NB` em `struct` e em `S → NB B`; `NF`, `MF` na função; `MC` na chamada | marcadores dos caps. 5–7 |
| `MT`, `ME`, `MW` (cap. 7) e `MJ`, `MD`, `MK` (novos) | rótulos dos comandos; `MJ`, `MD`, `MK` dão suporte a `break`/`continue` |
| nada mais foi alterado | `B` continua exigindo ≥1 declaração e ≥1 comando; não há `;` depois de `type`; não há variável global nem comando-expressão |

---

## 3. Análise sintática

### Construção la-LR(1) (`lalr.cpp`)

1. **FIRST e anuláveis** por ponto fixo (os marcadores ε são anuláveis).
2. **Autômato LR(0)**: `closure`, `goto` e estados identificados pelo núcleo → **174 estados**.
3. **Lookaheads**: cada item do núcleo guarda um conjunto de terminais (máscara
   de 64 bits). Repete-se até o ponto fixo: calcula-se o fecho LR(1) do estado
   com os lookaheads atuais e propagam-se os lookaheads de cada item para o item
   deslocado no estado destino. O resultado é exatamente o que se obtém
   construindo os conjuntos LR(1) canônicos e **fundindo os estados de mesmo
   núcleo** (la-LR(1)), sem construir os conjuntos canônicos.
4. **`Action[q][a]`** (`std::vector<std::vector<int>>`, colunas = terminais e
   depois não terminais, de modo que `Action[Top()][Left[r]]` é o desvio):
   `0` erro, `p > 0` empilha/desvia para o estado `p`, `p < 0` reduz pela regra
   `-p-1`. Além disso `Len[r]` e `Left[r]`. A redução de `START → P` com `$` é o
   estado final.

`./ssl --table`:

```
Tabela la-LR(1)
  regras (com a regra aumentada): 89
  estados:                        174
  entradas de empilha:            463
  entradas de desvio (goto):      324
  entradas de reduz:              902
  conflitos empilha/reduz:        1
  conflitos reduz/reduz:          0
  estado 162, lookahead 'else': empilha/reduz, resolvido por empilhar; reduziria 27 (S -> 'if' '(' E ')' MT S)
```

### Else pendente
O único conflito é o esperado: no estado 162
(`S → 'if' '(' E ')' MT S .` / `... . 'else' ME S`) com lookahead `else`. É
resolvido **empilhando**, de modo que o `else` casa com o `if` mais próximo
(há teste que confere o código gerado). Reduz/reduz é resolvido pela regra de
menor número e contado; não ocorre nenhum. Os marcadores ε **não** criam
conflitos: o teste `test_table` falha se aparecer qualquer conflito além desse.

### Mensagens de erro sintático
`arquivo:linha:coluna: erro sintatico: encontrado 'tok' "lexema"; esperado: ...`.
A lista de esperados é **exata**: como os estados la-LR(1) fundem contextos, a
linha da tabela pode conter reduções que só falham depois; por isso, para cada
terminal candidato, a lista é obtida simulando as reduções sobre uma cópia da
pilha (sem ações semânticas) até o terminal ser empilhado ou rejeitado.

```
exemplos/erros/sintatico.ssl:5:5: erro sintatico: encontrado 'return' "return"; esperado: ';', '&&', '||', '<', '>', '<=', '>=', '!=', '==', '+', '-', '*', '/'
```

---

## 4. Análise de escopo (cap. 5)

* `SymbolTable` = pilha de níveis (`vector<vector<Object*>>`), com
  `newBlock`, `endBlock`, `define`, `search` (só o nível corrente) e `find`
  (do nível corrente ao global). O nível 0 é o global (funções e tipos). Os
  `Object` ficam num `std::deque` (endereços estáveis): `endBlock` não libera nada,
  pois campos e parâmetros continuam referenciados pela struct/função.
* **`IDD`**: `search`; se existe → `ERR_REDECL` e o objeto novo fica **fora da
  tabela** (não altera o original); senão `define`. O kind começa em `NO_KIND_DEF_`.
* **`IDU`**: `find`; se não existe → `ERR_NOT_DECL` e o nome é definido no nível
  corrente com kind `UNIVERSAL_`, de modo que os usos seguintes não repetem o erro.
* **`NB`** abre um nível em `struct` (campos fora do nível da struct) e em
  `S → NB B` (bloco-comando); `EndBlock` na redução da regra.
* **`NF`** na função: o nome da função está no nível externo (recursão
  permitida); o `NewBlock` do `NF` abre o nível único de **parâmetros e
  variáveis locais** (redeclarar um parâmetro como variável é erro). O corpo
  `B` da função não abre outro nível; `DF` fecha o nível.
* Campos de struct não passam pela tabela de símbolos: `LV '.' ID` procura o
  campo na lista de campos do tipo (`ERR_FIELD_NOT_DECL`).

| Código | Mensagem (exemplo) |
|---|---|
| `ERR_REDECL` | identificador 'a' ja declarado neste escopo |
| `ERR_NOT_DECL` | identificador 'z' nao declarado |

---

## 5. Análise de tipos (cap. 6)

### Kinds e objetos
`VAR_ PARAM_ FUNCTION_ FIELD_ ARRAY_TYPE_ STRUCT_TYPE_ ALIAS_TYPE_ SCALAR_TYPE_
UNIVERSAL_` e `NO_KIND_DEF_`. `integer`, `char`, `boolean`, `string` e o tipo
universal são cinco instâncias de `Object` (`pInt`, `pChar`, `pBool`,
`pString`, `pUniversal`). O **tipo universal tem kind `UNIVERSAL_`** (e não
`SCALAR_TYPE_`, como na figura 6.3): assim todo teste de kind o reconhece e
silencia erros derivados de uma declaração já reportada.

### Compatibilidade (`CheckTypes`)
Por **nome**, com *alias transparente*: resolvem-se os `ALIAS_TYPE_` até o tipo
base e dois tipos são compatíveis se são o mesmo objeto ou se um deles é
universal. Dois `type` de array/struct com a mesma forma **não** são compatíveis.

### Regras por construção

| Construção | Regra |
|---|---|
| `T → IDU` | kind de tipo ou universal; senão `ERR_TYPE_EXPECTED` e tipo universal |
| `DV`, `DC`, `LP` | preenchem kind, tipo, tamanho e endereço (cap. 7.5) |
| `if`, `while`, `do-while` | condição `boolean` (`ERR_BOOL_TYPE_EXPECTED`) |
| `LV = E` | `CheckTypes(LV, E)` (`ERR_TYPE_MISMATCH`) |
| `&&`, `||` | operandos `boolean`; resultado `boolean` |
| `<` `>` `<=` `>=` `==` `!=` | operandos compatíveis (`ERR_TYPE_MISMATCH`) **e escalares** (`ERR_INVALID_TYPE`); resultado `boolean` |
| `+` | `integer` ou `string` (concatenação); `-` `*` `/` só `integer` |
| `-F`, `++`, `--` | `integer` (`ERR_INVALID_TYPE`); `!F` exige `boolean` |
| `LV.ID` | struct (`ERR_KIND_NOT_STRUCT`) com o campo (`ERR_FIELD_NOT_DECL`) |
| `LV[E]` | array (`ERR_KIND_NOT_ARRAY`) ou **string → char**; índice `integer` (`ERR_INVALID_INDEX_TYPE`) |
| `LV → IDU` | VAR ou PARAM (`ERR_KIND_NOT_VAR`) |
| `IDU MC ( LE )` | `MC`: kind função (`ERR_KIND_NOT_FUNC`); `LE`: quantidade e tipo de cada argumento (`ERR_TOO_MANY_ARGS`, `ERR_PARAM_TYPE`), `F`: `ERR_TOO_FEW_ARGS` |
| `return E` | `CheckTypes(tipo de retorno, E)` (`ERR_RETURN_TYPE`) |
| `break`, `continue` | dentro de um laço (`ERR_BREAK_OUTSIDE`, `ERR_CONTINUE_OUTSIDE`) |

Após um erro num operador o resultado passa a ser o tipo universal (o livro
propaga o tipo do operando esquerdo), o que evita um segundo erro na mesma expressão.
Um erro não gera erros derivados: `x = zz + 1;` com `zz` não declarado produz um único erro.

---

## 6. Geração de código (cap. 7)

### Máquina e instruções usadas
Máquina de pilha da seção 7.1. Instruções (figura 7.2) efetivamente emitidas:

`ADD SUB MUL DIV INC DEC NEG AND OR NOT EQ NEQ LT LE GT GE`,
`JMP_FW JMP_BW TJMP_FW TJMP_BW`, `CALL n`, `RET`, `CALL_LIB l, f`,
`LOAD_CONST n`, `POP`, `DUP`, `ADD n`, `MUL n`, `LOAD_REF n`, `DE_REF n`,
`STORE_REF n`, e as pseudo-instruções `BEGIN_FUNC n, p, v`, `END_FUNC` e
rótulos `Ln:`. **Não são usadas** `LOAD_VAR`, `STORE_VAR`, `CHAR_AT`, `SET_CHAR`
(figura 7.9, versão sem left-values: a linguagem tem arrays e structs). Nenhuma
instrução foi inventada. O cabeçalho do arquivo gerado traz o **pool de
constantes** em linhas de comentário (`;   0: integer 2`), para o montador.

### Tamanhos e endereços
Tamanho em células escalares: `integer`, `char`, `boolean`, `string` = 1;
array = `n · tamanho(elem)`; struct = soma dos campos (cada campo guarda seu
deslocamento); alias = tamanho do tipo base. **Parâmetros** a partir de 0, em
ordem; **locais** logo depois, com a numeração **contínua entre os blocos da
mesma função** (variáveis de blocos irmãos não reutilizam endereços).
`BEGIN_FUNC n, p, v`: `n` = número da função (ordem de declaração), `p` =
células de parâmetros, `v` = células de variáveis **locais** (decisão 11). O
`v` só é conhecido no fim da função: `MF` emite `BEGIN_FUNC` e guarda sua
posição no buffer de código, e `DF` a completa (no livro: `ftell/fseek`).

```
function f(a: integer, v: vec, p: ponto): integer { var i: integer; ... }
  a → 0     v → 1..3 (vec = array [3] of integer)     p → 4..5     i → 6
  BEGIN_FUNC 0, 6, 1
```

### Esquema de cada construção (`||X||` = código de X)

| Construção | Código |
|---|---|
| `LV → IDU` | `LOAD_REF índice` (**índice** da variável, não o tamanho) |
| `LV.ID` | `||LV||` `ADD deslocamento` |
| `LV[E]` (array) | `||LV||` `||E||` `MUL tamanho_elem` `ADD` |
| `F → LV` | `||LV||` `DE_REF tamanho` |
| `LV = E ;` | `||LV||` `||E||` `STORE_REF tamanho` |
| `++LV` / `--LV` | `||LV||` `DUP` `DUP` `DE_REF 1` `INC`/`DEC` `STORE_REF 1` `DE_REF 1` |
| `LV++` / `LV--` | igual, seguido de `DEC` / `INC` |
| operadores | `||esq||` `||dir||` `ADD`/`SUB`/`MUL`/`DIV`/`AND`/`OR`/`LT`/`GT`/`LE`/`GE`/`EQ`/`NEQ`; `-F` → `NEG`; `!F` → `NOT` |
| literais | `LOAD_CONST n` (`n` = índice no pool; `true`/`false` também) |
| `f(args)` | `||arg1||` … `||argn||` `CALL n` |
| `return E ;` | `||E||` `RET` |
| `if (E) S` | `||E||` `TJMP_FW L1` `||S||` `L1:` |
| `if (E) S1 else S2` | `||E||` `TJMP_FW L1` `||S1||` `JMP_FW L2` `L1:` `||S2||` `L2:` |
| `while (E) S` | `L1:` `||E||` `TJMP_FW L2` `||S||` `JMP_BW L1` `L2:` |
| `do S while (E);` | `L1:` `||S||` `[Lc:]` `||E||` `NOT` `TJMP_BW L1` `[Lb:]` |
| `break ;` | `JMP_FW` para o rótulo logo após o laço |
| `continue ;` | `while`: `JMP_BW` para o teste; `do-while`: `JMP_FW` para a condição |
| `s[i]` (string), leitura | `||LV||` `||E||` `CALL_LIB 0, 0` |
| `s[i] = c ;` | `||LV||` `||E||` `||c||` `CALL_LIB 0, 1` `POP` |

Os rótulos são numerados no programa inteiro (`newLabel`), e os rótulos opcionais
do `do-while` (`Lc`, `Lb`) só são criados quando há `continue`/`break`. Os marcadores
`MW`, `MD` empilham o laço corrente numa pilha de laços; `MJ` (teste do `while`),
`MK` (início da condição do `do-while`) e as próprias regras de `break`/`continue`
a consultam.

### CALL_LIB e strings
`CALL_LIB 0, 0` = `char_at(ref, i)`: desempilha o índice e a referência (`bp +
ref` é a célula da variável string) e empilha o caractere. `CALL_LIB 0, 1` =
`set_char(ref, i, c)`: troca o caractere na própria variável e empilha `c`
(toda `CALL_LIB` empilha um resultado, figura 7.2), por isso a atribuição termina
em `POP`. Biblioteca 0 = *strings*; não há outras.

---

## 7. Decisões e ambiguidades

Itens 1–13 são os levantados no enunciado; 14 em diante surgiram durante a implementação.

1. **`'T'` entre aspas em `DC`** é o não terminal `T` (decisão da VC).
2. **Campo em `LV '.' ID`**: o cap. 5 usa `ID` (sem escopo) e a gramática diz `IDU`. Criei o
   não terminal `ID → Id` sem verificação: um `IDU` procuraria o campo na tabela de símbolos
   e acusaria "não declarado" indevidamente (ou acertaria uma variável homônima).
3. **`return`**: não existe na gramática. Sem ele uma função não devolve valor e, como os
   parâmetros são por valor e não há variáveis globais, nenhum programa teria efeito
   observável. Acrescentei a palavra reservada e `S → 'return' E ';'`, com a verificação
   de tipo contra o tipo de retorno e o código `||E|| RET` (o mesmo dos exemplos 7.4).
   É a única alteração da linguagem. Não verifico se todo caminho da função executa um `return`.
4. **`B → '{' LDV LS '}'`** exige ≥1 declaração e ≥1 comando. **Mantive a gramática oficial**
   (é a fonte primária) e reescrevi os exemplos do livro: corpo de laço com um só comando fica
   sem chaves; com vários comandos o bloco precisa de uma declaração (nos exemplos,
   uma variável auxiliar). O custo é só aparecer uma variável a mais; relaxar `LDV` não era indispensável.
5. **`NF` no lugar de `NB`** na função (fig. 7.18 × texto): vale o texto. Os parâmetros precisam
   de um nível novo antes de `LP`; `NF` também numera a função e abre o nível.
6. **`LOAD_REF`** (fig. 7.27 usa o tamanho do tipo): emite o **índice** da variável; o tamanho
   só é argumento de `DE_REF`/`STORE_REF`.
7. **Tipo universal** com kind `UNIVERSAL_` (fig. 6.3 dá `SCALAR_TYPE_`): ver seção 5.
8. **Listas `pNext`** (figs. 6.6–6.13): como parâmetros e locais ficam no mesmo nível, seguir `pNext`
   a partir do primeiro parâmetro percorreria também as variáveis. Funções e structs guardam um
   `vector<Object*>` próprio (parâmetros, campos) e `LI`, `LP`, `DC` carregam o `vector` como atributo.
9. **`CheckTypes`**: o estrutural (fig. 6.17) nunca avança `f1`/`f2` no laço dos campos (laço
   infinito) e recursa sem fim em tipos que se referem a si mesmos; o por nome (fig. 6.16) faz de
   `type A = integer` um tipo incompatível com `integer`. Adotei **por nome com alias transparente**.
10. **`true`/`false`**: não há `LOAD_TRUE`/`LOAD_FALSE` (fig. 7.7) e a fig. 7.26 usa um token secundário
    que o léxico não dava. O léxico passou a guardar a constante booleana no pool e a devolvê-la como
    token secundário; o código é `LOAD_CONST n`.
11. **`BEGIN_FUNC n, p, v`**: o texto e a fig. 7.18 contam os parâmetros em `v`; os exemplos 7.4
    não (`fatorial`: `BEGIN_FUNC 0 1 2`, com `n` parâmetro e `i`, `f` locais). Como o pool de funções guarda
    "tamanho dos parâmetros" e "tamanho das variáveis" separadamente e a VM reserva `p + v` células,
    `v` conta **só as locais** (os exemplos do livro também são internamente inconsistentes: o de `strinv`
    declara 3 locais e informa 1). Formato com vírgulas, como na fig. 7.18 e no enunciado.
12. **Indexação de string** (fig. 6.22 dá o tipo; fig. 7.27 só gera código para arrays): usa
    `CALL_LIB` como recomenda o livro, com a numeração da seção 6. `CHAR_AT`/`SET_CHAR` exigem
    o índice da variável e não servem para left-values gerais (campo, elemento de array).
13. **`break`/`continue`**: sem regras de atributos no livro (só aparecem na fig. 7.5). Implementei com uma
    pilha de laços e os marcadores `MW`, `MJ`, `MD`, `MK` (seção 6); `continue` do `do-while` salta com
    `JMP_FW` para a condição.
14. **Token secundário na redução**: o algoritmo da fig. 4.6 lê o próximo token logo após empilhar, de
    modo que `tokenSecundario` (figs. 5.8, 6.7) já é o do *lookahead* quando `IDD → Id` é reduzida. O
    analisador guarda o último terminal **empilhado** (`last_`), usado para o secundário e para a posição das mensagens.
15. **Verificação da condição**: o livro verifica `E` ao reduzir o comando inteiro (figs. 6.15, 7.21). Fiz no
    marcador `MT`/`MJ`, logo após a expressão, porque a posição reportada fica na condição e não no fim do comando.
    No `do-while` continua na redução do comando.
16. **`NE` × `NEQ`**: a fig. 7.24 emite `NE`, o conjunto de instruções (fig. 7.2) define `NEQ`; uso `NEQ`.
17. **`S → B ';'`** (fig. 7.5) × `S → B` (gramática): vale a gramática (`S → NB B`, sem `;`). Também
    não há comando-expressão, portanto nem o `POP` que a fig. 7.5 prevê para eles.
18. **Exemplos 7.4**: `return`, `'\0'` (o léxico só aceita `' any '`) e `s1[i] - s2[i]`
    (subtração de `char`, contrária às regras 6.20) foram reescritos (`exemplos/strings.ssl`).
19. **Operadores relacionais** exigem operandos escalares (a máquina compara uma célula; `EQ` sobre structs
    não faria sentido); a fig. 6.19 só pede compatibilidade.
20. **Gramática oficial × README da VC**: o PDF não tem `DV` em `DE` (sem variáveis globais), não tem `;` depois
    de `type`, usa `IDD`/`IDU` e `LE`/`LP` não vazios (funções têm ≥1 parâmetro e chamadas ≥1 argumento). Segui o PDF.
21. **Léxico (a VC)**: (a) `ID = letra.(letra+digito)*`, mas o laço só aceitava letras (`x1` virava `x` e `1`); corrigido.
    (b) `std::stoi` lançava exceção em numerais grandes; agora é erro léxico. (c) o `lineNumber()` da VC conta a
    quebra de linha lida como lookahead e por isso o `--tokens` pode mostrar a linha seguinte; **mantido** para
    a saída ser idêntica à da VC (as mensagens novas usam `tokenLine()`/`tokenColumn()`, corretas).
    (d) cada literal gera uma entrada no pool, inclusive o `NUM` de `array [n]`, que nunca é carregado.
22. **Limitações**: sem checagem de limites em tempo de execução; sem recuperação de erro sintático; a convenção
    de `CALL`/`RET` da VM (bp, retorno) é a do interpretador de teste, o livro não a fixa.

---

## 8. Como compilar, executar e testar

```
cd compilador
make                      # g++ -std=c++11 -Wall -Wextra -Wpedantic -O2  → ./ssl e tests/test_compilador
./ssl exemplos/fatorial.ssl            # assembly na saída padrão
./ssl exemplos/fatorial.ssl -o f.asm   # ... ou em arquivo
./ssl --tokens exemplos/fatorial.ssl   # só o léxico, formato da VC
./ssl --table                          # estatísticas e conflitos da tabela
./ssl --grammar                        # regras numeradas
echo 'function f(a: integer): integer { var x: integer; x = a; return x; }' | ./ssl
make test                 # testes da VF, exemplos e testes da VC (79 verificações)
```

Sem `make`: `g++ -std=c++11 -Wall -Wextra -Wpedantic -o ssl main.cpp parser.cpp semantics.cpp
symtab.cpp lalr.cpp grammar.cpp lexer.cpp`. Código de saída: 0 sem erros, 1 com erros, 2 uso incorreto.

### Testes (`tests/test_compilador.cpp`: executável único, 330 verificações)

* **léxico**: `return`, dígitos em identificadores, constantes booleanas, posições, erros acumulados;
* **tabela**: exatamente 1 conflito empilha/reduz (`else`) e 0 reduz/reduz; else pendente confere no código;
* **sintático**: linha, coluna, token encontrado e esperados; bloco sem declaração, `var` global, comando-expressão;
* **escopo** e **tipos**: um caso para cada código de erro, ausência de erros em cascata, acúmulo de erros;
* **código gerado**: saída esperada **calculada à mão** segundo as figuras 7.4–7.10 (if, if-else, while,
  do-while, break/continue, operadores, `++`/`--`, chamada, arrays, structs, strings, endereçamento);
* **execução**: o assembly é carregado num interpretador mínimo da VM (`tests/vm.hpp`) e os programas rodam
  (fatorial, Fibonacci, ordenação de array, troca de campos de struct, inversão de string, ...);
* **cobertura**: todas as 89 regras da gramática são reduzidas por algum teste;
* `tests/run_examples.sh`: compila `exemplos/*.ssl` (saída igual a `exemplos/esperado/*.asm`), confere que
  `exemplos/erros/*.ssl` falham sem gerar código (mensagens iguais a `exemplos/esperado/erros/*.txt`) e que
  `--tokens` reproduz exatamente a saída da VC.

---

## 9. Exemplos

### Código gerado (`exemplos/fatorial.ssl`, trecho)

```
function fatorial(n: integer): integer
{
    var i, f: integer;
    i = 2;
    f = 1;
    while (i <= n)
        f = f * i++;
    return f;
}
```
```
; Pool de constantes (LOAD_CONST n)
;   0: integer 2
;   1: integer 1
BEGIN_FUNC 0, 1, 2          ; (n → 0; i → 1, f → 2)
	LOAD_REF 1
	LOAD_CONST 0
	STORE_REF 1             ; i = 2
	LOAD_REF 2
	LOAD_CONST 1
	STORE_REF 1             ; f = 1
L0:
	LOAD_REF 1
	DE_REF 1
	LOAD_REF 0
	DE_REF 1
	LE
	TJMP_FW L1              ; while (i <= n)
	LOAD_REF 2              ; endereço de f
	LOAD_REF 2
	DE_REF 1                ; valor de f
	LOAD_REF 1
	DUP
	DUP
	DE_REF 1
	INC
	STORE_REF 1
	DE_REF 1
	DEC                     ; i++  (valor antigo)
	MUL
	STORE_REF 1             ; f = f * i++
	JMP_BW L0
L1:
	LOAD_REF 2
	DE_REF 1
	RET
END_FUNC
```
(os comentários `;` à direita foram acrescentados aqui; o compilador não os emite.)

### Struct, array e string (`s[i] = c`, `v[i] = p.y`)

```
function h(v: vec, p: pt, s: string): integer
{ var i: integer; var c: char; i = 1; v[i] = p.y; c = s[0]; s[1] = c; return v[2]; }
```
```
BEGIN_FUNC 0, 6, 2
	LOAD_REF 6 / LOAD_CONST 1 / STORE_REF 1                     i = 1
	LOAD_REF 0 / LOAD_REF 6 / DE_REF 1 / MUL 1 / ADD            endereço de v[i]
	LOAD_REF 3 / ADD 1 / DE_REF 1 / STORE_REF 1                 = p.y
	LOAD_REF 7 / LOAD_REF 5 / LOAD_CONST 2 / CALL_LIB 0, 0 / STORE_REF 1     c = s[0]
	LOAD_REF 5 / LOAD_CONST 3 / LOAD_REF 7 / DE_REF 1 / CALL_LIB 0, 1 / POP  s[1] = c
	LOAD_REF 0 / LOAD_CONST 4 / MUL 1 / ADD / DE_REF 1 / RET                 return v[2]
END_FUNC
```
(uma instrução por linha na saída real; juntadas aqui por `/` só para economizar espaço.)

### Erros (`exemplos/erros/`)

```
$ ./ssl exemplos/erros/lexico.ssl
exemplos/erros/lexico.ssl:4:11: erro lexico: '&' isolado (esperado '&&')
exemplos/erros/lexico.ssl:5:9: erro lexico: numeral grande demais
exemplos/erros/lexico.ssl:5:21: erro lexico: caractere invalido '@'

$ ./ssl exemplos/erros/escopo.ssl
exemplos/erros/escopo.ssl:1:33: erro escopo: identificador 'a' ja declarado neste escopo
exemplos/erros/escopo.ssl:3:24: erro escopo: identificador 'a' ja declarado neste escopo
exemplos/erros/escopo.ssl:5:12: erro escopo: identificador 'x' ja declarado neste escopo
exemplos/erros/escopo.ssl:6:9: erro escopo: identificador 'a' ja declarado neste escopo
exemplos/erros/escopo.ssl:7:5: erro escopo: identificador 'y' nao declarado
exemplos/erros/escopo.ssl:7:9: erro escopo: identificador 'z' nao declarado
exemplos/erros/escopo.ssl:8:9: erro escopo: identificador 'soma' nao declarado
exemplos/erros/escopo.ssl:13:9: erro escopo: identificador 'interna' nao declarado
exemplos/erros/escopo.ssl:17:10: erro escopo: identificador 'f' ja declarado neste escopo
9 erro(s); nenhum codigo gerado

$ ./ssl exemplos/erros/tipos.ssl          (23 erros; os primeiros)
exemplos/erros/tipos.ssl:20:15: erro tipo: 'x' nao e um tipo
exemplos/erros/tipos.ssl:21:16: erro tipo: tipos incompativeis
exemplos/erros/tipos.ssl:23:10: erro tipo: expressao do tipo boolean esperada
exemplos/erros/tipos.ssl:27:11: erro tipo: campo 'z' nao existe na struct
exemplos/erros/tipos.ssl:33:12: erro tipo: argumentos de menos na chamada de 'g'
exemplos/erros/tipos.ssl:42:17: erro tipo: tipo do return incompativel com o tipo de retorno da funcao
```

A coluna de um erro de escopo ou de tipo é a do último token empilhado quando a regra é
reduzida (`IDU`: o próprio identificador; uma atribuição: o `;`).

---

## 10. Revisão de cobertura

* **Toda regra da gramática tem ação semântica**: `Semantics::run` é um `switch` sobre o `enum Rule`
  sem `default`; o compilador (`-Wswitch`, parte de `-Wall`) acusa regra sem `case`. As 89 regras são
  exercitadas pelos testes (`test_rule_coverage`). As regras puramente estruturais
  (`P`, `LDE`, `DE`, `B`, `LDV`, `LS`) só consomem e empilham um nó vazio, conforme a convenção da seção 4.1.
* **Regras de atributos dos caps. 5–7 implementadas**: 5.8–5.10, 6.4–6.13, 6.14–6.23, 7.4–7.8, 7.10–7.28.
* **Omitidas com justificativa**: 6.17 (`CheckTypes` estrutural: laço infinito, decisão 9); 7.7 `LOAD_TRUE`/`LOAD_FALSE`
  (decisão 10); 7.9 e 7.29 (versão sem left-values: `LOAD_VAR`, `STORE_VAR`, `CHAR_AT`, `SET_CHAR`, `MA`; a
  linguagem tem arrays e structs); 7.10 `IDU '.' ID '(' LE ')'` com `CALL_LIB` de bibliotecas (a gramática não tem
  chamada de função de biblioteca); POP de comando-expressão da 7.5 (não há comando-expressão); `ftell/fseek` da
  7.18 (substituído por um buffer em memória com remendo em `DF`).

## Prompt utilizado

O prompt desta etapa está em `prompt_segundo_trabalho.txt`; a VC usou o prompt documentado em `../README.md`.
