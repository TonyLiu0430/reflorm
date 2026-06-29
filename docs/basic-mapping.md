# 基本映射

這份文件描述目前已實作的過渡測試 scaffold。本 API 不代表 cpporm 長期核心方向。

長期方向請以 `docs/core_instruction/basic.md` 與 `docs/syntax.md` 的 annotation-first 設計為準。

本階段仍刻意不支援關聯。

## Model 探測

cpporm 使用 C++26 reflection 探測非靜態資料成員。

```cpp
struct user {
    std::int64_t id;
    std::string name;
    bool dirty;
};

constexpr auto descriptor = cpporm::describe<user>();
```

預設行為：

- table 名稱預設使用反射取得的型別 identifier。
- column 名稱預設使用反射取得的欄位 identifier。
- 欄位順序遵循反射取得的非靜態資料成員順序。

## 顯式映射

```cpp
template<>
inline constexpr auto cpporm::mapping<user> = cpporm::table<user>("users",
    cpporm::field<&user::id>.column("user_id").primary_key(),
    cpporm::field<&user::dirty>.ignore()
);
```

目前支援的 metadata：

- `column(name)` 覆寫資料庫 column 名稱。
- `primary_key()` 將欄位標記為 primary key。
- `ignore()` 將欄位排除在 ORM persistence metadata 之外。

## 目前 Descriptor 結構

```cpp
constexpr auto descriptor = cpporm::describe<user>();

descriptor.name;
descriptor.fields[0].member_name;
descriptor.fields[0].column_name;
descriptor.fields[0].primary_key;
descriptor.fields[0].ignored;
```

descriptor 是目前測試使用的低階 API。之後若要加入更高階的 SQL API，會先寫文件再實作。

## 後續範圍

- database 連線或執行
- private field policy

關聯 metadata、SQL 產生與 query builder 已移到獨立文件：

- `docs/relationships.md`
- `docs/relations.md`
- `docs/query.md`
- `docs/sql-generator.md`
