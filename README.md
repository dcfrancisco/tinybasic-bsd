# bsdbasic

A small, portable BASIC interpreter designed with PDP-11 2.11BSD (K&R C constraints) in mind, and developed/tested on modern macOS/Linux.

PDP-11 / 2.11BSD is currently a compatibility target (I don’t have real hardware), so consider it “best effort” until someone confirms it on an actual system.

Target: CBM BASIC v2-style, line-numbered program execution, staying close to Microsoft 6502 BASIC source compatibility where practical.

## Naming / Disclaimer

This project is an independent hobby BASIC interpreter. The name “bsdbasic” is meant as “BASIC that builds/runs well on BSD-ish systems (and especially 2.11BSD on PDP-11)”.

It is not affiliated with, endorsed by, or derived from any official BSD source tree or Commodore/Microsoft BASIC implementation.

## Build

### macOS/Linux

```sh
make
```

### 2.11BSD (PDP-11)

```sh
make
```

The `Makefile` uses `uname` to choose flags.

## Run

```sh
./bsdbasic path/to/program.bas
```

## BASIC Coverage

- Line-numbered programs loaded from a file (`10 ...`, `20 ...`)
- Multiple statements per line via `:`

### Implemented (today)

Statements / keywords:

- [x] `REM`
- [x] `PRINT` (supports `;` and `,`, plus `TAB(n)` and `SPC(n)` within `PRINT`)
- [x] `INPUT`
- [x] `LET` (and implied assignment: `A=1`)
- [x] `IF ... THEN` (THEN line-number or THEN statement)
- [x] `GOTO`
- [x] `GOSUB` / `RETURN`
- [x] `FOR` / `NEXT`
- [x] `DIM`
- [x] `DATA` / `READ` / `RESTORE`
- [x] `ON ... GOTO` / `ON ... GOSUB`
- [x] `END` / `STOP`

CBM-ish extras:

- [x] `SLEEP(ticks)` (60 ticks ~= 1 second)
- [x] `PEEK(addr)`
- [x] `POKE addr,value`
- [x] `WAIT addr,mask[,value]`
- [x] `CLR`
- [x] `RUN`
- [x] `GET A$`

Operators / expressions:

- [x] Numeric + string values
- [x] `+ - * /`, parentheses
- [x] Relational: `= <> < <= > >=`
- [x] `AND` / `OR` / `NOT`

Functions:

- [x] `ABS()` `INT()` `RND()` `SGN()`
- [x] `SIN()` `COS()` `TAN()` `ATN()` `SQR()` `LOG()` `EXP()`
- [x] `LEN()` `VAL()` `ASC()`
- [x] `STR$()` `CHR$()`
- [x] `LEFT$()` `MID$()` `RIGHT$()`

### Not implemented (yet)

Statements / keywords:

- [ ] `NEW`, `LIST` (interactive editor commands)
- [ ] `LOAD`, `SAVE`, `VERIFY`
- [ ] `OPEN`, `CLOSE`, `CMD`
- [ ] `PRINT#`, `INPUT#`, `GET#`
- [ ] `SYS`, `USR`, `POKE` to real machine memory (current PEEK/POKE is VM memory)
- [ ] `DEF FN`

Functions / variables:

- [ ] `TI` / `TI$`
- [ ] `FRE()`
- [ ] `POS()`

I/O and other CBM features:

- [ ] Full CBM `PRINT` formatting/zones/quotes/spacing quirks
- [ ] Disk/device semantics (logical file numbers, secondary addresses, etc.)

### Extension: `SLEEP(ticks)`

`SLEEP(ticks)` sleeps for a number of 60Hz ticks (so `SLEEP(60)` sleeps about one second). On 2.11BSD the clock granularity is coarser (typically ~1/30s), so sub-tick sleeps are rounded up.

## Test

```sh
make test
```

## Status

Interpreter is functional and growing toward CBM BASIC v2 equivalency.

## License

MIT License. See LICENSE.
