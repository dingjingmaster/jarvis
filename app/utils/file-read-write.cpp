//
// Created by dingjing on 10/18/22.
//
#include "file-read-write.h"

#include <gio/gio.h>
#include <glib-2.0/glib.h>

#include "../core/c-log.h"

std::string FileReadWrite::getFileContent(std::string &path)
{
    g_autoptr(GFile) file = g_file_new_for_path(path.c_str());
    g_autoptr(GError) error = nullptr;

    if (!g_file_query_exists(file, nullptr)) {
        return std::string();
    }

    g_autoptr(GFileInputStream) fr = g_file_read(file, nullptr, &error);
    if (error) {
        loge("%d - %s", error->code, error->message);
        return std::string();
    }

    g_autoptr(GFileInfo) fileInfo = g_file_input_stream_query_info(fr, G_FILE_ATTRIBUTE_STANDARD_SIZE, nullptr, &error);
    if (error) {
        loge("%d - %s", error->code, error->message);
        return std::string();
    }

    guint64 size = g_file_info_get_attribute_uint64(fileInfo, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    if (size <= 0) {
        logw("size is 0");
        return std::string();
    }

    g_autofree char* buf = (char*) g_malloc0(size + 1);

    if (!g_input_stream_read_all(G_INPUT_STREAM(fr), buf, size, nullptr, nullptr, &error)) {
        loge("%d - %s", error->code, error->message);
        return std::string();
    }

    return std::move(std::string(buf, size));
}
