# Mutations 規劃

本文件描述 SQLite runtime 第一版 mutation API。目標是先支援 model target + arbitrary payload struct 的 insert / update / upsert，不先做 nested write。

## 目前範圍

- `database.insert(model)` 作為完整 model object shorthand。
- `database.insert<Model>(values)` 支援任意 payload struct。
- `database.update<Model>(values)` 支援任意 payload struct。
- `database.upsert<Model>(values)` 支援任意 payload struct。
- 使用 target model reflection 決定 table/column，使用 payload struct 中同名欄位提供 value。
- payload 可以有額外欄位；ORM 會忽略沒有對應到 target model 的欄位。
- `update` / `upsert` 要求 exactly one primary key。
- mutation 使用 prepared statement 與 typed bindings。

## 基本語法

```cpp
auto db = cpporm::sqlite::database::open_memory();

user value{
    .id = 1,
    .email = "tony@example.com"
};

auto inserted = db->insert(value);
auto updated = db->update(user{.id = 1, .email = "new@example.com"});
auto upserted = db->upsert(user{.id = 1, .email = "final@example.com"});
```

也可以傳入任意 struct，只要欄位名稱能對應 target model：

```cpp
struct {
    std::int64_t id;
    std::string email;
    int ignored_extra;
} payload{1, "tony@example.com", 123};

auto inserted = db->insert<user>(payload);
```

這會使用 `user` 的 table/column metadata，但只綁定 payload 中對應到 `user` model 的欄位。`ignored_extra` 不會進 SQL。

Partial update 使用 payload 的欄位決定 `SET` clause，但 payload 必須包含 primary key：

```cpp
struct user_email_patch {
    std::int64_t id;
    std::string email;
};

auto updated = db->update<user>(user_email_patch{.id = 1, .email = "new@example.com"});
```

## SQLite SQL

`insert(user)`：

```sql
INSERT INTO "users" ("user_id", "email") VALUES (?, ?)
```

`update(user)`：

```sql
UPDATE "users" SET "email" = ? WHERE "user_id" = ?
```

`upsert(user)`：

```sql
INSERT INTO "users" ("user_id", "email") VALUES (?, ?) ON CONFLICT ("user_id") DO UPDATE SET "email" = excluded."email"
```

## 非目標

- 不支援 nested write。
- 不支援 bulk insert。
- 不支援 composite primary key。
- 不支援 generated/default column policy。
- 不支援 return inserted row。

這些之後應另外設計，避免把基本 runtime mutation 和完整 data mapper 一次混在一起。
