#include "httpd.h"

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "driveway_httpd"

#define FLASH_GPIO GPIO_NUM_4

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data,
                                size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index)
    {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
    {
        return 0;
    }
    j->len += len;
    return len;
}

esp_err_t jpg_httpd_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;
    int64_t fr_start = esp_timer_get_time();

    gpio_set_level(FLASH_GPIO, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    fb = esp_camera_fb_get(); // double to get current image
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
    if (!fb)
    {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    res = httpd_resp_set_type(req, "image/jpeg");
    if (res == ESP_OK)
    {
        res = httpd_resp_set_hdr(req, "Content-Disposition",
                                 "inline; filename=capture.jpg");
    }

    if (res == ESP_OK)
    {
        if (fb->format == PIXFORMAT_JPEG)
        {
            ESP_LOGI(TAG, "send JPEG directly");
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        }
        else
        {
            ESP_LOGI(TAG, "encode JPEG");
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK
                                                                   : ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
    }
    esp_camera_fb_return(fb);
    gpio_set_level(FLASH_GPIO, 0);

    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "JPG: %uKB %ums", (uint32_t)(fb_len / 1024),
             (uint32_t)((fr_end - fr_start) / 1000));
    return res;
}

static const httpd_uri_t uri_root_handler = {.uri = "/",
                                             .method = HTTP_GET,
                                             .handler = jpg_httpd_handler,
                                             .user_ctx = NULL};

httpd_handle_t driveway_httpd_start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    gpio_pad_select_gpio(FLASH_GPIO);
    gpio_set_direction(FLASH_GPIO, GPIO_MODE_OUTPUT);

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_root_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}