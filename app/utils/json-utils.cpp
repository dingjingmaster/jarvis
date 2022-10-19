//
// Created by dingjing on 10/19/22.
//

#include "json-utils.h"

#include <json-glib/json-glib.h>

std::string JsonUtils::jsonBuildIndexPrice(JsonUtils::PriceType type, GoldData &data)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();

    switch (type) {
        case INDEX_PRICE_TYPE_AU: {
            json_builder_begin_object (builder);
            json_builder_set_member_name (builder, "auPrice");
            json_builder_add_double_value (builder, data.price);

            json_builder_set_member_name (builder, "auUTime");
            json_builder_add_int_value (builder, data.dateTime);

            //json_builder_set_member_name(builder, "auDetail");
            //json_builder_add_null_value(builder);
            break;
        }
        case INDEX_PRICE_TYPE_AG: {
            json_builder_begin_object (builder);
            json_builder_set_member_name (builder, "agPrice");
            json_builder_add_double_value (builder, data.price);

            json_builder_set_member_name (builder, "agUTime");
            json_builder_add_int_value (builder, data.dateTime);

            //json_builder_set_member_name(builder, "agDetail");
            //json_builder_add_null_value(builder);
            break;
        }
    }

    json_builder_end_object (builder);

    g_autoptr(JsonNode) root = json_builder_get_root (builder);

    g_autoptr(JsonGenerator) gen = json_generator_new ();

    json_generator_set_root (gen, root);

    g_autofree char *str = json_generator_to_data (gen, NULL);

    logd(str);

    return std::move(std::string(str));
}
