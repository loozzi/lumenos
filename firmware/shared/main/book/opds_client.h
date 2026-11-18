/*
 * SPDX-FileCopyrightText: 2026 dpReader
 * SPDX-License-Identifier: Apache-2.0
 *
 * Module duyệt và tải sách trực tiếp qua giao thức OPDS (Calibre-Web / OPDS 1.2 Atom XML).
 * Mặc định kết nối tới https://sach.vreader.cloud:8443/opds (cho phép tùy chỉnh).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPDS_MAX_ENTRIES 24

typedef struct {
    char title[128];        ///< Tiêu đề mục hoặc tên sách (UTF-8)
    char author[64];        ///< Tác giả (nếu có)
    char summary[160];      ///< Tóm tắt ngắn
    char href[256];         ///< Đường dẫn duyệt tiếp hoặc tải sách (đã chuẩn hóa URL đầy đủ)
    char ext[8];            ///< Định dạng sách: "epub", "txt" hoặc "" nếu là thư mục
    bool is_book;           ///< true: mục sách tải được; false: danh mục con (catalog)
} opds_entry_t;

typedef struct {
    char feed_title[96];                    ///< Tiêu đề trang OPDS hiện tại
    char next_url[256];                     ///< Đường dẫn trang kế tiếp (rel="next") nếu có
    size_t count;                           ///< Số lượng mục trong entries
    opds_entry_t entries[OPDS_MAX_ENTRIES]; ///< Danh sách các mục
} opds_feed_t;

/// UI-owned browsing session: retain self-started WiFi between requests; close only after synchronous work returns.
void opds_session_begin(void);
void opds_session_end(void);
bool opds_session_active(void);

typedef void (*opds_progress_cb_t)(size_t received_bytes, size_t total_bytes, void *user_ctx);

/* UI-thread-only observer; background feed fetches forward it through opds_fetch_async_poll.
 * Must not start/stop network. Clear before the owning page/context exits. */
void opds_set_network_observer(void (*observer)(void *),void *context);

typedef enum {
    OPDS_ASYNC_IDLE = 0,    ///< 没有进行中的目录请求 / No feed request in flight
    OPDS_ASYNC_LOADING,     ///< 后台任务仍在加载 / Worker still loading
    OPDS_ASYNC_DONE,        ///< 最新请求已完成，结果已写出 / Latest request finished; result copied out
} opds_async_state_t;

/// 在后台任务加载目录；新请求取代旧请求（旧结果丢弃）。UI线程调用。
/// Fetch a feed in a background worker; a newer request supersedes older ones (stale results are dropped). UI thread only.
bool opds_fetch_feed_async(const char *url);
/// UI线程轮询：转发网络状态观察者，完成时复制结果与提示并返回 DONE。
/// UI-thread poll: forwards the network observer and, when finished, copies the feed and message and returns DONE.
opds_async_state_t opds_fetch_async_poll(opds_feed_t *out_feed, esp_err_t *out_err,
                                         char *message, size_t message_cap);
/// 放弃未取的结果与排队请求；进行中的任务完成后其结果被丢弃。/ Drop queued/unclaimed results; an in-flight result is discarded when it lands.
void opds_fetch_async_cancel(void);
/// 后台任务仍持有网络时为 true。/ True while the worker is still running.
bool opds_fetch_async_busy(void);

/// Tải và phân tích danh mục OPDS tại `url` (nếu `url == NULL` hoặc rỗng sẽ dùng URL gốc trong Cài đặt).
esp_err_t opds_fetch_feed(const char *url, opds_feed_t *out_feed, char *message, size_t message_cap);

/// Tải trực tiếp một cuốn sách từ `entry` về thư mục sách (`/sdcard/books` hoặc `/flash/books`).
esp_err_t opds_download_book(const opds_entry_t *entry, char *saved_path, size_t saved_path_cap,
                             opds_progress_cb_t progress_cb, void *user_ctx,
                             char *message, size_t message_cap);

#ifdef __cplusplus
}
#endif
