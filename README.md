# Překladač jazyka IFJ26

Tento repozitář obsahuje implementaci překladače imperativního jazyka **IFJ26** (inspirováno jazykem Scala) do cílového **IFJcode26**. 
Projekt byl vytvořen v rámci předmětů **Formální jazyky a překladače (IFJ)** a **Algoritmy (IAL)**.

## Zvolená varianta implementace
- **Tabulka symbolů:** Tabulka s rozptýlenými položkami s implicitním zřetězením položek (TRP s otevřenou adresací)

## Tým
| Jméno a příjmení | VUT login | GitHub |
| :--- | :--- | :--- |
| Jakub Holub (Vedoucí) | `xholubj00` | `@3lenderman` |
| Vít Kutěj | `xkutejv00` | `@Vigron` |
| David Pokorák | `xpokord00` | `@Tekaruxd` |
| Oliver Truchlý | `xtrucho00` | `@JustRevilO` |

## Překlad a spuštění
Projekt využívá nástroj GNU Make. Podporované cíle:
- `make build` – Standardní překlad cílové binárky (vyžadováno pro finální odevzdání).
- `make build-check` – Statická kontrola kódu (cppcheck) následovaná překladem.
- `make format` – Automatické formátování zdrojových kódů podle pravidel LLVM (`clang-format`).
- `make valgrind` – Spuštění projektu nástrojem Valgrind pro detekci úniků paměti.

### Spuštění překladače
Překladač čte zdrojový kód ze standardního vstupu (`stdin`) a generuje IFJcode26 na standardní výstup (`stdout`).
Příklad spuštění s testovacím souborem:
`./ifj26-compiler < tests/script_test_01`

## Pravidla
- Zprávy commitů a veškerý obsah zdrojových souborů, včetně komentářů a názvosloví v angličtině.
- Plán projektu, zadání úkolů a finální dokumentace v češtině.

### Struktura repozitáře
- `src/` – Adresář obsahující veškeré zdrojové a hlavičkové soubory.
- `tests/` – Adresář s základními a rozšířenými testy.

### Struktura commitů
Zprávy psané v angličtině s využitím následujících prefixů:
- `feat:` – Nová funkcionalita (např. *feat: add token structure*)
- `fix:` – Oprava chyby (např. *fix: memory leak in dstring*)
- `refactor:` – Úprava kódu bez změny chování (např. *refactor: simplify switch in lexer*)
- `doc:` – Změny v dokumentaci nebo README
- `build:` – Změny v Makefile, CI/CD nebo konfiguraci
- `test:` – Přidání nebo úprava testovacích skriptů
- `chore:` - Ostatní (např. *chore: change structure of repository*)
