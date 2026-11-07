/*
 * TML Extension API v1 — C 语言宿主 SDK（运行时：native / c）
 *
 * TYW Launcher 的扩展永远运行在启动器进程之外，仅通过 stdin/stdout 的
 * NDJSON（每行一个 JSON）交换白名单方法。本文件提供最小实现：
 *   · 自动完成 initialize 握手；
 *   · 封装 log / toast / host.info / instances.list / event.subscribe /
 *     storage.get / storage.set / settings.get / settings.set；
 *   · 接收宿主广播的事件、优雅退出。
 *
 * 零依赖：仅需 C 标准库（gcc / clang / MSVC 均可编译）。JSON 解析为针对
 * 本协议约定的极简实现，不引入任何第三方库。
 *
 * 可调用的能力由 manifest.json 的 permissions 决定，未申请的永远不会
 * 返回数据。详见启动器「设置 → 其他 → 拓展」或 https://mtr.tywmc.cn
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TML_MAX_LINE 65536

static int g_ready = 0;
static int g_stop = 0;
static long g_next_id = 2;

static long g_pending_id = 0;
static int g_pending_done = 0;
static char g_pending_result[TML_MAX_LINE];

/* ---------- 极简 JSON 取值（仅适用于本协议约定的简单消息） ---------- */

/* 取顶层字符串字段值，返回 malloc 字符串，失败返回 NULL */
static char* tml_jstr(const char* json, const char* key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && (*p == ':' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* start = p;
    while (*p && *p != '"') p++;
    size_t len = (size_t)(p - start);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = 0;
    return out;
}

/* 取顶层整数（用于 id / apiVersion） */
static long tml_jint(const char* json, const char* key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return -1;
    p += strlen(pat);
    while (*p && (*p == ':' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return strtol(p, NULL, 10);
}

/* 在 "params":{...} 内取字符串字段 */
static char* tml_jstr_params(const char* json, const char* key) {
    const char* p = strstr(json, "\"params\"");
    if (!p) return NULL;
    p = strchr(p, '{');
    if (!p) return NULL;
    return tml_jstr(p, key);
}

/* ---------- 收发 ---------- */

static void tml_send(const char* fmt, ...) {
    char buf[TML_MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stdout, "%s\n", buf);
    fflush(stdout);
}

/* 从 stdin 读取一行（去掉换行），返回 malloc 字符串，EOF 返回 NULL */
static char* tml_read_line(void) {
    size_t cap = 256, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = getchar()) != EOF) {
        if (c == '\n') break;
        if (c == '\r') continue;
        if (len + 1 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = 0;
    return buf;
}

/* ---------- 协议分发 ---------- */

static int tml_handle_line(const char* line);

/* 同步请求：发送方法/参数，阻塞读取直到拿到对应 id 的结果。
 * 返回结果 JSON（静态缓冲，下次调用即失效）；出错返回 NULL。 */
static const char* tml_call(const char* method, const char* params_json) {
    long id = g_next_id++;
    g_pending_id = id;
    g_pending_done = 0;
    g_pending_result[0] = 0;
    tml_send("{\"id\":%ld,\"method\":\"%s\",\"params\":%s}", id, method, params_json ? params_json : "{}");
    while (!g_pending_done && !g_stop) {
        char* line = tml_read_line();
        if (!line) { g_stop = 1; break; }
        tml_handle_line(line);
        free(line);
    }
    if (g_stop) return NULL;
    return g_pending_result;
}

static int tml_handle_line(const char* line) {
    /* 响应（宿主对扩展请求的回复） */
    if (strstr(line, "\"result\"") || strstr(line, "\"error\"")) {
        long id = tml_jint(line, "id");
        if (id == g_pending_id) {
            const char* err = strstr(line, "\"error\"");
            if (err) {
                char* msg = tml_jstr(line, "message");
                snprintf(g_pending_result, sizeof(g_pending_result), "{\"error\":\"%s\"}", msg ? msg : "host error");
                if (msg) free(msg);
            } else {
                const char* r = strstr(line, "\"result\"");
                const char* start = strchr(r, '{');
                if (start) {
                    const char* end = start + strlen(start) - 1;
                    while (end > start && *end != '}') end--;
                    size_t n = (size_t)(end - start + 1);
                    if (n < sizeof(g_pending_result)) {
                        memcpy(g_pending_result, start, n);
                        g_pending_result[n] = 0;
                    }
                }
            }
            g_pending_done = 1;
        }
        return g_stop ? 0 : 1;
    }
    /* 方法消息（宿主 → 扩展） */
    char* method = tml_jstr(line, "method");
    if (!method) return 1;
    long id = tml_jint(line, "\"id\"");
    if (strcmp(method, "initialize") == 0) {
        tml_send("{\"id\":%ld,\"result\":{\"ready\":true}}", id);
        g_ready = 1;
    } else if (strcmp(method, "shutdown") == 0) {
        g_stop = 1;
    } else if (strcmp(method, "event") == 0) {
        char* etype = tml_jstr_params(line, "type");
        if (etype) {
            fprintf(stderr, "[event] %s\n", etype);
            free(etype);
        }
    } else {
        /* 宿主下发的自定义命令：模板直接确认 */
        tml_send("{\"id\":%ld,\"result\":{}}", id);
    }
    free(method);
    return g_stop ? 0 : 1;
}

/* ---------- 便捷方法 ---------- */

static void tml_log(const char* level, const char* msg) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"level\":\"%s\",\"message\":\"%s\"}", level ? level : "info", msg ? msg : "");
    tml_call("log", p);
}

static void tml_toast(const char* msg) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"message\":\"%s\"}", msg ? msg : "");
    tml_call("toast", p);
}

static const char* tml_host_info(void) {
    return tml_call("host.info", "{}");
}

static const char* tml_list_instances(void) {
    return tml_call("instances.list", "{}");
}

static const char* tml_subscribe(const char* types_json) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"types\":%s}", types_json ? types_json : "[]");
    return tml_call("event.subscribe", p);
}

static void tml_storage_set(const char* key, const char* value) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"key\":\"%s\",\"value\":\"%s\"}", key ? key : "", value ? value : "");
    tml_call("storage.set", p);
}

static const char* tml_storage_get(const char* key) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"key\":\"%s\"}", key ? key : "");
    return tml_call("storage.get", p);
}

static void tml_settings_set(const char* key, const char* value) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"key\":\"%s\",\"value\":\"%s\"}", key ? key : "", value ? value : "");
    tml_call("settings.set", p);
}

static const char* tml_settings_get(const char* key) {
    char p[4096];
    snprintf(p, sizeof(p), "{\"key\":\"%s\"}", key ? key : "");
    return tml_call("settings.get", p);
}

/* ---------- 入口 ---------- */

int main(void) {
    /* 读取宿主下发的协议配置（环境变量，由宿主进程设置） */
    const char* ext_id = getenv("TML_EXT_ID");
    const char* api = getenv("TML_EXT_API");
    fprintf(stderr, "[C 扩展] id=%s api=%s\n", ext_id ? ext_id : "?", api ? api : "?");

    /* 等待握手 initialize */
    while (!g_ready && !g_stop) {
        char* line = tml_read_line();
        if (!line) break;
        tml_handle_line(line);
        free(line);
    }
    if (g_stop) return 0;

    /* ===== 下面是扩展的业务逻辑示例 ===== */
    tml_log("info", "C 扩展已启动（TML Extension API v1）");

    const char* info = tml_host_info();
    if (info) fprintf(stderr, "[host.info] %s\n", info);

    const char* instances = tml_list_instances();
    if (instances) fprintf(stderr, "[instances] %s\n", instances);

    tml_subscribe("[\"launcher.started\",\"game.launched\",\"settings.changed\"]");

    tml_storage_set("demo", "hello");
    const char* v = tml_storage_get("demo");
    if (v) fprintf(stderr, "[storage.get demo] %s\n", v);

    tml_settings_set("foo", "bar");
    const char* s = tml_settings_get("foo");
    if (s) fprintf(stderr, "[settings.get foo] %s\n", s);

    tml_toast("C 扩展已就绪");

    /* 保持运行，处理宿主广播的事件与命令，直到收到 shutdown */
    while (!g_stop) {
        char* line = tml_read_line();
        if (!line) break;
        tml_handle_line(line);
        free(line);
    }
    fprintf(stderr, "[C 扩展] 已退出\n");
    return 0;
}
