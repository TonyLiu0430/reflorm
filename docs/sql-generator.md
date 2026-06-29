# SQL Generator 規劃

本文件描述基本 SQL generator 的方向。第一個目標 DBMS 是 SQLite，但設計需保留支援多個 DBMS 的空間。

## 目前範圍

- 支援基本 `SELECT ... FROM ...`。
- 支援 `WHERE`、`ORDER BY`、`LIMIT`、`OFFSET` 的第一版 query builder。
- 支援 SQLite identifier quoting。
- SQL 在編譯期由 model reflection 與 annotations 產生。
- 不處理 database connection 或 query execution。
- 不處理 `JOIN`。

## 基本語法

```cpp
using models::user;

constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
);
```

若 model 定義如下：

```cpp
struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}]]
    std::int64_t id;

    [[=cpporm::column{"display_name"}]]
    std::string name;
};
```

則 SQLite SQL 應為：

```sql
SELECT "user_id", "display_name" FROM "users"
```

## Dialect

SQLite dialect 應由 `cpporm::sqlite` 本身指定：

```cpp
constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id);
```

`cpporm::select` 可作為目前 SQLite 預設捷徑，但多 DBMS 情境下應優先使用 dialect type：

```cpp
constexpr auto query = cpporm::select<user>(cpporm::fields<user>.id);
```

未來新增其他 DBMS 時，應新增 dialect type，不應把不同 DBMS 的規則混在同一個 generator 裡。

## SQLite Identifier Quoting

SQLite 使用 double quote 包住 identifiers：

```sql
"users"
"user_id"
```

identifier 內若出現 double quote，需以兩個 double quote escape。

## Select 結果型別

SQL generator 與 projection type 必須一致。

```cpp
constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
);

using row = typename decltype(query)::row_type;
```

`row` 只包含 `id` 與 `name`，未 select 的欄位不得出現在型別中。

詳細 projection 規則見 `docs/select.md`。

## 編譯期驗證

SQL generator 應沿用 select 的編譯期驗證：

- model 必須是完整 class type。
- selected fields 必須屬於 model。
- 不允許 duplicate fields。
- 不允許 select `[[=cpporm::ignore{}]]` field。

完整 query builder 規劃見 `docs/query.md`。
