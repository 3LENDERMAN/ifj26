# Překladač jazyka IFJ26

Tento repozitář obsahuje implementaci překladače imperativního jazyka **IFJ26** (inspirováno jazykem Scala) do cílového **IFJcode26**. 
Projekt byl vytvořen v rámci předmětů **Formální jazyky a překladače (IFJ)** a **Algoritmy (IAL)**.

## Pravidla
- Zprávy commitů a veškerý obsah zdrojových souborů, včetně komentářů a názvosloví v angličtině.
- Plán projektu, zadání úkolů a finální dokumentace v češtině.

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
Projekt využívá nástroj GNU Make. Překlad se provede příkazem:
```bash
make build-check
