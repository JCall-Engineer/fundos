# FundOS

FundOS is a cross-platform, locally-run personal finance application built around envelope budgeting.
The core library and Qt desktop app are complete; release packaging is the remaining work before the first release.
Licensed under the [AGPL](https://www.gnu.org/licenses/agpl-3.0.html).

## What is Envelope Budgeting?

Envelope budgeting is a method of allocating every dollar of income to a named purpose before you spend it.
Rather than tracking spending after the fact, you decide in advance how much goes to rent, groceries, savings, and so on, then spend from those envelopes.
This shifts budgeting from reactive to intentional.

FundOS implements envelope budgeting as a locally-run desktop and mobile application.
Your financial data lives on your devices and nowhere else.
Transactions are imported from OFX files exported by your bank, or entered manually. No bank credentials required.

## Why FundOS?

Many web-based budgeting services connect to your bank automatically by collecting your login credentials and logging in on your behalf to scrape your transaction history.
This makes them a high-value target for attackers seeking access to people's bank accounts.
FundOS sidesteps this entirely.

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

Core library and Qt desktop app complete; packaging in progress.

| Area                                        | Status                                      |
|---------------------------------------------|---------------------------------------------|
| Underlying types (currency, datetime, etc.) | ✅ Complete                                 |
| Numeric locale support                      | ✅ Complete                                 |
| String translations                         | 🤝 Contributions welcome                    |
| Database schema                             | ✅ Complete                                 |
| Transaction recording and correction        | ✅ Complete                                 |
| OFX import (1.x SGML and 2.x XML)           | ✅ Complete                                 |
| Account, fund, and budget CRUD              | ✅ Complete                                 |
| Fund allocation                             | ✅ Complete                                 |
| Budget automation logic                     | ✅ Complete                                 |
| Transaction history (filtered views)        | ✅ Complete                                 |
| Fund history (filtered views)               | ✅ Complete                                 |
| Qt desktop app                              | ✅ Complete                                 |

### OFX Testing

The OFX parser has been manually tested against real bank exports.
Automated tests are deferred: committing real financial data is a non-starter, and synthetic files don't reflect the edge cases real bank exports produce.

## Roadmap

| Phase | Deliverable                        | Status         | Notes                                                                    |
|-------|------------------------------------|----------------|--------------------------------------------------------------------------|
| 1     | Core library                       | ✅ Complete    |                                                                          |
| 2     | Qt desktop app                     | ✅ Complete    | Windows and Linux; macOS testing pending access to a Mac                 |
| 3     | Release packaging                  | 🚧 In Progress | MSI (Windows), .deb (Debian), RPM (Fedora/RHEL/openSUSE)                 |
| 4     | Android app (Kotlin)               | 🔲 Planned     |                                                                          |
| 5     | iOS app (Swift)                    | 🔲 Planned     | Pending access to a Mac                                                  |
| 6     | Cross-device sync                  | 🔲 Planned     | Significant undertaking; design TBD                                      |

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

Building FundOS requires Qt6 and CMake. The process is the same across platforms after you take care of the dependencies.

### Windows

#### Dependencies

- [Visual Studio 2022](https://visualstudio.microsoft.com/) or later with the **Desktop development with C++** workload
- [Qt 6](https://www.qt.io/download-qt-installer) installed via the Qt Online Installer — select the **MSVC 2022 64-bit** kit under your Qt version. The MinGW kit is not supported with this project. Qt is available under the LGPL v3 open source license.

#### CMakeUserPresets.json

CMake needs to know where Qt is installed on your machine. Copy the provided example and fill in your Qt path:

```powershell
copy CMakeUserPresets.json.example CMakeUserPresets.json
```

Edit `CMakeUserPresets.json` and replace the placeholder path with your actual Qt installation directory, typically `C:/Qt/6.x.x/msvc2022_64`.

After you have taken care of the dependencies you can complete the [common building procedure](#common-building-procedure).

### Linux

> **Note:** Linux support has been confirmed on Arch Linux. Build reports for other distributions are welcome.

Install Qt 6 via your package manager:

- Debian/Ubuntu (apt)
  - `qt6-base-dev`
  - `qt6-tools-dev`
  - `libvulkan-dev`
- Fedora (dnf)
  - `qt6-qtbase-devel`
  - `qt6-linguist`
  - `vulkan-headers`
- Arch (pacman)
  - `qt6-base`
  - `qt6-tools`
  - `vulkan-headers`

If you install Qt via the Qt Online Installer instead, CMake may not find it automatically.
In that case, create a `CMakeUserPresets.json` modeled on `CMakeUserPresets.json.example` and set `CMAKE_PREFIX_PATH` to your Qt installation directory, typically `~/Qt/6.x.x/gcc_64`.

After you have taken care of the dependencies you can complete the [common building procedure](#common-building-procedure).

### Common Building Procedure

FundOS uses CMake with presets. Building is a two-step process: a **configure** step that generates build system files, and a **build** step that compiles. Each step has its own preset.

**Source discovery uses `GLOB_RECURSE`.** When you add or remove source files, re-run the configure step — the build system does not detect new or deleted files automatically during incremental builds.

### Presets

| Configure preset        | Build preset            | Platform | Notes                                    |
|-------------------------|-------------------------|----------|------------------------------------------|
| `windows-debug-local`   | `windows-debug-local`   | Windows  | Requires `CMakeUserPresets.json`         |
| `windows-release-local` | `windows-release-local` | Windows  | Requires `CMakeUserPresets.json`         |
| `linux-debug`           | `linux-debug`           | Linux    |                                          |
| `linux-release`         | `linux-release`         | Linux    |                                          |

```powershell
cmake --preset <configure-preset>
cmake --build --preset <build-preset> --parallel
```

## Running Tests

Tests are built automatically as part of the build step.

| Platform | Configuration | Path                                          |
|----------|---------------|-----------------------------------------------|
| Windows  | Debug         | `.\out\windows-debug\bin\Debug\tests.exe`     |
| Windows  | Release       | `.\out\windows-release\bin\Release\tests.exe` |
| Linux    | Debug         | `./out/linux-debug/bin/tests`                 |
| Linux    | Release       | `./out/linux-release/bin/tests`               |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
