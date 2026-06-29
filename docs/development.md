# 開發流程

cpporm 採用先文件、後程式碼的開發方式。

## 原則

任何使用者可見的 API、語法與語意，都要先寫進 `docs/`，再實作對應程式碼。

## 流程

1. 先在 `docs/` 新增或更新文件，描述預期行為。
2. 先寫短小的使用範例，再決定內部實作名稱。
3. 如果設計依賴尚不穩定的 C++26 reflection 行為，先把研究記錄放在 `.temp/`。
4. 將穩定結論從 `.temp/` 整理進 `docs/`。
5. 實作滿足文件描述的最小程式碼變更。
6. 加入能覆蓋文件語法的編譯期範例或測試。

## 目前編譯器基準

- 需要 C++26 或更新版本。
- GCC 需要 `-freflection` 才能啟用實驗性 C++26 reflection。
- 本機開發使用 WinLibs GCC 16.0.1 snapshot。
- 編譯器、make 程式與 runtime DLL 目錄都應由 `CMakeUserPresets.json` 指定，不能依賴系統 `PATH` 上的 MinGW。

## 本機 Runtime 設定

這台機器的系統 `PATH` 指向的 MinGW 不是目前需要的 GCC 16 snapshot，因此 user preset 必須顯式指定 runtime DLL 目錄：

```json
{
  "cacheVariables": {
    "CPPORM_RUNTIME_DLL_DIR": "C:/dev/winlibs-x86_64-posix-seh-gcc-16.0.1-snapshot20260222-mingw-w64ucrt-13.0.0-r1/mingw64/bin"
  },
  "environment": {
    "PATH": "C:/dev/winlibs-x86_64-posix-seh-gcc-16.0.1-snapshot20260222-mingw-w64ucrt-13.0.0-r1/mingw64/bin;$penv{PATH}"
  }
}
```

`CPPORM_RUNTIME_DLL_DIR` 會被測試目標用來 prepend 測試執行時的 `PATH`，避免測試 exe 載入到舊版 `libstdc++`、`libgcc_s` 或其他 MinGW runtime DLL。

## Compile-Fail 測試

需要驗證「應該編譯失敗」的 API 時，不要把案例放進正常 doctest binary。做法是：

1. 在 `tests/compile_fail/` 新增獨立 `.cpp`。
2. 讓該檔案只 include `cpporm/cpporm.hpp` 與必要標準庫 header。
3. 用最小 model 觸發預期的 compile-time error。
4. 在 `CMakeLists.txt` 用 `add_cpporm_compile_fail_test(...)` 註冊，並指定預期錯誤訊息。

CTest 會透過 `tests/cmake/expect_compile_failure.cmake` 直接呼叫 compiler。測試只有在「編譯失敗」且「compiler output 包含指定錯誤訊息」時才算通過。這避免錯誤案例因其他語法錯誤失敗卻被誤判為正確。

範例：

```cmake
add_cpporm_compile_fail_test(
    cpporm_compile_fail_reference_type_mismatch
    tests/compile_fail/reference_type_mismatch.cpp
    "cpporm reference field type must match referenced field type"
)
```
