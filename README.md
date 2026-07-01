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

Core library, Qt desktop app, and release packaging are complete for Windows and Linux.
macOS users can build from source; see [building](#building).
String translations for additional languages are welcome; see [TRANSLATING.md](TRANSLATING.md).

### OFX Testing

The OFX parser has been manually tested against real bank exports.
Automated tests are deferred: committing real financial data is a non-starter, and synthetic files don't reflect the edge cases real bank exports produce.

## Roadmap

| Phase | Deliverable                        | Status         | Notes                                             |
|-------|------------------------------------|----------------|---------------------------------------------------|
| 1     | Core library                       | ✅ Complete    |                                                   |
| 2     | Qt desktop app                     | ✅ Complete    | Windows and Linux; macOS can be built from source |
| 3     | Release packaging                  | ✅ Complete    | MSI (Windows), .deb (Debian/Ubuntu), RPM (Fedora) |
| 4     | Android app (Kotlin)               | 🔲 Planned     |                                                   |
| 5     | Cross-device sync                  | 🔲 Planned     | Significant undertaking; design TBD               |

## Architecture

FundOS is structured as a thin-client architecture across all platforms:

```text
core/        C++20 library: all business logic, SQLite persistence, OFX parsing
desktop/     Qt application (Windows, Linux, macOS)
android/     Kotlin application via JNI/NDK
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

#### Building a Windows Installer

Install [WiX Toolset v3](https://github.com/wixtoolset/wix3/releases) and add its `bin\` directory to your
`PATH` (typically `C:\Program Files (x86)\WiX Toolset v3.14\bin\`).

Build the release target, then run the install and packaging steps:

```powershell
cmake --build --preset windows-release-local --parallel
cmake --install out/build/windows-release --prefix out/install/windows
cpack --config out/build/windows-release/CPackConfig.cmake -G WIX
```

The resulting `FundOS-<version>-win64.msi` will appear in `out/packages/<version>/`.

### Linux

> **Note:** Linux support has been confirmed on Ubuntu, Fedora, and Arch Linux, including deb packaging on Ubuntu and rpm packaging on Fedora. Build reports for other distributions are welcome.

Install Qt 6 via your package manager:

- Debian/Ubuntu (apt)
  - `qt6-base-dev`
  - `qt6-tools-dev`
  - `libqt6svg6-dev`
  - `libxkbcommon-dev`
  - `libvulkan-dev`
- Fedora (dnf)
  - `qt6-qtbase-devel`
  - `qt6-qttools-devel`
  - `qt6-qtsvg-devel`
  - `libxkbcommon-devel`
  - `vulkan-headers`
- Arch (pacman)
  - `qt6-base`
  - `qt6-tools`
  - `vulkan-headers`

If you install Qt via the Qt Online Installer instead, CMake may not find it automatically.
In that case, create a `CMakeUserPresets.json` modeled on `CMakeUserPresets.json.example` and set `CMAKE_PREFIX_PATH` to your Qt installation directory, typically `~/Qt/6.x.x/gcc_64`.

After you have taken care of the dependencies you can complete the [common building procedure](#common-building-procedure).

#### Building a Linux Package

To package FundOS for your distribution, first build a release version, then run the following command with the `<FORMAT>` for your distribution:

```bash
cpack --config out/build/linux-release/CPackConfig.cmake -G <FORMAT>
```

| Distro        | Format | Output                       |
|---------------|--------|------------------------------|
| Debian/Ubuntu | `DEB`  | `FundOS-<version>-Linux.deb` |
| Fedora        | `RPM`  | `FundOS-<version>-Linux.rpm` |

The resulting package will appear in `out/packages/<version>/`.

### Common Building Procedure

FundOS uses CMake with presets. Building is a two-step process: a **configure** step that generates build system files, and a **build** step that compiles. Each step has its own preset.

**Source discovery uses `GLOB_RECURSE`.** When you add or remove source files, re-run the configure step — the build system does not detect new or deleted files automatically during incremental builds.

### Presets

| Configure preset        | Build preset            | Platform | Notes                            |
|-------------------------|-------------------------|----------|----------------------------------|
| `windows-debug-local`   | `windows-debug-local`   | Windows  | Requires `CMakeUserPresets.json` |
| `windows-release-local` | `windows-release-local` | Windows  | Requires `CMakeUserPresets.json` |
| `linux-debug`           | `linux-debug`           | Linux    |                                  |
| `linux-release`         | `linux-release`         | Linux    |                                  |

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
