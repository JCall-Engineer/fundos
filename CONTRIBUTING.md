# Contributing to FundOS

FundOS welcomes contributions, but the project has strong opinions — especially in the core library.
Please open an issue before writing code. This lets us align on approach before you invest time in an implementation.

UI layer contribution guidelines will be added as those layers take shape.

## Philosophy

The core library is the heart of FundOS.
UI layers are intentionally thin — they display data and call into core.
Logic belongs in core, not in UI code.
Contributions that blur this line will be asked to move the logic down.

## Process

1. Open an issue describing what you want to change and why.
2. Wait for acknowledgment before writing code.
3. Fork the repository and work on a branch.
4. Open a pull request referencing the issue.

## Style — Core Library

The core library is held to a high standard. The rules below are not negotiable.

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

Prefer dispatch tables, variant visitors, or other structured approaches over long chains of `if`/`else if`. This applies in both core and UI layers.

### Ternary expressions

Prefer ternary expressions over `if` statements for value selection. Use `if` statements when side effects are involved.

## Style — UI Layers

UI contribution guidelines are forthcoming.
The tab/space, naming, brace, and documentation comment rules above apply universally across all layers.
