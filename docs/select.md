# Select 功能規劃

本文件規劃 cpporm 的 `select` 功能。目前已實作基本 projection type 與 SQLite `SELECT` SQL 產生；後續擴充仍應先更新本文件。

## 目標

- `select` 的欄位集合必須在編譯期決定。
- 回傳 row type 必須是編譯期產生的 struct。
- 沒有被 select 到的欄位，不可以出現在回傳 struct 中。
- SQL column name 由 model field 的 annotation metadata 決定。
- 回傳 struct 的 C++ field name 預設沿用原 model field identifier。

## 非目標

- 本階段不規劃 join。
- 本階段不規劃 aggregate expression，例如 `count(*)`。
- 本階段不規劃 runtime 動態欄位選擇。
- 本階段不規劃把未 select 欄位包成 `std::optional` 或設成 default value；未 select 就是不在型別裡。

## 基本語法

```cpp
namespace models {
struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::column{"display_name"}]]
    std::string name;

    std::string email;
};
}

using models::user;

constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
);
using row = typename decltype(query)::row_type;
```

`row` 的等價結構應接近：

```cpp
struct row {
    std::int64_t id;
    std::string name;
};
```

以下程式應成立：

```cpp
static_assert(requires(row r) { r.id; });
static_assert(requires(row r) { r.name; });
static_assert(!cpporm::has_member<row, "email">());
```

## Query API 草案

低階型別 API 仍由實作內部使用 reflection metadata；公開查詢 API 應優先使用 model type 與自動產生的 field accessors：

```cpp
using models::user;

constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
);
using row = typename decltype(query)::row_type;
```

查詢建構 API：

```cpp
constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
);
```

之後有 database executor 時，執行結果應類似：

```cpp
auto rows = db.fetch(query.as<std::vector>());
auto rows = db.fetch(query.as<std::list>());
auto rows = db.fetch(query.limit<3>().as<std::array>());
```

## 欄位選擇規則

欄位必須用 model type 對應的 field accessor 指定：

```cpp
cpporm::fields<user>.id
```

不使用字串指定 C++ 欄位：

```cpp
cpporm::sqlite::select<user>("id"); // 不採用
```

理由：

- field accessor 可在編譯期驗證欄位存在。
- field accessor 可驗證欄位屬於指定 model。
- 字串應只用於資料庫名稱 metadata，不應用於引用 C++ 欄位。

## SQL Column Name

select SQL 使用 column annotation：

```cpp
struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}]]
    std::int64_t id;
};
```

選擇 `cpporm::fields<user>.id` 時：

- 回傳 struct field name 是 `id`。
- SQL column name 是 `user_id`。

若沒有 `column` annotation：

- 回傳 struct field name 使用 C++ field identifier。
- SQL column name 也使用 C++ field identifier。

## Projection Struct 產生方式

實作應使用 C++26 reflection 的 `std::meta::define_aggregate` 產生 projection struct。

概念上：

```cpp
template<std::meta::info Model, std::meta::info... Fields>
struct select_result {
    struct type;

    consteval {
        std::meta::define_aggregate(^^type, {
            std::meta::data_member_spec(
                std::meta::type_of(Fields),
                {.name = std::meta::identifier_of(Fields)}
            )...
        });
    }
};

template<std::meta::info Model, std::meta::info... Fields>
using select_result_t = typename select_result<Model, Fields...>::type;
```

實際程式碼可能需要因 GCC 16 snapshot 限制調整，但語意必須維持：projection type 只包含 `Fields...`。

## 編譯期驗證

`select_result_t<Model, Fields...>` 應做以下驗證：

- `Model` 必須代表完整 class type。
- 每個 `Fields` 必須代表 `Model` 的 non-static data member。
- 不允許選到 `[[=cpporm::ignore{}]]` 的 field。
- 不允許重複欄位。
- 欄位順序依 `Fields...` 順序，不強制依 model 宣告順序。

範例：

```cpp
constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.name,
    cpporm::fields<user>.id
);
using row = typename decltype(query)::row_type;
```

`row` 欄位順序應是：

```cpp
struct row {
    std::string name;
    std::int64_t id;
};
```

## 未選欄位必須不存在

這是 select 功能的核心要求。

如果：

```cpp
constexpr auto query = cpporm::sqlite::select<user>(
    cpporm::fields<user>.id,
    cpporm::fields<user>.name
);
using row = typename decltype(query)::row_type;
```

則 `row` 不得有 `email` 欄位。

錯誤方向：

```cpp
struct row {
    std::int64_t id;
    std::string name;
    std::optional<std::string> email; // 不允許
};
```

正確方向：

```cpp
struct row {
    std::int64_t id;
    std::string name;
};
```

## 實測狀態

已用 GCC 16 snapshot 確認：

- `std::meta::define_aggregate` 可以合成 aggregate struct。
- `std::meta::data_member_spec` 可以使用原 model field 的 type 與 identifier 產生成員。
- 合成後的 projection struct 可以只包含 selected fields。
- `std::meta::info` 可以作為 template non-type parameter。
- `select_result<Model, Fields...>` 這種 class template 可以在內部用 `define_aggregate` 產生唯一 projection type。

待實測：

- duplicate field detection 的最佳實作方式。
- field 是否屬於指定 model 的穩定驗證方式。
