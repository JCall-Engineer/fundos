# Contributing to FundOS

FundOS welcomes contributions, but the project has strong opinions — especially in the core library.
Please open an issue before writing code. This lets us align on approach before you invest time in an implementation.

## Philosophy

The core library is the heart of FundOS.
UI layers are intentionally thin — they display data and call into core.
Logic belongs in core, not in UI code; contributions that belong in core will be asked to move there.

## Process

1. Open an issue describing what you want to change and why.
2. Wait for acknowledgment before writing code.
3. Fork the repository and work on a branch.
4. Open a pull request referencing the issue.

The core library is held to a strict standard and contributions are reviewed closely.
UI contributions are held to a lower bar — results matter more than implementation details — but the process is the same.

## Style — Core Library

The rules below are not negotiable.

### Indentation and alignment

Tabs are used for indentation. Spaces are used for vertical alignment.
These serve different purposes and must not be mixed up.

Tabs establish indentation level — their visual width is irrelevant because spaces handle alignment independently.
This means alignment holds correctly at any tab width.

```cpp
// Tabs indent each line to the correct level.
// Spaces align the field names, type names, and column indices into columns.
auto extract = [&](sqlite3_stmt* stmt) -> transaction {
	transaction out;
	out.record.id_           =           sqlite3_column_int64  (stmt, 0);
	out.record.amount        = currency {sqlite3_column_int64  (stmt, 1)};
	out.record.date_recorded = datetime {sqlite3_column_int64  (stmt, 2)};
	out.record.memo          =           extract_text          (stmt, 3);
	out.record.fitid         =           extract_optional_text (stmt, 4);
	return out;
};
```

Related lines of code should be vertically aligned where it aids readability:

```cpp
      T& value()       & { return std::get<T>(data); }
const T& value() const & { return std::get<T>(data); }
```

### Braces

K&R style. Curly braces are always included, even for single-statement bodies.

```cpp
if (condition) {
	do_something();
}
```

### Naming

The project is spelled **FundOS** in prose, titles, and headings, and `fundos` in code, namespaces, and directory names. Use the appropriate form for context.

- Variable and function names use full words. **No abbreviations**.
- Types use `snake_case`, matching the C++ standard library convention.
- Variable names must not shadow type names.
- Namespaces are used extensively. Elements inside a namespace are not indented.

### Namespaces and long classes

Closing braces for namespaces and long classes include a comment identifying what is closed:

```cpp
}; // class db
} // namespace fundos::import
} // namespace fundos
```

### Trailing commas

Trailing commas are used in all lists where the language permits.
This keeps git history clean — reordering or removing an entry does not produce a spurious diff on the line above it:

```cpp
enum class error : uint8_t {
	none,
	not_ready,
	corrupted,
	unavailable,
};
```

### Documentation comments

Every significant type, function, and constant gets a Doxygen `///` comment.
Do not split a sentence across multiple lines — reword until it fits on one line.
`@param` and `@return` tags are used for non-obvious parameters and all return values that can represent distinct outcomes.

### No sentence wrapping

Lines do not have a hard column limit, but sentences are not split across lines.
Reword until a sentence is a reasonable length, then keep it on one line.

### Prefer structure over if/else chains

Prefer dispatch tables, variant visitors, or `switch` statements over long chains of `if`/`else if` when branching on a known set of discrete values. This applies in both core and UI layers.

The goal is to make the set of cases explicit. A visitor over a `std::variant` won't compile if a new variant goes unhandled; a `switch` makes the matched values visible at a glance in a way a chain of equality checks does not.

This rule does not apply to range checks or other conditions that don't reduce to discrete value matching — a trichotomy on `<`, `==`, `>` is cleaner as `if/else if/else` than any alternative.

### Ternary expressions

Prefer ternary expressions over `if` statements for value selection. Use `if` statements when side effects are involved.

## Qt interoperability

The core library headers are included by Qt targets, and Qt defines several global macros that can silently corrupt identifiers in any included header.

### Reserved macro names

Qt defines the following macros in the global namespace: `signals`, `slots`, `emit`, `foreach`, `forever`.
These expand to nothing or to Qt-specific syntax, and will silently corrupt any identifier that matches them.
Identifiers in the core library must not use any of these names.

### Include order

In any translation unit that includes both core library headers and Qt headers, includes must appear in this order:

1. Standard library headers
2. Core library headers
3. Qt headers

This ensures Qt's macros are not in scope when core library headers are parsed.
Note that moc-generated files are an exception — Qt controls their include order — which is why the naming restriction above is also necessary.

## Style — Qt Desktop Layer

The tab/space, braces, naming, trailing comma, and documentation comment rules above apply here too.
The Qt layer diverges from the core library in a few places.

### Type naming

Qt types use PascalCase (`AppDatabase`, `DatePicker`, `ImportDialog`), matching Qt's own convention.
Everything else — member variables, local variables, functions — uses `snake_case`.

### Constructor style

When the parameter list is short, everything goes on one line:

```cpp
AccountPage::AccountPage(AppCoordinator* coordinator, QWidget* parent) : QWidget(parent), app_coordinator(coordinator) {
```

When the parameter list warrants multiple lines, the initializer list follows the closing `)` and the opening brace ends that line:

```cpp
AccountPage::AccountPage(
	AppCoordinator* coordinator,
	fundos::account opening,
	std::optional<fundos::transaction> requested,
	QWidget* parent
) : QWidget(parent), app_coordinator(std::move(coordinator)), record(std::move(opening)), requested_transaction(std::move(requested)) {
```

When individual initializers need inline comments, each goes on its own line and the opening brace gets its own line:

```cpp
explicit DatePickerPopup(QDate value, DatePicker* owner_widget, QWidget* parent = nullptr)
	: QWidget(parent, Qt::Popup)
	// Qt::Popup makes this an auto-dismissing top-level window:
	// clicking outside it closes it automatically, and close() hides it cleanly after a day is picked.
	, current_date(value)
	, owner(owner_widget)
{
```

### Strings

`tr()` is mandatory for all user-visible strings.
Use `tr()` with numbered arguments (`%1`, `%2`) rather than string concatenation, so translators can reorder them for their language.
Any commit that adds or changes `tr()` strings must proofread them before committing — lupdate can sometimes track string mutations and preserve translations, but this is not guaranteed, and orphaned translations are deleted.
See [TRANSLATING.md](TRANSLATING.md) for the full translation workflow.

### File organization

```text
desktop/
  components/    Elements added to a page or dialog (buttons, cards, drag handles, etc.)
  content/       Whole pages and dialogs
  shell/         Top-level chrome (MainWindow, StatusBar)
  *.{hpp,cpp}    Global infrastructure (AppCoordinator, AppDatabase, AppContext)
```

File organization is a judgment call, not a formula.
The guiding question is whether splitting a class into its own file makes the codebase easier to navigate, or just adds files.
Classes that are deeply coupled to one parent and never constructed elsewhere are usually better kept together; classes that do enough on their own to feel like independent abstractions usually warrant their own file.

### Architecture

Database operations run on a dedicated thread via `AppDatabase`.
OFX file parsing during import runs on its own thread.
Work that can be moved off the main thread should be.

Decision points and non-obvious choices must be documented with inline comments explaining the rationale.

### Care points

Contributions to the Qt layer are expected to respect:

- **Locale support**: all numeric formatting goes through the core locale types; hard-coded formats are not acceptable.
- **Visual hierarchy**: spacing, weight, and color should follow the established hierarchy rather than introducing ad hoc styling.
- **Icon consistency**: icons come from [Tabler Icons](https://tabler.io/icons) (MIT licensed). Do not introduce icons from other sets.
- **License compliance**: any new dependency must have a license compatible with the AGPL, and its terms must be followed.
