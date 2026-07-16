# WeRead LVGL - 微信读书 C 语言 LVGL 移植版

基于 LVGL 9.2 的微信读书桌面客户端，使用 C 语言从 Python 版本 JIX 移植而来。

## 项目状态

### ✅ 已完成

1. **基础项目结构**
   - 目录结构：`src/`, `lib/`, `build/`
   - Git 仓库初始化

2. **依赖库集成**
   - LVGL 9.2.0（从 WeRead/components 复制）
   - cJSON（JSON 解析）
   - qrcodegen（二维码生成）
   - SDL2（显示驱动）
   - libcurl（HTTP 客户端）
   - OpenSSL（加密支持）

3. **构建系统**
   - CMakeLists.txt 配置完成
   - 所有依赖正确链接
   - 成功编译生成可执行文件

4. **基础 UI 测试**
   - SDL2 窗口 800x600 成功启动
   - LVGL 基础渲染正常
   - 测试标签组件显示

### 🚧 待完成

5. **中文字体支持**（任务 #9）
   - 需要添加中文字体文件（Noto Sans CJK 或类似）
   - 在 lv_conf.h 中配置字体
   - 当前中文字符显示为空白

6. **API 客户端模块**（任务 #6）
   - 实现 HTTP 请求封装（基于 libcurl）
   - 实现加密算法：
     - `calc_hash()` - 自定义哈希用于 API 签名
     - `sign_payload()` - XOR 请求签名
     - `decrypt_wr()` - Base64 + 字符位置交换解密
   - 实现 JSON 响应解析（基于 cJSON）
   - Cookie 管理和持久化

7. **认证流程**（任务 #7）
   - 6 步 QR 码登录：
     1. `POST /web/login/getuid` 获取 uid
     2. 生成二维码 URL
     3. `POST /web/login/getinfo` 轮询（获取 skey, vid）
     4. `POST /web/login/weblogin` 轮询（获取 accessToken）
     5. `POST /web/login/session/init` 初始化 session
     6. `POST /web/login/renewal` 刷新 cookies
   - 使用 qrcodegen 生成二维码
   - 在 LVGL 中渲染二维码

8. **UI 屏幕架构**（任务 #8）
   - WelcomeScreen - 欢迎界面
   - LoginScreen - 登录界面（显示二维码）
   - ShelfScreen - 书架主界面
   - DiscoverScreen - 发现页面
   - BookDetailScreen - 书籍详情
   - ReaderScreen - 阅读器
   - BossScreen - 老板键遮罩

9. **核心功能**
   - 书架同步（`/web/shelf/sync`）
   - 书籍信息获取
   - 章节列表加载
   - 章节内容解密和渲染
   - 阅读进度同步
   - HTML 到纯文本转换
   - 本地缓存系统

## 构建说明

### 依赖

macOS:
```bash
brew install sdl2 curl openssl pkg-config cmake
```

### 编译

```bash
cd /Users/i/Code/Build_Your_Onw_X_With_AI/WeRead/WeReadLVGL
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 运行

```bash
./bin/weread_lvgl
```

## 项目结构

```
WeReadLVGL/
├── CMakeLists.txt          # CMake 构建配置
├── lv_conf.h              # LVGL 配置文件
├── lib/                   # 第三方库
│   ├── lvgl/             # LVGL 9.2.0
│   ├── cJSON/            # JSON 解析库
│   └── qrcodegen/        # 二维码生成库
├── src/
│   ├── main.c            # 程序入口（已完成基础窗口）
│   ├── api/              # API 客户端（待实现）
│   ├── ui/               # UI 组件和屏幕（待实现）
│   │   ├── screens/      # 各个屏幕
│   │   └── widgets/      # 自定义组件
│   └── utils/            # 工具函数（待实现）
│       ├── crypto.c      # 加密算法
│       ├── cache.c       # 缓存管理
│       └── html_parser.c # HTML 解析器
└── build/                # 构建输出
    └── bin/
        └── weread_lvgl   # 可执行文件
```

## API 参考

基于 JIX Python 实现分析的关键 API 端点：

### 认证
- `POST /web/login/getuid` - 获取登录 UID
- `POST /web/login/getinfo` - 轮询获取 skey/vid
- `POST /web/login/weblogin` - 轮询获取 accessToken
- `POST /web/login/session/init` - 初始化 session
- `POST /web/login/renewal` - 刷新 token

### 书架
- `GET /web/shelf/sync?onlyBookid=1&cbcount=1` - 同步书架
- `POST /web/shelf/syncBook` - 获取详细书籍信息

### 书籍
- `GET /web/book/info?bookId={id}` - 书籍详情
- `POST /web/book/chapterInfos` - 章节列表
- `POST /web/book/read` - 阅读进度初始化/更新

### 章节内容（加密）
- `POST /web/book/chapter/e_0` - EPUB/PDF 内容片段 0
- `POST /web/book/chapter/e_1` - EPUB/PDF 内容片段 1
- `POST /web/book/chapter/e_2` - EPUB/PDF 样式
- `POST /web/book/chapter/e_3` - EPUB/PDF 内容片段 3
- `POST /web/book/chapter/t_0` - TXT 内容片段 0
- `POST /web/book/chapter/t_1` - TXT 内容片段 1

### 发现
- `GET /web/categories?synckey=0` - 分类列表
- `GET /web/recommend_books` - 推荐书籍

## 加密算法

需要在 C 中实现的关键算法（参考 `weread_cli/client.py`）：

### calc_hash(value)
- MD5 前 3 字符 + 类型字节 + 后缀 + hex 分块
- 用于 bookId/chapterUid 的 API 签名哈希

### sign_payload(dict)
- 按字母顺序排序键
- URL 编码值
- XOR 操作（魔数 0x15051505）
- 返回十六进制签名

### decrypt_wr(encrypted_string)
- 去除首字符
- 基于尾部的位提取生成位置列表
- 字符位置交换
- Base64 解码

## 数据模型

```c
typedef struct {
    char book_id[64];
    char title[256];
    char author[128];
    char cover_url[512];
    char format[8];        // "epub", "pdf", "txt"
    int progress;          // 0-100
    int chapter_offset;
    int chapter_uid;
    int update_time;
    bool is_on_shelf;
} Book;

typedef struct {
    int chapter_uid;
    char title[256];
    int level;
    int word_count;
    int pay_status;
    char anchor[128];
} Chapter;

typedef struct {
    char *html;
    char *style;
    char format[8];
} ChapterContent;
```

## 参考实现

- **JIX (Python)**: `/Users/i/Code/Build_Your_Onw_X_With_AI/WeRead/JIX/`
  - 完整的 Python 终端客户端实现
  - 参考 `weread_cli/client.py` 了解 API 细节
  - 参考 `weread_cli/auth.py` 了解认证流程
  - 参考各 screen 文件了解 UI 逻辑

## 下一步

1. **中文字体** - 添加 Noto Sans CJK 或系统字体
2. **API 客户端** - 实现 HTTP + 加密 + JSON 处理
3. **登录屏幕** - 实现二维码显示和轮询
4. **书架屏幕** - 实现书籍列表显示
5. **阅读器** - 实现章节内容解密和渲染

## 开发笔记

- 使用 SDL2 作为 PC 端显示驱动
- LVGL 配置为 32 位色深
- 禁用了 FreeType（会在后续添加中文字体时启用）
- OpenSSL 用于 MD5/SHA256 等加密操作
- 所有 API 请求需要正确的签名（sign_payload）
- 章节内容是多片段加密传输，需要组合后解密

## 许可证

待定
