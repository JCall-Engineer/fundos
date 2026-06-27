# Translating FundOS

FundOS uses Qt Linguist for translations. Each language is a `.ts` file under `translations/`.

## Adding a new language

Add a `TS_FILES` entry to the `qt_add_translations` call in `desktop/CMakeLists.txt`:

```cmake
qt_add_translations(fundos
	TS_FILES
		${CMAKE_CURRENT_SOURCE_DIR}/translations/fundos_en.ts
		${CMAKE_CURRENT_SOURCE_DIR}/translations/fundos_fr.ts
	LUPDATE_OPTIONS -no-obsolete
)
```

Then run the update target to generate the new `.ts` file:

```sh
cmake --build --preset <your-preset> --target update_translations
```

## Editing translations

`.ts` files are XML. They can be edited directly or with Qt Linguist, which is installed to the `bin/` directory of the Qt development toolchain.

If editing the XML directly:

- Use XML escape sequences for special characters: `&quot;` `&apos;` `&amp;` `&lt;` `&gt;`
- Preserve the formatting lupdate generates — unnecessary whitespace changes produce noisy diffs on the next lupdate run.

## Keeping translations in sync

When source strings change, run the update target again to sync the `.ts` files:

```sh
cmake --build --preset <your-preset> --target update_translations
```

lupdate will attempt to match mutated strings to their previous translations and mark them as unfinished rather than deleting them, but this is heuristic and not guaranteed.
Orphaned translations are deleted.
This is why any commit that adds or changes `tr()` strings must proofread them before committing — a string corrected after the fact may orphan translations that were already contributed.

## Submitting a translation

Follow the same process as any other contribution: open an issue first, then open a pull request with the updated `.ts` file.
