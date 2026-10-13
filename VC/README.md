# Analisador Léxico — Simple Script Language

Trabalho da disciplina de Compiladores: implementação de um Analisador
Léxico em C++ para a linguagem definida na apostila do professor Luiz
Guedes (IME), capítulo 2 — "Análise Léxica".

## A linguagem

A apostila define uma linguagem de programação exemplo chamada **Simple
Script Language**, com sintaxe baseada em ECMAScript (JavaScript /
WMLScript). Ela suporta:

- Tipos primitivos: `integer`, `char`, `boolean`, `string`
- Tipos compostos: `array` e `struct`, além de sinônimos de tipo (`type`)
- Declaração de variáveis (`var`) e funções (`function`)
- Comandos: `if`/`else`, `while`, `do...while`, `break`, `continue`,
  blocos e atribuição
- Expressões com operadores lógicos (`&&`, `||`), relacionais (`<`,
  `>`, `<=`, `>=`, `==`, `!=`), aritméticos (`+`, `-`, `*`, `/`),
  incremento/decremento (`++`, `--`) e acesso a membros/arrays (`.`, `[]`)

## Gramática (BNF)

```
P    → LDE
LDE  → LDE DE | DE
DE   → DF | DT | DV

T    → 'integer' | 'char' | 'boolean' | 'string' | ID

DT   → 'type' ID '=' 'array' '[' NUM ']' 'of' T
     | 'type' ID '=' 'struct' '{' DC '}'
     | 'type' ID '=' T
DC   → DC ';' LI ':' T | LI ':' T

DF   → 'function' ID '(' LP ')' ':' T B
LP   → LP ',' ID ':' T | ID ':' T

B    → '{' LDV LS '}'
LDV  → LDV DV | DV
LS   → LS S | S

DV   → 'var' LI ':' T ';'
LI   → LI ',' ID | ID

S    → 'if' '(' E ')' S
     | 'if' '(' E ')' S 'else' S
     | 'while' '(' E ')' S
     | 'do' S 'while' '(' E ')' ';'
     | B
     | LV '=' E ';'
     | 'break' ';'
     | 'continue' ';'

E    → E '&&' L | E '||' L | L
L    → L '<' R | L '>' R | L '<=' R | L '>=' R | L '==' R | L '!=' R | R
R    → R '+' Y | R '-' Y | Y
Y    → Y '*' F | Y '/' F | F
F    → LV | '++' LV | '--' LV | LV '++' | LV '--'
     | '(' E ')' | ID '(' LE ')' | '-' F | '!' F
     | TRUE | FALSE | CHR | STR | NUM

LE   → LE ',' E | E
LV   → LV '.' ID | LV '[' E ']' | ID
```

Expressões regulares dos terminais léxicos:

```
letra  = 'a'..'z' + 'A'..'Z'
digito = '0'..'9'

ID  = letra . (letra + digito)*
NUM = digito . digito*
CHR = ''' . any . '''
STR = '"' . any* . '"'
```

> **Observação:** a gramática formal da apostila não define `_` como
> parte de identificadores válidos (diferente do pseudo-código de
> exemplo do próprio livro, que aceita), nem define uma palavra
> reservada `return`. O lexer segue a gramática formal; isso está
> documentado como comentário em `lexer.cpp`.

## Estrutura do projeto

| Arquivo               | Conteúdo                                                                                                              |
|------------------------|-------------------------------------------------------------------------------------------------------------------|
| `lexer.hpp`            | Enum `Token`, classe `Lexer` (API pública do lexer)                                                                |
| `lexer.cpp`            | Implementação do autômato finito (`Lexer::nextToken`), tabela de palavras reservadas (busca binária), tabela hash de identificadores e pool de constantes |
| `main.cpp`             | Driver de teste: lê um arquivo-fonte (ou `stdin`) e imprime a sequência de tokens reconhecidos em formato tabular  |
| `tests/test_main.cpp`  | Testes unitários (sem dependências externas) cobrindo palavras reservadas, literais, símbolos/operadores e erros léxicos |

## Como compilar e rodar

**Com `g++` (Linux / MinGW / WSL):**
```
g++ -Wall -Wextra -std=c++17 -o lexer main.cpp lexer.cpp
./lexer arquivo.ssl
```

**Com `cl.exe` (MSVC / Visual Studio):**
```
cl /EHsc /std:c++17 main.cpp lexer.cpp
lexer.exe arquivo.ssl
```

Sem argumentos, o programa lê o código-fonte de `stdin`. A saída é uma
tabela com linha, nome do token, token secundário (quando aplicável) e
o valor/lexema reconhecido. Erros léxicos (caractere inválido, string
ou char não terminados) são reportados em `stderr` com o número da
linha, e a análise continua até o fim do arquivo.

### Rodando os testes

```
g++ -Wall -Wextra -std=c++17 -o tests/test_main tests/test_main.cpp lexer.cpp
./tests/test_main
```

## Prompt utilizado

Como parte do trabalho, o professor pediu que documentássemos o
prompt que usaríamos para gerar o trabalho inteiro com uma IA. Segue o
prompt utilizado:

> Tenho em anexo a apostila de Compiladores do professor Luiz Guedes
> (IME), em PDF. Dentro dela, no capítulo 2 ("Análise Léxica"), seção
> 2.1, é definida uma linguagem de programação chamada "Simple Script
> Language", com sua gramática em BNF e as expressões regulares dos
> tokens elementares (identificador, número, caractere e string).
>
> 1. Extraia do PDF a gramática completa dessa linguagem (todas as
>    regras de produção) e as expressões regulares dos terminais
>    léxicos, exatamente como aparecem no documento.
> 2. Extraia também a enumeração de tokens (`t_token`) e o pseudo-código
>    de exemplo do Analisador Léxico apresentados na seção 2.2, e
>    aponte qualquer inconsistência entre o pseudo-código e a
>    definição formal da gramática (por exemplo, diferenças no
>    tratamento de identificadores).
> 3. Implemente em C++ idiomático (RAII, `std::string`,
>    `std::unordered_map`, sem ponteiros crus nem alocação manual), de
>    forma completa e funcional (sem os bugs presentes no pseudo-código
>    do livro), um Analisador Léxico para essa linguagem, encapsulado
>    em uma classe `Lexer` dentro do namespace `ssl`, contendo:
>    - Reconhecimento de todas as palavras reservadas via busca binária
>      em tabela ordenada alfabeticamente;
>    - Reconhecimento de todos os símbolos e operadores da linguagem,
>      incluindo os compostos (`==`, `!=`, `<=`, `>=`, `&&`, `||`,
>      `++`, `--`);
>    - Reconhecimento de identificadores, números inteiros, literais de
>      caractere e de string, seguindo as expressões regulares
>      definidas na apostila;
>    - Uma tabela de identificadores (nomes) implementada com
>      `std::unordered_map`, evitando duplicatas;
>    - Um "pool" de constantes (`std::vector`) para armazenar literais
>      de caractere, inteiro e string;
>    - Tratamento de erros léxicos (caractere inválido, string/char
>      não terminados) com número da linha, reportado em `stderr` sem
>      interromper a análise;
>    - Um lookahead de um caractere, como descrito na apostila, lendo
>      de qualquer `std::istream` (arquivo ou `stdin`).
> 4. Escreva um `main.cpp` de teste que leia um arquivo-fonte (ou
>    `stdin`, se nenhum argumento for passado) e imprima, para cada
>    token reconhecido: a linha, o nome do token, o token secundário
>    (quando houver) e o valor/lexema correspondente, em formato
>    tabular.
> 5. Escreva testes unitários em `tests/test_main.cpp` (sem framework
>    externo, usando macros simples de `CHECK`/`CHECK_EQ`) cobrindo:
>    infraestrutura básica (espaços em branco, contagem de linhas,
>    EOF), identificadores e palavras reservadas, números, strings,
>    caracteres, símbolos e operadores simples/compostos, e casos de
>    erro léxico (caractere inválido, string/char não terminados,
>    operadores isolados como `&` ou `|`).
> 6. Compile o código com `g++ -Wall -Wextra -std=c++17` e com
>    `cl.exe`, rode os testes e corrija o que for necessário até que
>    a compilação fique livre de warnings e todos os testes passem.
> 7. Gere um `README.md` documentando a linguagem, a gramática
>    completa, a estrutura do projeto e as instruções de compilação e
>    execução, tanto para `g++` quanto para `cl.exe` (MSVC).
