<!-- markdownlint-disable MD024 -->
# Changelog

All notable changes to FundOS are documented here.

## [1.0.3] — 2026-08-22

### Fixed

- Update OFX parser to stop treating closing tags in transactions as malformed
- Update OFX parser to handle self closing tags more gracefully
- Update import process to allow user selection of name or memo field for imported memo

## [1.0.2] — 2026-07-06

### Fixed

- Ledger balance checkpoints displayed out of order in account history, causing bank-balance rows to appear interleaved with the wrong transactions and produce spurious discrepancy rows

## [1.0.1] — 2026-07-03

### Fixed

- Funds with a balance not appearing on initial home page load
- Closed funds appearing in the transaction dialog and budget editor combo boxes even without a balance
- Closed funds being blocked from allocation, preventing users from saving a transaction against them; closed funds are now a cosmetic convenience only

## [1.0.0] — 2026-06-30

### Added

- Core library: accounts, funds, transactions, allocations, budgets, phases, targets, corrections
- OFX/QFX import with duplicate and correction detection
- Manual transaction entry
- Envelope budgeting engine with fixed and percentage phases, freely interleaved
- Per-target caps and overdraw support
- Append-only transaction ledger with full audit trail
- Qt desktop application for Windows and Linux
- Windows installer (MSI)
- Linux packages (.deb for Debian/Ubuntu, .rpm for Fedora)
- In-app update checker
- In-app documentation link
