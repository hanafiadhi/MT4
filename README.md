## Disclaimer

- Compiling on **MacBook Pro (Retina, 15-inch, Mid 2015)** for Windows 32-bit target
- Generates a `.dll` with `.def` file to avoid name mangling
- Intended for use **only** on Windows MetaTrader 4 (MT4)
- Trade Logging Interval (#CCA): Default is every 5 seconds.
- Exposure Logging Interval (#XSG): Default is every 30 seconds.
- Order Types: Supports only market orders (buy or sell), not pending orders.
- Upsert Behavior:
      - If new data arrives for a trade or exposure record, it replaces the existing entry (update).
      - If no existing record is found, it inserts a new entry.
- Exposure Handling: Clears all previous exposure data when the EA starts.
- EA Replacement Scenario: If the EA is changed while trades are still open, those open trades remain unchanged in the SQLite database.
---

## Prerequisites

1. **Homebrew** (or equivalent) installed on macOS
2. **Mingw-w64** toolchain for cross-compiling:
   ```sh
   brew install mingw-w64
   ```
3. SQLite amalgamation source (e.g., `sqlite-amalgamation-3490100` folder)
4. `sqlite_integrated.c` source file in your project root

---

## Directory Structure

```text
project-root/
├── sqlite_integrated.c
├── sqlite-amalgamation-3490100/
│   └── sqlite3.c
│   └── sqlite3.h
│   └── sqlite3ext.h
└── build/
```

*(Create a `build/` folder for output artifacts)*

---

## Build Command

Run the following command from `project-root` to compile and generate `sqlite_32.dll` and its import definition `sqlite.def`:

```sh
# Cross-compile to 32-bit Windows DLL
i686-w64-mingw32-gcc -shared \
  -I./sqlite-amalgamation-3490100 \
  -o build/sqlite_32.dll \
  sqlite_integrated.c sqlite-amalgamation-3490100/sqlite3.c \
  -Wl,--add-stdcall-alias \
  -Wl,--output-def,build/sqlite.def \
  -static-libgcc \
  -static-libstdc++
```

### Flags Explanation

- `-shared`: Create a shared library (`.dll`)
- `-I./sqlite-amalgamation-3490100`: Include path for SQLite headers
- `-o build/sqlite_32.dll`: Output DLL file
- `-Wl,--add-stdcall-alias`: Add stdcall aliases to export table
- `-Wl,--output-def,build/sqlite.def`: Generate `.def` file for import definitions
- `-static-libgcc -static-libstdc++`: Link C/C++ runtime statically

---

## Usage in MT4

1. Copy `build/sqlite_32.dll` and `build/sqlite.def` into your MT4 `Libraries/` folder.
2. In your `.mq4` script (e.g., `trade&exposure_v2.mq4`), declare DLL imports:
   ```mql4
   #import "sqlite_32.dll"
   void upsertTradeBinary(uchar &data[], int length);
   // ... other function imports
   #import
   ```
3. Compile your MQL4 script and run in MT4.

---

## License

This project is released under the [MIT License](LICENSE).
