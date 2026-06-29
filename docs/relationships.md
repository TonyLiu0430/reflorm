# 關聯與 Reference Key 規劃

本文件描述第一版關聯 metadata。目標是先讓 model field 可以用 annotation 標示 reference key，並在 `register_namespace` 階段自動驗證 reference 是否能對應到已註冊 model 的欄位。

## 目前範圍

- 用 annotation 標示 local foreign-key field 指向的 target key。
- target key 使用 `cpporm::fields<Model>.field` 指定，不用字串。
- `register_namespace(^^ns)` 會驗證 reference metadata。
- registry descriptor 會保留 reference 的 model/table/member/column 名稱。

## 非目標

- 不產生 `JOIN` SQL。
- 不產生 relationship property。
- 不處理 eager loading / lazy loading。
- 不處理 cascade rule。
- 不處理 runtime DB schema introspection。

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
```

`post::user_id` 表示 local column `author_id` reference 到 `user::id`，而 `user::id` 的 DB column name 由 `column` annotation 決定為 `user_id`。

## 註冊期驗證

`register_namespace(^^models)` 會做以下檢查：

- referenced model 必須是同一個 namespace 直接註冊的 struct。
- referenced field 不可是 `[[=cpporm::ignore{}]]`。
- local field type 必須與 referenced field type 相同。
- 同一個 field 不可重複標示多個 `references` annotation。

第一版要求 referenced model 已經是完整型別，因為 `cpporm::fields<user>.id` 需要從完整 model 產生 field accessor。

## Descriptor

被標示 reference 的 field descriptor 會帶有：

```cpp
field.has_reference
field.referenced_model_name
field.referenced_table_name
field.referenced_member_name
field.referenced_column_name
```

這些 metadata 後續可供 migration、join builder 或 schema generator 使用。

## 設計決策

reference target 使用 `cpporm::fields<user>.id`，而不是字串或 `^^user::id`：

- 避免字串拼錯後只在 runtime 發現。
- 避免在公開 DSL 中暴露非必要的 reflection spelling。
- target field 的 C++ 存在性與型別可在編譯期驗證。

目前 annotation 採用 template form：

```cpp
cpporm::references<cpporm::fields<user>.id>{}
```

原因是 GCC 16 snapshot 對 annotation payload 中直接存 `std::meta::info` 的 runtime materialization 不穩定；使用 template annotation 可讓 target field 由 annotation type 攜帶。
