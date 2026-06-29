# Query Builder 規劃

本文件描述第一版完整查詢 builder。目標是先產生可預期的 SQLite SQL 字串、typed bindings 與正確 projection row type，不處理 DB 連線或實際執行。

## 目前範圍

- `SELECT selected_fields`
- `FROM table`
- `WHERE predicate_expression`
- `AND` / `OR` / `NOT` predicate 組合
- `LIKE` comparison
- `ORDER BY field ASC|DESC`
- 多個 `ORDER BY` 以 `,` 連接
- `LIMIT N`
- `OFFSET N`

## 非目標

- 不支援 join。
- 不支援 aggregate expression。
- 不支援 runtime 動態欄位選擇。
- 不支援實際 DB 執行；目前只建立 SQL 與 typed bindings。

## 基本語法

```cpp
using models::user;

constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
)
    .where(
        cpporm::fields<user>.email == "tony@example.com"
        && cpporm::fields<user>.id >= 10
    )
    .order_by(cpporm::desc(cpporm::fields<user>.name))
    .limit<10>()
    .offset<20>();
```

SQLite SQL：

```sql
SELECT "user_id", "display_name" FROM "users" WHERE ("email" = ? AND "user_id" >= ?) ORDER BY "display_name" DESC LIMIT 10 OFFSET 20
```

多 DBMS 情境下，正式語法應由 dialect type 指定：

```cpp
using models::user;

constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
)
    .where(cpporm::fields<user>.email == "tony@example.com")
    .limit<10>();
```

## Predicate

第一版支援以下 comparison predicate：

```cpp
cpporm::fields<user>.id == 1
cpporm::fields<user>.id != 1
cpporm::fields<user>.id < 10
cpporm::fields<user>.id <= 10
cpporm::fields<user>.id > 0
cpporm::fields<user>.id >= 0
cpporm::fields<user>.name.like("%Tony%")
```

SQLite 對應：

```sql
=
<>
<
<=
>
>=
LIKE
```

predicate 持有 typed bind value。SQL generator 仍產生 placeholder `?`，實際 executor 之後會使用 query 的 bindings 進行綁定。

多個條件可用 C++ operator 組成 predicate AST：

```cpp
using models::user;

constexpr auto active_user =
    cpporm::fields<user>.email == "tony@example.com" && cpporm::fields<user>.id >= 10;

constexpr auto name_matches = cpporm::fields<user>.name.like("%Tony%");

auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
    .where(active_user || name_matches);
```

SQL：

```sql
SELECT "user_id" FROM "users" WHERE (("email" = ? AND "user_id" >= ?) OR "display_name" LIKE ?)
```

若條件較多，也可以用 `all(...)`、`any(...)` 與 `!` 表達 Prisma-like 的 AND / OR / NOT：

```cpp
constexpr auto email_or_name_matches = cpporm::any(
    cpporm::fields<user>.email == "tony@example.com",
    cpporm::fields<user>.name.like("%Tony%")
);

constexpr auto id_is_not_too_small = !(cpporm::fields<user>.id < 10);

auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
    .where(cpporm::all(email_or_name_matches, id_is_not_too_small));
```

SQL：

```sql
SELECT "user_id" FROM "users" WHERE (("email" = ? OR "display_name" LIKE ?) AND NOT ("user_id" < ?))
```

例如：

```cpp
auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
    .where(
        cpporm::fields<user>.email == "tony@example.com"
        && cpporm::fields<user>.id >= std::int64_t{10}
    );
```

SQL：

```sql
SELECT "user_id" FROM "users" WHERE ("email" = ? AND "user_id" >= ?)
```

bindings 型別概念上是：

```cpp
std::tuple<std::string_view, std::int64_t>
```

整數欄位可以接受任意 integral value，並在 predicate 建立時正規化成欄位型別：

```cpp
.where(cpporm::fields<user>.id >= 10)
.where(cpporm::fields<user>.id >= 10u)
.where(cpporm::fields<user>.id >= 10ll)
```

浮點數不可直接與整數欄位比較：

```cpp
.where(cpporm::fields<user>.id >= 10.5) // 不合法
```

即使 value 是編譯期常量，第一版仍不把 value inline 進 SQL。原因是 SQL literal escaping 與 injection 規則會因 DBMS 而異；統一使用 placeholder 能讓 SQL generator 與未來 prepared statement executor 對齊。

## Order By

```cpp
query.order_by(cpporm::asc(cpporm::fields<user>.id))
query.order_by(cpporm::desc(cpporm::fields<user>.name))
```

## 呼叫順序

第一版要求 builder 依 SQL clause 順序呼叫：

```cpp
select().where(...).order_by(...).limit<N>().offset<N>()
```

不支援先 `limit` 後 `where`，也不支援先 `offset` 後 `limit`。

這是第一版簡化；未來可以改為儲存 query AST 後統一 render SQL。

## 編譯期驗證

- `where` 與 `order_by` 的 field 必須屬於 model。
- `where` 與 `order_by` 不可使用 `[[=cpporm::ignore{}]]` field。
- `limit` 不能重複指定。
- `offset` 不能重複指定。
- `offset` 必須在 `limit` 之後指定。

## Dialect 擴充

目前預設 dialect 是 SQLite。Dialect 至少要提供：

```cpp
static consteval auto append_identifier(sql_string&, std::string_view) -> void;
static consteval auto append_parameter(sql_string&) -> void;
```

未來支援其他 DBMS 時，應新增 dialect type，而不是把不同 DBMS 的行為塞進 SQLite generator。

## Materialization

query 會暴露 projection row type：

```cpp
using row = typename decltype(query)::type;
```

多筆結果可以選擇任何 `Container<row>` 後具有 `begin/end` 的容器：

```cpp
auto rows = query.as<std::vector>();
auto rows = query.as<std::list>();
```

`as<std::vector>()` 與 `as<std::list>()` 都只是 materialization policy。未來 executor 執行後的 result type 應是：

```cpp
std::expected<std::vector<row>, cpporm::query_error>
std::expected<std::list<row>, cpporm::query_error>
```

固定長度結果使用 `std::array`，長度由 compile-time `limit<N>()` 推斷，不由使用者在 `as` 裡手動填：

```cpp
auto rows = query.limit<3>().as<std::array>();
```

其 result type 應是：

```cpp
std::expected<std::array<row, 3>, cpporm::query_error>
```

如果實際 row 數不足或超過 `N`，executor 應回傳 `std::unexpected(cpporm::query_error{...})`，不得丟 runtime exception，也不得回傳錯誤碼。

## 錯誤處理

runtime 錯誤一律使用現代 C++ vocabulary types：

- 可恢復錯誤使用 `std::expected<T, cpporm::query_error>`。
- 可缺值但非錯誤的情境可使用 `std::optional<T>`。
- 不使用錯誤碼作為主要 API。
- 不使用 runtime exception 作為一般查詢錯誤處理。

編譯期錯誤，例如欄位不存在、select 到 ignored field、query clause 順序錯誤，應在 constant evaluation 階段診斷。
