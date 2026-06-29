# Nested Select 規劃

本文件描述 Prisma-like nested select 的第一版方向。Nested select 是 relation query 的主要 public API；`include` 之後可以做成 shorthand，但不作為第一版核心。

## 目前範圍

- `find_many<Model>()` 建立 model query。
- root `.select(...)` 可同時接 root fields 與 relation nested select。
- relation nested select 使用 `cpporm::relations<Model>.name.select(...)`。
- 會產生 query plan metadata；SQLite runtime 目前可執行一層 nested select。
- `has_many` nested select 使用 split-query executor，避免 join 造成笛卡爾積。
- `relation<T>` nested select materialize 為 `std::optional<child_row>`。

## 基本語法

```cpp
using namespace models;

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

Plan 會包含：

```sql
SELECT "user_id", "email" FROM "users"
SELECT "post_id", "title" FROM "posts" WHERE "author_id" IN (?)
```

第二個 query 的 `?` 代表 batched parent ids。SQLite runtime 執行時會依 root query 結果動態展開實際 placeholder 數量，例如 `IN (?, ?, ?)`。

Runtime 第一版要求 relation lookup 需要的 root key 明確出現在 root select 內：

- `has_many<T>` 需要 source model 的 referenced key，例如 `user.id`。
- `relation<T>` 需要 source model 的 local FK，例如 `post.user_id`。

這個限制讓 public result row 不需要藏額外欄位。之後可再加入 hidden-key plan，讓 executor 自動補查所需欄位但不暴露到 result row。

## 為什麼不是 JOIN

Nested `has_many` 如果用單一 join row set：

```sql
SELECT ... FROM users JOIN posts ...
```

會讓一個 user 對多筆 posts 時 root row 重複出現。多個 collection relation 同時 select 時，row 數會互相相乘，形成笛卡爾積爆炸。

因此第一版 nested select 的執行策略是：

- root model 一個 query。
- 每個 `has_many` relation 一個 batched relation query。
- materializer 用 relation key 把 child rows group 回 parent rows。

## 非目標

- 不立即實作多層 nested select。
- 不立即實作 `include` shorthand。
- 不立即支援 composite key。
- 不立即支援 hidden-key select。

## 編譯期驗證

`.select(...)` 應檢查：

- root field 必須屬於 root model。
- nested relation 必須屬於 root model。
- nested selected fields 必須屬於 relation target model。
- relation 必須能由 registry metadata 找到對應 FK/reference。
