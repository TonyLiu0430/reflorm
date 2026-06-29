# Join 規劃

本文件描述第一版 join builder。目標是先利用 `references` annotation 產生明確的 SQL `JOIN ... ON ...`，不先處理跨表 projection 或 relation object。

## 目前範圍

- 從 base model 對 referenced target model 做 inner join。
- join 條件由 local field 的 `cpporm::references<cpporm::fields<Target>.id>{}` annotation 推導。
- `.join<Target>()` 會在編譯期檢查 base model 是否有唯一 reference 指向 `Target`。
- join clause 產生 SQLite SQL。

## 非目標

- 不支援跨表 projection row type。
- 不支援 left/right/full join。
- 不支援多欄位 composite key。
- 不支援同一 base model 對同一 target model 有多個 reference 的自動判斷。
- 不支援 runtime 動態 join。

## 基本語法

```cpp
namespace models {

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;
};

struct [[=cpporm::table{"posts"}]] post {
    [[=cpporm::column{"post_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    [[=cpporm::column{"author_id"}, =cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;
};

}

using namespace models;

constexpr auto query = cpporm::sqlite::select<post>(cpporm::fields<post>.id)
    .join<user>();
```

SQL：

```sql
SELECT "post_id" FROM "posts" JOIN "users" ON "posts"."author_id" = "users"."user_id"
```

## 編譯期驗證

`.join<Target>()` 會做以下檢查：

- base model 必須有 field 標示 `references<cpporm::fields<Target>....>{}`。
- target model 必須是完整 class type。
- 找到的 reference 必須唯一。
- referenced field 不可為 `ignore`。
- local field type 必須與 referenced field type 一致。

第一版只支援 base model 持有 FK 的方向，例如 `post -> user`。反向 join、同表多關聯與顯式 relation name 之後再設計。
