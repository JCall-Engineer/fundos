# FundOS

FundOS is a cross-platform, locally-run personal finance application built around envelope budgeting.
It is currently in active development — the core library is complete and the UI clients are next.
Licensed under the [AGPL](https://www.gnu.org/licenses/agpl-3.0.html).

## How It Works

FundOS models personal finance using five core concepts — accounts, funds, transactions, allocations, and budgets — with budgets further broken down into phases and targets.

**Accounts** represent real-world bank accounts.
Transactions are recorded against an account, either imported from an OFX file or entered manually.
Each transaction has an amount, a date, an optional memo, and optional bank-assigned identifiers used to detect duplicates and corrections.

**Funds** are virtual envelopes — named buckets that money is mentally assigned to.
A fund can have an optional cap (a ceiling on how much it will accept) and can be open or closed.

**Allocations** connect transactions to funds.
A transaction's amount is distributed across one or more funds via allocations, which can be set manually or automated by a budget.

**Budgets** define rules for automatically allocating income transactions.
A budget is made up of **phases** that execute in user-defined order.
Each phase is either fixed or percentage — phases of both types may be freely interleaved.

A **fixed phase** contains targets that each claim a set amount from the running remainder.

A **percentage phase** contains targets that each claim a percentage of the remainder as it stood when the phase began — targets within a percentage phase do not deplete each other.

A **target** optionally specifies a cap, which limits how much will be added to the target fund — if the fund's balance is already near its cap, the target claims only enough to reach it, not the full amount.
A target may also allow overdraw, pulling the remainder negative to fully meet the fund's need.

Any remainder — positive or negative — after all phases complete flows to a designated overflow fund.

**Corrections** are first-class records.
When a bank amends a transaction, the correction is stored as a new row linked to the original — nothing is mutated, and the full audit trail is preserved.

## Project Status

Active development — core library complete.

| Area                                        | Status                                      |
|---------------------------------------------|---------------------------------------------|
| Underlying types (currency, datetime, etc.) | ✅ Complete                                 |
| Numeric locale support                      | ✅ Complete                                 |
| String translations                         | 🤝 Contributions welcome                    |
| Database schema                             | ✅ Complete                                 |
| Transaction recording and correction        | ✅ Complete                                 |
| OFX import (1.x SGML and 2.x XML)           | ✅ Complete (untested, presumed working)    |
| Multi-user handling                         | 🤔 Under consideration                      |
| Account, fund, and budget CRUD              | ✅ Complete                                 |
| Fund allocation                             | ✅ Complete                                 |
| Budget automation logic                     | ✅ Complete                                 |
| Transaction history (filtered views)        | ✅ Complete                                 |
| Fund history (filtered views)               | ✅ Complete                                 |

## Roadmap

| Phase | Deliverable                        | Status         | Notes                                                                   |
|-------|------------------------------------|----------------|-------------------------------------------------------------------------|
| 1     | Core library                       | ✅ Complete    |                                                                         |
| 2     | OFX parser tests                   | 🔲 Planned     |                                                                         |
| 3     | Qt desktop app                     | 🔲 Planned     | Windows and Linux, macOS testing pending access to a Mac                |
| 4     | Android app (Kotlin)               | 🔲 Planned     |                                                                         |
| 5     | iOS app (Swift)                    | 🔲 Planned     | Pending access to a Mac                                                 |
| 6     | Cross-device sync                  | 🔲 Planned     | Significant undertaking; design TBD                                     |
| 7     | Release packaging                  | 🔲 Planned     | MSI (Windows), .deb (Debian), RPM (Fedora/RHEL/openSUSE), APK (Android) |

## Architecture

FundOS is structured as a thin-client architecture across all platforms:

```text
core/        C++20 library — all business logic, SQLite persistence, OFX parsing
desktop/     Qt application (Windows, Linux, macOS)
android/     Kotlin application via JNI/NDK
ios/         Swift application (planned)
```

All business logic lives in `core`.
UI layers are intentionally thin — they call into the core library and display results.
SQLite is vendored as an amalgamation (`sqlite3.c` / `sqlite3.h`).

## Building

FundOS uses CMake with presets for each platform and configuration. The configure step generates build system files; the build step compiles.

**When you add or remove source files, re-run the configure step** — the build uses `GLOB_RECURSE` to discover sources, which only re-scans during configure, not during incremental builds.

### Windows (Debug)

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### Windows (Release)

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

### Linux (Debug)

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
```

### Linux (Release)

```bash
cmake --preset linux-release
cmake --build --preset linux-release
```

## Running Tests

Tests are built automatically as part of the build step.

### Windows

```powershell
.\out\windows-debug\bin\Debug\tests.exe
.\out\windows-release\bin\Release\tests.exe
```

### Linux

```bash
./out/linux-debug/bin/tests
./out/linux-release/bin/tests
```

## Output Structure

```text
out/
  build/               <- CMake generator files
  windows-debug/
    bin/Debug/         <- tests.exe, fundos.exe
    lib/Debug/         <- static libraries
  windows-release/
    bin/Release/
    lib/Release/
  linux-debug/
    bin/
    lib/
  linux-release/
    bin/
    lib/
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
