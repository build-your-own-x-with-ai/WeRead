# WeRead LVGL - 微信读书 C 语言桌面客户端

基于 LVGL 9.2 + SDL2 的微信读书桌面客户端，使用 C 语言从 Python 版本 [JIX](https://github.com/) 移植而来。

窗口大小 800×600，支持 QR 码登录、书架浏览、分类发现、书籍详情、章节阅读等完整功能。

## 功能概览

- **QR 码登录**：扫码认证，skey/vid session 管理，自动续期
- **书架**：按阅读时间排序，显示进度百分比和阅读状态
- **发现**：分类浏览（飙升·出版等）+ 推荐书籍
- **书籍详情**：封面、作者、简介、章节列表、阅读进度
- **阅读器**：4 片段解密、HTML 解析、CJK 排版、图文混排、章节导航
- **老板键**：一键切换为伪终端界面（`top` 风格）
- **全屏模式**：F 键切换

## 构建

### 依赖（macOS）

```bash
brew install sdl2 curl openssl pkg-config cmake
```

### 编译

```bash
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

> 注意：macOS Homebrew 环境下，CMakeLists.txt 需要 `target_link_directories` 包含 `${SDL2_LIBRARY_DIRS}` 和 `${CURL_LIBRARY_DIRS}`，否则链接器报 "library not found"。

### 运行

```bash
./bin/weread_lvgl
```

首次运行会显示 QR 码，用微信扫码登录。登录凭据保存在 `~/.weread-cli/cookie.json`，与 JIX (Python) 共享。

## 键盘操作

| 按键 | 功能 |
|------|------|
| `<` / `>` | 上一章 / 下一章 |
| `Space` | 向下翻页（85% 视口高度） |
| `b` / `B` | 向上翻页 |
| `↑` / `↓` | 滚动（150px） |
| `g` | 跳到章节顶部 |
| `G` | 跳到章节底部 |
| `f` / `F` | 全屏切换 |
| `v` / `V` | 老板键切换 |
| `q` / `Q` | 退出阅读器 |

## 项目结构

```
WeReadLVGL/
├── CMakeLists.txt              # CMake 构建配置
├── lv_conf.h                   # LVGL 配置（FreeType、TJPGD、FS_POSIX 等）
├── patches/                    # 对 LVGL 库的补丁（lib/ 被 gitignore）
├── lib/                        # 第三方库（.gitignore）
│   ├── lvgl/                   # LVGL 9.2.0
│   ├── cJSON/                  # JSON 解析
│   └── qrcodegen/              # 二维码生成
├── src/
│   ├── main.c                  # 入口：SDL2 窗口、FreeType 字体、主循环
│   ├── api/
│   │   └── api_client.c/h      # HTTP 客户端、签名、4 片段解密、图片下载
│   ├── auth/
│   │   └── auth_controller.c/h # QR 登录状态机、getinfo 长轮询、cookie 管理
│   ├── ui/
│   │   ├── screen_manager.c/h  # 栈式屏幕管理（push/pop/replace）
│   │   ├── screen_login.c/h    # QR 码登录界面
│   │   ├── screen_shelf.c/h    # 书架（排序、进度条、发现/登出按钮）
│   │   ├── screen_discover.c/h # 发现（分类列表 + 推荐书籍双视图）
│   │   ├── screen_book_detail.c/h # 书籍详情（信息区 + 章节列表）
│   │   └── screen_reader.c/h   # 阅读器（图文混排、翻页、老板键）
│   └── utils/
│       ├── crypto.c/h          # decrypt_wr、chk、calc_hash、sign_payload
│       ├── html_parser.c/h     # HTML → content_block_t 解析器
│       └── content_renderer.c/h # content_block_t → 可显示文本（CJK 宽度）
└── build/
    └── bin/weread_lvgl         # 可执行文件
```

## 关键技术点

### 加密算法（crypto.c）

从 JIX Python 1:1 移植，所有输出与 Python 参考实现逐字节验证一致：

- **decrypt_wr**：去首字符 → 尾部二进制位置对 → 反向交换 → Base64 解码
- **chk**：MD5 自校验（前 32 字符 = header，其余 = body，header == md5(body)）
- **calc_hash**：自动检测 digit/non-digit kind，9 位 hex 分块 + 长度前缀 + MD5 校验
- **sign_payload**：URL 编码排序键值对，双 32 位 XOR 累加器反向迭代

### 章节内容解密

EPUB 格式分 4 片段传输（e_0, e_1, e_2, e_3），其中 e_2 为样式，其余为内容。HTML = decrypt(e_0 + e_1 + e_3)。TXT 格式 2 片段（t_0, t_1），HTML = decrypt(t_0 + t_1)。

### 阅读器渲染

- HTML 解析为 content_block_t（P/H1-H4/PRE/IMAGE/BLOCKQUOTE/HR）
- 分块渲染（TEXT_CHUNK_LINES=30）避免 LVGL LONG_WRAP 对 pre/code 的 5-10× 膨胀导致 OOM
- 图文混排：遍历 blocks，遇到 BLOCK_IMAGE 时 flush 文本段、下载图片、创建 lv_image
- LVGL 文件系统：`LV_FS_POSIX_LETTER='A'`，图片路径格式 `A:tmp/weread_img_*.jpg`

### LVGL 9.2 注意事项

- 每个控件必须显式设置 `bg_opa(LV_OPA_COVER)` + `border_width(0)` + `pad_all(0)`，默认主题会添加可见边框
- Flex 滚动容器不绘制子元素下方的空白区域 → 用不可滚动的 bg_panel 覆盖
- `lv_obj_scroll_by(obj, x, y)` 方向反直觉：+y 内容下移（显示上方），-y 内容上移（显示下方）
- `lv_obj_get_height()` 返回滚动内容全高，非视口 → 用 `lv_disp_get_ver_res(NULL)` 获取视口
- `LV_USE_DRAW_SDL=1` 无 SDL_RenderClear → 在 `window_update` 后清空 back buffer（见 `patches/`）

### SDL 渲染补丁

`lib/` 目录被 gitignore，对 LVGL SDL 驱动的修改保存在 `patches/sdl_render_clear_fix.patch`。

修改内容：
- `window_create`：创建 renderer 后用背景色 clear + present
- `window_update`：`SDL_RenderPresent` 后用背景色 clear back buffer

编译后需手动应用此补丁到 `lib/lvgl/src/drivers/sdl/lv_sdl_window.c`。

## Cookie 共享

WeReadLVGL 与 JIX (Python) 共享 `~/.weread-cli/cookie.json`，格式：
```json
{"cookie": "wr_vid=xxx; wr_skey=yyy"}
```

任一端登录后，另一端可直接使用。

## 参考实现

- **JIX (Python)**：`../JIX/` — 完整的 Python Textual TUI 实现
  - `weread_cli/client.py` — API 和加密细节
  - `weread_cli/auth.py` — 认证流程
  - `screens/` — UI 逻辑参考

## 许可证

待定
