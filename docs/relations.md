# Relation Field 規劃

本文件描述 Prisma-like relation field 的第一版方向。`references` annotation 只描述 DB-level foreign key；relation field 則描述 domain/query API 可使用的關聯入口。

## 目前範圍

- 支援 relation marker field：`cpporm::relation<T>` 與 `cpporm::has_many<T>`。
- relation field 不對應 DB column。
- `register_namespace(^^ns)` 讀出 relation metadata。
- relation metadata 可連回 local foreign key 與 target key。
- include/query executor 設計預設採用 split-query/batched load，避免一對多 join 造成笛卡爾積爆炸。

## 非目標

- 不立即實作 runtime DB executor。
- 不立即實作 nested include materialization。
- 不把 `has_many` include 直接展開成單一 join row set。
- 不支援多欄位 composite relation。

## 基本語法

```cpp
namespace models {

struct [[=cpporm::table{"users"}]] user;

struct [[=cpporm::table{"posts"}]] post {
    [[=cpporm::column{"post_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    [[=cpporm::column{"author_id"}, =cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;

    cpporm::relation<user> author;
};

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    cpporm::has_many<post> posts;
};

}
```

第一版 relation matching 使用既有 `references` metadata：

- `post::author` 的 target model 是 `user`。
- `post::user_id` reference 到 `user::id`。
- registry 可由 `relation<user>` 找到唯一 local FK `post::user_id`。
- `user::posts` 可由 `post` 上指向 `user` 的 reference 反推出反向關聯。

## Include 策略

不把 `has_many` include 預設轉成單一 join：

```cpp
cpporm::sqlite::find_many<user>()
    .include(cpporm::relations<user>.posts);
```

預期 executor plan 是 split query：

```sql
SELECT ... FROM "users";
SELECT ... FROM "posts" WHERE "author_id" IN (?, ?, ...);
```

這樣避免：

- user 有多篇 post 時主 row 被重複展開。
- 多個 `has_many` include 互相相乘造成笛卡爾積。
- projection row type 被 SQL join row shape 綁死。

`relation` include 可以選擇 join 或 batched load，但 public API 不應要求使用者思考 join type。SQL generator/executor 之後可依 dialect 與 query shape 最佳化。

## 註冊期驗證

`register_namespace(^^models)` 應檢查：

- relation target model 必須在同一 namespace 註冊。
- `relation<T>` 的 source model 必須有唯一 field reference 到 `T`。
- `has_many<T>` 的 target model 必須有唯一 field reference 回 source model。
- relation field 不可同時標成 `column`、`primary_key` 或 `references`。

## Descriptor

relation descriptor 應至少包含：

```cpp
relation.member_name
relation.kind
relation.target_model_name
relation.target_table_name
relation.local_member_name
relation.local_column_name
relation.target_member_name
relation.target_column_name
```

其中 `relation` 的 local field 在 source model；`has_many` 的 local field 在 target model。
