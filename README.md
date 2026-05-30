# FundOS

FundOS is a cross-platform, locally-run personal finance application built around envelope budgeting.
It is currently in active development: the core library is complete and the UI clients are next.
Licensed under the [AGPL](https://www.gnu.org/licenses/agpl-3.0.html).

## What is FundOS?

Envelope budgeting is a method of allocating every dollar of income to a named purpose before you spend it.
Rather than tracking spending after the fact, you decide in advance how much goes to rent, groceries, savings, and so on, then spend from those envelopes.
This shifts budgeting from reactive to intentional.

FundOS implements envelope budgeting as a locally-run desktop and mobile application.
Your financial data lives on your devices and nowhere else.
Transactions are imported from OFX files exported by your bank, or entered manually. No bank credentials required.

Many web-based budgeting services connect to your bank automatically by collecting your login credentials and logging in on your behalf to scrape your transaction history.
This makes them a high-value target for attackers seeking access to people's bank accounts.
FundOS sidesteps this entirely.

## Why FundOS?

Most budgeting tools, when they support automation at all, let you allocate fixed dollar amounts to categories.
FundOS goes further. Budgets are made up of phases that can be fixed dollar amounts or percentages of remaining income, freely interleaved in any order.
This makes it straightforward to express rules like "save 10% off the top, then cover fixed expenses, then split what's left" without workarounds.

FundOS is open source, licensed under the AGPL.
A tool that exists to help people take control of their finances shouldn't be locked behind a subscription that itself undermines financial freedom.

## How It Works

FundOS models personal finance around five core concepts: accounts, funds, transactions, allocations, and budgets. Budgets are further broken down into phases and targets.

**Accounts** represent real-world bank accounts. They answer: where the money lives.

**Funds** are named virtual envelopes that money is mentally assigned to. They answer: what the money's for.

**Transactions** are recorded against an account, either imported from an OFX file or entered manually.
Each transaction has an amount, a date, an optional memo, and optional bank-assigned identifiers used to detect duplicates and corrections.

**Allocations** connect transactions to funds.
A transaction's amount is distributed across one or more funds via allocations, which can be set manually or automated by a budget.

**Budgets** define rules for automatically allocating income transactions.
A budget is made up of **phases** that execute in user-defined order.
Each phase is either fixed or percentage. Phases of both types may be freely interleaved.

A **fixed phase** contains targets that each claim a set amount from the running remainder.

A **percentage phase** contains targets that each claim a percentage of the remainder as it stood when the phase began. Targets within a percentage phase do not deplete each other.

A **target** is a rule for how much to add to a specific fund.
Fixed targets are well suited for predictable expenses like rent, groceries, or utilities.
Percentage targets are well suited for goals like savings, investments, or discretionary spending.
A target optionally specifies a cap on the target fund's balance, claiming only enough to bring the fund to its cap rather than the full amount.
A target may also allow overdraw, pulling the remainder negative to fully meet the fund's need.

After all phases complete, any remaining balance, positive or negative, flows to the budget's designated overflow fund.

**Corrections** are first-class records.
When a bank amends a transaction, the correction is stored as a new row linked to the original. Nothing is mutated and the full audit trail is preserved.

## Project Status

Active development: core library complete.

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
core/        C++20 library: all business logic, SQLite persistence, OFX parsing
desktop/     Qt application (Windows, Linux, macOS)
android/     Kotlin application via JNI/NDK
ios/         Swift application (planned)
```

All business logic lives in `core`.
UI layers are intentionally thin, calling into the core library and displaying results.
SQLite is vendored as an amalgamation (`sqlite3.c` / `sqlite3.h`).

## Building

FundOS uses CMake with presets for each platform and configuration. The configure step generates build system files; the build step compiles.

**When you add or remove source files, re-run the configure step.** The build uses `GLOB_RECURSE` to discover sources, which only re-scans during configure, not during incremental builds.

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
