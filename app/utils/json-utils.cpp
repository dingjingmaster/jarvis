//
// Created by dingjing on 10/19/22.
//

#include "json-utils.h"

#include "../core/c-log.h"
#include <json-glib/json-glib.h>

std::string JsonUtils::jsonBuildIndexPrice(GoldDataClient& data)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "price");
    json_builder_add_double_value (builder, data.price);

    json_builder_set_member_name (builder, "uTime");
    json_builder_add_int_value (builder, data.dateTime);

    json_builder_set_member_name (builder, "d3");
    json_builder_add_double_value (builder, data.priceAvg3);

    json_builder_set_member_name (builder, "d7");
    json_builder_add_double_value (builder, data.priceAvg7);

    json_builder_set_member_name (builder, "d30");
    json_builder_add_double_value (builder, data.priceAvg30);

    json_builder_end_object (builder);

    g_autoptr(JsonNode) root = json_builder_get_root (builder);

    g_autoptr(JsonGenerator) gen = json_generator_new ();

    json_generator_set_root (gen, root);

    g_autofree char *str = json_generator_to_data (gen, NULL);

    logd(str);

    return std::move(std::string(str));
}
