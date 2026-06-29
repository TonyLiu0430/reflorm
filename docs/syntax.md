# cpporm 語法草案

這份文件描述 annotation-first 的使用者端語法目標。實作應先符合本文件，再補程式碼。

如果想先看偏使用者視角的完整範例導覽，見 `docs/examples/usage-examples.md`。

## 核心方向

- 使用 C++26 reflection 探測 namespace、struct 與 field。
- 使用 P3394 annotations 表達 ORM metadata。
- 使用者不應逐一手動註冊每個 model type。
- 使用者應傳入 namespace reflection，讓 cpporm 掃描該 namespace 下的 struct。
- 傳統 `mapping<T>`、`table<T>(...)`、`field<&T::x>` 只能視為過渡 scaffold。

## 基本 Model

```cpp
namespace models {
struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::unique{}]]
    std::string email;

    [[=cpporm::column{"display_name"}]]
    std::string name;
};
}
```

規則：

- `[[=cpporm::table{"users"}]]` 指定 table 名稱。
- `[[=cpporm::column{"user_id"}]]` 指定 column 名稱。
- `[[=cpporm::id{}]]` 指定 primary key。
- `[[=cpporm::unique{}]]` 指定單欄位 unique constraint。
- 沒有 `column` annotation 的 field 預設使用反射取得的 field identifier。

## Namespace 註冊

```cpp
constexpr auto registry = cpporm::register_namespace(^^models);
```

規則：

- `register_namespace` 是 `consteval` 函數。
- 參數是 namespace reflection，也就是 `std::meta::info`。
- C++ namespace 不能當一般值傳入，因此 API 必須使用 `^^models`。
- 初始版本只掃描 direct namespace members。
- nested namespace 是否遞迴掃描之後再設計。

## 自動發現 Struct

`register_namespace(^^models)` 應掃描 namespace 成員，挑出符合條件的 struct：

```cpp
std::meta::is_type(member)
    && std::meta::is_class_type(member)
    && std::meta::is_complete_type(member)
    && !std::meta::is_union_type(member)
```

所有符合條件的 direct struct 都會納入 registry。`table` annotation 只用來覆寫 table 名稱；沒有 `table` annotation 時，table 名稱預設使用 struct identifier。

## 忽略欄位

```cpp
namespace models {
struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::ignore{}]]
    bool dirty;
};
}
```

## 複合 Constraint

複合 key / unique 不放在 `struct [[=...]]` 位置，也不使用 non-static dummy member。使用 `static constexpr cpporm::model_constraint` 承載 annotation：

```cpp
namespace models {
struct project {
    std::int64_t tenant_id;
    std::string slug;
    std::string name;

    [[=cpporm::id{"tenant_id", "slug"}]]
    static constexpr cpporm::model_constraint _id{};

    [[=cpporm::unique{"tenant_id", "name"}]]
    static constexpr cpporm::model_constraint _unique_name{};
};
}
```

規則：

- 單欄位 constraint 放在 field annotation。
- 複合 constraint 放在 model block 底部的 `static constexpr` dummy member。
- `model_constraint` dummy 不佔 instance layout，不產生 column。
- annotation payload 內的 field name 會在 compile time 檢查。

規則：

- `ignore` annotation 排除該 field 的 ORM persistence metadata。
- 被忽略的 field 仍然是一般 C++ 成員。

## 字串 Metadata

annotation payload 需要是 structural type。不要使用 `char const*` 作為 metadata payload。

目前實作使用 structural fixed string 作為 annotation 物件的 payload：

```cpp
struct table {
    cpporm::fixed_string name;
};

struct column {
    cpporm::fixed_string name;
};
```

使用方式：

```cpp
[[=cpporm::table{"users"}]]
[[=cpporm::column{"user_id"}]]
```

## 關聯

關聯由 DB-level `references` annotation 搭配 domain-level relation field 描述。詳細規劃見 `docs/relationships.md` 與 `docs/relations.md`。

基本方向：

```cpp
struct post;

struct [[=cpporm::table{"users"}]] user {
    std::int64_t id;
    cpporm::has_many<post> posts;
};

struct [[=cpporm::table{"posts"}]] post {
    [[=cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;

    cpporm::relation<user> author;
};
```

## Select

`select` 功能應在編譯期決定欄位集合，並產生只包含 selected fields 的回傳 struct。未被 select 到的欄位不得出現在回傳型別中。

詳細規劃見 `docs/select.md`。

## SQL Generator

基本 SQL generator 先以 SQLite 為目標，主要語法是 `cpporm::sqlite::select<...>()`。

詳細規劃見 `docs/sql-generator.md`。

完整 query builder 規劃見 `docs/query.md`。

## 目前限制

- 目前只確認 GCC 16 snapshot 支援 `[[=...]]`、`annotations_of`、`annotations_of_with_type`。
- `annotations_of_with_type` 需要精確 annotation type；目前 `table` / `column` 使用固定 annotation type 搭配 structural payload，因此可直接用 `annotations_of_with_type` 篩選。
- 初始範例假設 model 欄位是 public。
- private field mapping 之後可再評估是否透過 `std::meta::access_context::unchecked()` 支援。
