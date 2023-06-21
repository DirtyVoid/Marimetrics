#include "globe.h"

#define MAX_CONFIG_JSON_LEN 1024

static const char *dir_path = "/";

static bool get_bool(const cJSON *json, const char *key, bool default_value) {
    cJSON *json_val = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsBool(json_val) ? cJSON_IsTrue(json_val) : default_value;
}

static int64_t get_int(const cJSON *json, const char *key, int default_value) {
    cJSON *json_val = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsNumber(json_val) ? cJSON_GetNumberValue(json_val) : default_value;
}

static float get_float(const cJSON *json, const char *key, float default_value) {
    cJSON *json_val = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsNumber(json_val) ? cJSON_GetNumberValue(json_val) : default_value;
}

static void get_string(const cJSON *json, const char *key, const char *default_value,
                       char *out, size_t out_capacity) {
    assert(strlen(default_value) < out_capacity);

    cJSON *json_val = cJSON_GetObjectItemCaseSensitive(json, key);
    if (cJSON_IsString(json_val)) {
        const char *str = cJSON_GetStringValue(json_val);
        size_t len = strlen(str);
        if (len < out_capacity) {
            memcpy(out, str, len);
            out[len] = '\0';
            return;
        }
    }
    strcpy(out, default_value);
}

static void validate_config(const struct app_config *conf) {
    assert(conf->log_period_seconds > 0 || conf->log_period_seconds == -1);
}

static bool config_eq(const struct app_config *conf0, const struct app_config *conf1) {
#define CONFIG_EQ_config_string(name)                                                  \
    if (strncmp(conf0->name, conf1->name, array_size(conf0->name)))                    \
        return false;
#define CONFIG_EQ_config_bool(name)                                                    \
    if (conf0->name != conf1->name)                                                    \
        return false;
#define CONFIG_EQ_int(name)                                                            \
    if (conf0->name != conf1->name)                                                    \
        return false;
#define CONFIG_EQ_float(name)                                                          \
    if (conf0->name != conf1->name)                                                    \
        return false;
#define CONFIG_EQ_epoch_t(name) CONFIG_EQ_int(name)
#define CONFIG_EQ_2(type, name) CONFIG_EQ_##type(name)
#define CONFIG_EQ(type, name, string, default) CONFIG_EQ_2(type, name)
    CONFIG_EXPANSION(CONFIG_EQ);
    return true;
}

static FILE *open_config_file(const char *flags) {
    const char *basename = "config.json";

    size_t dir_len = strlen(dir_path);
    assert(dir_len > 0 && dir_path[dir_len - 1] == '/');
    size_t base_len = strlen(basename);

    char *path = malloc(dir_len + base_len + 1);
    strcpy(path, dir_path);
    strcpy(path + dir_len, basename);
    FILE *file = fopen(path, flags);
    free(path);
    return file;
}

cJSON *load_app_config_json() {

    FILE *file = open_config_file("r");

    const char *content_buf = "";

    char *allocated_buf = NULL;
    if (file) {
        /* allocate space for text and null character */
        allocated_buf = malloc(MAX_CONFIG_JSON_LEN + 1);
        size_t n_read = fread(allocated_buf, 1, MAX_CONFIG_JSON_LEN, file);
        assert(!ferror(file)); /* no file error */
        assert(feof(file));    /* whole file read */
        fclose(file);
        if (n_read < MAX_CONFIG_JSON_LEN) {
            allocated_buf[n_read] = '\0';
            content_buf = allocated_buf;
        }
    }

    cJSON *json = cJSON_Parse(content_buf);

    if (allocated_buf) {
        free(allocated_buf);
    }

    return json;
}

struct app_config app_config_from_json(cJSON *json) {
    struct app_config conf = {};

#define CONFIG_LOAD_JSON_config_string(name, string, default)                          \
    get_string(json, string, default, conf.name, array_size(conf.name));
#define CONFIG_LOAD_JSON_config_bool(name, string, default)                            \
    conf.name = get_bool(json, string, default);
#define CONFIG_LOAD_JSON_int(name, string, default)                                    \
    conf.name = get_int(json, string, default);
#define CONFIG_LOAD_JSON_float(name, string, default)                                  \
    conf.name = get_float(json, string, default);
#define CONFIG_LOAD_JSON_epoch_t(name, string, default)                                \
    CONFIG_LOAD_JSON_int(name, string, default)
#define CONFIG_LOAD_JSON_2(type, name, string, default)                                \
    CONFIG_LOAD_JSON_##type(name, string, default)
#define CONFIG_LOAD_JSON(type, name, string, default)                                  \
    CONFIG_LOAD_JSON_2(type, name, string, default)
    CONFIG_EXPANSION(CONFIG_LOAD_JSON);

    return conf;
}

struct app_config load_app_config() {
    cJSON *json = load_app_config_json();
    struct app_config conf = app_config_from_json(json);
    cJSON_Delete(json);
    validate_config(&conf);
    return conf;
}

void save_app_config_json(cJSON *json) {
    char *serialized_config = cJSON_Print(json);

    FILE *file = open_config_file("w");
    assert(file);

    size_t nb = strlen(serialized_config);
    size_t nwritten = fwrite(serialized_config, 1, nb, file);
    assert(nwritten == nb);

    free(serialized_config);

    fclose(file);
}

cJSON *app_config_to_json(const struct app_config *config) {
    validate_config(config);

    cJSON *json = cJSON_CreateObject();

#define CONFIG_STORE_JSON_config_string(name, string)                                  \
    cJSON_AddItemToObject(json, string, cJSON_CreateStringReference(config->name));
#define CONFIG_STORE_JSON_config_bool(name, string)                                    \
    cJSON_AddItemToObject(json, string, cJSON_CreateBool(config->name));
#define CONFIG_STORE_JSON_int(name, string)                                            \
    cJSON_AddItemToObject(json, string, cJSON_CreateNumber(config->name));
#define CONFIG_STORE_JSON_float(name, string)                                          \
    cJSON_AddItemToObject(json, string, cJSON_CreateNumber(config->name));
#define CONFIG_STORE_JSON_epoch_t(name, string) CONFIG_STORE_JSON_int(name, string)
#define CONFIG_STORE_JSON_2(type, name, string) CONFIG_STORE_JSON_##type(name, string)
#define CONFIG_STORE_JSON(type, name, string, default)                                 \
    CONFIG_STORE_JSON_2(type, name, string)
    CONFIG_EXPANSION(CONFIG_STORE_JSON);

    return json;
}

void save_app_config(const struct app_config *config) {
    validate_config(config);

    cJSON *json = app_config_to_json(config);

    save_app_config_json(json);

    cJSON_Delete(json);

#ifndef NDEBUG
    const struct app_config loaded_config = load_app_config(dir_path);
    assert(config_eq(config, &loaded_config));
#endif
}
