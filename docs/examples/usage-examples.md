# 使用範例導覽

這份文件從使用者視角展示 cpporm 目前想提供的寫法。重點不是列出所有內部設計，而是呈現這個專案的風格：用標準 C++ 型別描述資料模型，優先依賴 reflection 的預設命名，只在需要語意 metadata 或命名覆寫時使用 annotation，再推導出 schema、query、relation 與 result row。

## 風格摘要

- Model 仍是普通 C++ `struct`，不是繼承 ORM base class 的 entity。
- 預設 table/column 名稱來自 C++ identifier；rename 是額外 override，不是每個 model 都要寫。
- 語意 metadata 放在宣告旁邊，使用 `[[=cpporm::...]]` annotation，不需要另外寫一份 mapping table。
- Namespace 是註冊單位，`register_namespace(^^models)` 會掃描 direct structs。
- Query API 用 model type 與 field accessor 表達意圖，例如 `cpporm::fields<user>.email`。
- SQL value 不 inline，永遠產生 placeholder `?` 與 typed bindings。
- Projection result type 由 selected fields 生成，沒 select 的欄位不會出現在 row type 上。
- Relation 查詢以 nested select 為主要風格，避免把 `join` 當成 public 主 API。
- Runtime 錯誤用 `std::expected` 回傳，不把一般查詢錯誤包成 exception 流程。

## Model 宣告

```cpp
#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>

namespace models {

struct post;

struct user {
    [[=cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::unique{}]]
    std::string email;

    cpporm::has_many<post> posts;
};

struct post {
    [[=cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t author_id;

    std::string title;

    cpporm::relation<user> author;
};

} // namespace models
```

這段展示的風格重點：

- `user`、`post` 都是 plain struct。
- table 名稱預設是 struct identifier：`user`、`post`。
- column 名稱預設是 field identifier：`id`、`email`、`author_id`、`title`。
- `id` 是資料語意，不是命名設定；它是 Prisma-like 的 primary key 標記。
- `unique` 標記單欄位唯一性，schema generator 會產生 `UNIQUE`。
- `references` 描述 DB-level foreign key。
- `has_many<T>` 與 `relation<T>` 描述 domain-level relation，不會被當成資料表 column。

## 命名覆寫

如果 DB 命名和 C++ 命名不同，再使用 `table` / `column` annotation 覆寫：

```cpp
struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::column{"display_name"}]]
    std::string name;
};
```

這段不是主要範例的預設寫法。cpporm 的偏好是先讓 C++ identifier 成為 metadata source；只有遇到 legacy schema、複數 table name 或命名慣例不同時才加 rename annotation。

## 複合 Constraint

單欄位 constraint 放在 field annotation 上：

```cpp
struct user {
    [[=cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::unique{}]]
    std::string email;
};
```

複合 constraint 則放在 `static constexpr cpporm::model_constraint` dummy member 上：

```cpp
struct project {
    std::int64_t tenant_id;
    std::string slug;
    std::string name;

    [[=cpporm::id{"tenant_id", "slug"}]]
    static constexpr cpporm::model_constraint _id{};

    [[=cpporm::unique{"tenant_id", "name"}]]
    static constexpr cpporm::model_constraint _unique_name{};
};
```

這個 dummy member 必須是 `static constexpr`，因此不佔每個 entity object 的空間，也不參與 aggregate 初始化、select 或 mutation payload。它的用途只是在 model block 底部承載 Prisma-like 的 `@@id([...])` / `@@unique([...])` 語意。

目前複合 constraint 已支援 schema SQL 產生；mutation 的 `update` / `upsert` 第一版仍只支援單欄位 id。

## Namespace 註冊

```cpp
constexpr auto registry = cpporm::register_namespace(^^models);
```

這裡刻意不是：

```cpp
cpporm::register_model<user>();
cpporm::register_model<post>();
```

cpporm 的方向是讓 C++26 reflection 掃描 namespace 下的 model，減少每新增一個 struct 就要同步更新註冊表的樣板碼。

## Schema 產生

```cpp
constexpr auto schema = cpporm::sqlite::create_schema<^^models>();
```

概念上會得到 safe create plan：

```sql
CREATE TABLE IF NOT EXISTS "user" (...)
CREATE TABLE IF NOT EXISTS "post" (...)
```

目前 schema generator 偏向安全建立，不做 destructive auto migrate。這讓第一版 API 可以先可靠地描述結構，而不是一開始就自動修改既有資料表。

## Flat Select

```cpp
using models::user;

constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.email
)
    .where(cpporm::fields<user>.id >= 10)
    .order_by(cpporm::asc(cpporm::fields<user>.id))
    .limit<20>();
```

產生的 SQL 形狀：

```sql
SELECT "id", "email" FROM "user" WHERE "id" >= ? ORDER BY "id" ASC LIMIT 20
```

這段展示的風格重點：

- Public API 使用 `<user>`，而不是要求使用者到處寫 `<^^user>`。
- 欄位透過 `cpporm::fields<user>.id` 取得，保留 C++ member name 的可讀性。
- SQL 使用 placeholder，value 會進 typed bindings。
- `limit<20>()` 是 compile-time value，之後 `std::array` materialization 可以利用這個大小。

## Projection Row

```cpp
using row = typename decltype(query)::type;
```

`row` 只會包含這次 select 的欄位：

```cpp
row item{};
item.id;
item.email;
// item.posts; // 不存在，因為沒有被 select
```

這是 cpporm 的核心風格之一：select 不是回傳完整 model 再把沒用到的欄位留空，而是直接生成符合 projection 的 row type。

## Predicate

```cpp
auto active_users = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.email
)
    .where(
        cpporm::fields<user>.email.like("%@example.com")
        && cpporm::fields<user>.id >= 100
    );
```

也可以把條件拆開：

```cpp
constexpr auto email_matches = cpporm::fields<user>.email.like("%@example.com");
constexpr auto id_is_valid = cpporm::fields<user>.id >= 100;

auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
    .where(cpporm::all(email_matches, id_is_valid));
```

這段展示的風格重點：

- Predicate 是 C++ expression，不是手寫 SQL fragment。
- Binding type 由欄位型別推導。
- 整數欄位接受 integral value，但不接受 floating-point value 混用。

## Nested Select

```cpp
using models::post;

constexpr auto query = cpporm::sqlite::find_many<user>()
    .select(
        cpporm::fields<user>.id,
        cpporm::fields<user>.email,
        cpporm::relations<user>.posts.select(
            cpporm::fields<post>.id,
            cpporm::fields<post>.title
        )
    );
```

Result row 的形狀概念上是：

```cpp
struct row {
    std::int64_t id;
    std::string email;
    std::vector<post_row> posts;
};
```

這段展示的風格重點：

- Relation query 優先使用 nested select 表達資料形狀。
- `has_many` 不用 join row set 攤平成重複 parent row。
- SQLite runtime 目前使用 split-query：先查 root rows，再用 batched `IN (?, ...)` 查 child rows，最後 group 回 parent。
- 第一版 runtime 要求 relation lookup 需要的 root key 明確出現在 root select，例如這裡的 `user.id`。

反向 relation 也同樣用 nested select：

```cpp
constexpr auto query = cpporm::sqlite::find_many<post>()
    .select(
        cpporm::fields<post>.id,
        cpporm::fields<post>.author_id,
        cpporm::fields<post>.title,
        cpporm::relations<post>.author.select(
            cpporm::fields<user>.id,
            cpporm::fields<user>.email
        )
    );
```

`relation<T>` 的 result 是 `std::optional<child_row>`，如果找不到 target row 就是 `std::nullopt`。

## Runtime 查詢

```cpp
#include <cpporm/sqlite_runtime.hpp>

auto database = cpporm::sqlite::database::open_memory();
if (!database) {
    return std::unexpected(database.error());
}

auto created = database->execute_schema(cpporm::sqlite::create_schema<^^models>());
if (!created) {
    return std::unexpected(created.error());
}

auto rows = database->fetch(
    cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.email
    )
        .where(cpporm::fields<user>.id >= 1)
        .as<std::vector>()
);
```

`rows` 的型別是：

```cpp
std::expected<std::vector<row>, cpporm::query_error>
```

這段展示的風格重點：

- Runtime backend 是 optional，core headers 不強迫帶 SQLite runtime。
- 查詢錯誤是 value，呼叫端用 `std::expected` 處理。
- `as<std::vector>()` 是 materialization policy，不改變 query builder 本身的語意。

## Mutation Payload

```cpp
struct user_email_patch {
    std::int64_t id;
    std::string email;
};

auto updated = database->update<user>(user_email_patch{
    .id = 1,
    .email = "new@example.com"
});
```

也可以用 local aggregate payload：

```cpp
struct {
    std::int64_t id;
    std::string email;
    int ignored_extra;
} payload{1, "tony@example.com", 123};

auto inserted = database->insert<user>(payload);
```

這段展示的風格重點：

- Mutation target 是 model，value 可以是任意同名欄位 payload。
- 額外 payload 欄位會被忽略，不必為每種寫入場景建立完整 model object。
- `update` / `upsert` 要求 primary key，避免沒有條件的危險更新。

## 一個完整流程

```cpp
using models::user;
using models::post;

auto database = cpporm::sqlite::database::open_memory();
if (!database) {
    return std::unexpected(database.error());
}

if (auto result = database->execute_schema(cpporm::sqlite::create_schema<^^models>()); !result) {
    return std::unexpected(result.error());
}

auto inserted_user = database->insert(user{
    .id = 1,
    .email = "tony@example.com"
});
if (!inserted_user) {
    return std::unexpected(inserted_user.error());
}

auto inserted_post = database->insert(post{
    .id = 10,
    .author_id = 1,
    .title = "hello"
});
if (!inserted_post) {
    return std::unexpected(inserted_post.error());
}

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

這個流程把 cpporm 的主要取向串在一起：model 宣告接近普通 C++、metadata 靠 annotation、query 以 typed DSL 表達、runtime 以 `expected` 回報錯誤、relation 以 nested result shape 回到呼叫端。

## 跟傳統 ORM 寫法的差異

cpporm 刻意避免把主要使用方式設計成：

```cpp
template<>
inline constexpr auto mapping<user> = table<user>(
    field<&user::id>("user_id"),
    field<&user::email>("email")
);
```

也避免把 relation 查詢的主要 API 設計成：

```cpp
select<user>().join<post>().where(...)
```

不是因為這些能力完全不能存在，而是 public 主路徑應更貼近 C++26 reflection 能提供的優勢：

- 宣告即 metadata source。
- 欄位與 relation 都能由型別推導。
- Result shape 由 select shape 推導。
- SQL generator 和 runtime executor 可以共享同一份 typed query metadata。
