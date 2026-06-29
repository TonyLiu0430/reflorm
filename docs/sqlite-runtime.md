# SQLite Runtime 規劃

本文件描述第一版 SQLite runtime backend。核心 ORM 仍維持 header-only；SQLite runtime 是 optional target，只有啟用 `CPPORM_ENABLE_SQLITE_RUNTIME` 時才會建置。

## 啟用方式

```powershell
cmake --preset gcc16-debug -DCPPORM_ENABLE_SQLITE_RUNTIME=ON
cmake --build --preset gcc16-debug
ctest --preset gcc16-debug
```

啟用後 CMake 會透過 FetchContent 取得 SQLite amalgamation，並建立：

```cmake
cpporm::sqlite_runtime
```

使用 runtime API 時 include：

```cpp
#include <cpporm/sqlite_runtime.hpp>
```

## 目前範圍

- `cpporm::sqlite::database::open(path)`。
- `cpporm::sqlite::database::open_memory()`。
- `database.execute(sql)`。
- `database.execute_schema(schema_plan)`。
- flat `select(...).as<std::vector>()` 查詢。
- 一層 `find_many(...).select(...nested...).as<std::vector>()` nested select 查詢。
- prepared statement。
- typed bindings。
- row materialization。

## 基本語法

```cpp
auto database = cpporm::sqlite::database::open_memory();
if (!database) {
    return std::unexpected(database.error());
}

auto rows = database->fetch(
    cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.email
    )
        .where(cpporm::fields<user>.id >= 10)
        .as<std::vector>()
);
```

也可以執行 ORM 產生的 schema plan：

```cpp
auto result = database->execute_schema(
    cpporm::sqlite::create_schema<^^models>()
);
```

這只會執行 `CREATE TABLE IF NOT EXISTS`，不會自動修改既有 table。

Nested select 可執行一層 `has_many<T>` 或 `relation<T>`：

```cpp
auto rows = database->fetch(
    cpporm::sqlite::find_many<user>()
        .select(
            cpporm::fields<user>.id,
            cpporm::fields<user>.email,
            cpporm::relations<user>.posts.select(
                cpporm::fields<post>.id,
                cpporm::fields<post>.title
            )
        )
        .as<std::vector>()
);
```

目前 runtime 會執行 root query，再用 batched `IN (?, ...)` 查 relation rows 並 group 回 parent。`has_many<T>` result 是 `std::vector<child_row>`；`relation<T>` result 是 `std::optional<child_row>`。relation lookup 所需 root key 必須明確出現在 root select 內。

`rows` 的型別是：

```cpp
std::expected<std::vector<row>, cpporm::query_error>
```

## 型別支援

Runtime 支援 SQLite storage classes 的基本 C++ mapping：

- `NULL`：`std::optional<T>` result/binding、`std::nullptr_t` binding。
- `INTEGER`：integral types。
- `REAL`：floating-point types。
- `TEXT`：`std::string` result/binding、`std::string_view` binding。
- `BLOB`：`std::vector<std::byte>`、`std::vector<unsigned char>`。

暫不支援：

- date/time。
- enum。
- 多層 nested select executor。
- `std::array` materialization executor。

## 錯誤處理

Runtime 錯誤使用：

```cpp
std::expected<T, cpporm::query_error>
```

一般查詢錯誤不丟 runtime exception。

## 後續方向

下一步應補：

- `as<std::array>()` executor。
- hidden-key nested select plan。
- transaction API。
- prepared statement cache。
