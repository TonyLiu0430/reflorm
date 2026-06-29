# Schema Generator 規劃

本文件描述 SQLite `CREATE TABLE` generator。它只根據 model metadata 產生 desired schema SQL，不做 auto migrate。

## 目前範圍

- `cpporm::sqlite::create_table<Model>()` 產生單一 model 的 `CREATE TABLE` SQL。
- `cpporm::sqlite::create_table_if_not_exists<Model>()` 產生安全建表 SQL。
- `cpporm::sqlite::create_schema<^^namespace>()` 產生 namespace 內所有 direct model 的 schema plan。
- 支援 `table`、`column`、`primary_key`、`ignore`。
- 支援 `references<cpporm::fields<T>.id>{}` 產生 foreign key constraint。
- relation marker fields：`relation<T>`、`has_many<T>` 不產生 column。
- 第一版所有非 primary-key scalar column 都產生 `NOT NULL`。

## 非目標

- 不做 auto migrate。
- 不做 schema diff。
- 不做 table/column rename 偵測。
- 不做 destructive migration。
- 不支援 composite key。
- 不支援 index/unique/check/default value。
- 不支援 nullable/optional column。

## 基本語法

```cpp
constexpr auto sql = cpporm::sqlite::create_table<user>();
```

範例輸出：

```sql
CREATE TABLE "users" ("user_id" INTEGER PRIMARY KEY, "email" TEXT NOT NULL)
```

有 reference 時：

```cpp
constexpr auto sql = cpporm::sqlite::create_table<post>();
```

```sql
CREATE TABLE "posts" ("post_id" INTEGER PRIMARY KEY, "author_id" INTEGER NOT NULL, "title" TEXT NOT NULL, FOREIGN KEY ("author_id") REFERENCES "users" ("user_id"))
```

## Schema Plan

`create_schema` 用 namespace reflection 指定 model 集合：

```cpp
constexpr auto schema = cpporm::sqlite::create_schema<^^models>();
```

回傳：

```cpp
cpporm::schema_plan
```

其中包含：

```cpp
schema.statement_count
schema.statements[index]
```

`create_schema` 產生的是 `CREATE TABLE IF NOT EXISTS`，適合空 DB 或缺表補建，不會修改既有 table definition。

## Type Mapping

SQLite 第一版 mapping：

- integral -> `INTEGER`
- floating-point -> `REAL`
- `std::string` -> `TEXT`
- `std::vector<std::byte>` -> `BLOB`
- `std::vector<unsigned char>` -> `BLOB`
- `std::optional<T>` -> nullable version of `T`

非 `std::optional<T>` 且非 primary key 的欄位會產生 `NOT NULL`。`std::optional<T>` 欄位不產生 `NOT NULL`。

未支援型別應在 compile-time 報錯。

## Auto Migrate

不要把 `create_table` / `create_schema` 稱為 auto migrate。它們只會產生 desired schema 的建立語句。Auto migrate 需要：

- introspection：讀取目前 DB schema。
- diff：比較 desired schema 與 current schema。
- planner：產生 migration plan。
- safety policy：rename/drop/type change 是否允許。

這些之後應以 dry-run plan 形式先實作，不應一開始自動套用 destructive SQL。
