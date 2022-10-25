#include "esp_camera.h"
#include <Wifi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "esp_http_server.h"
#include "index.h"

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void rotate(int x, int y);
void rotate_man(int degree_x, int degree_y);

httpd_handle_t stream_httpd = NULL;
httpd_handle_t index_httpd = NULL;

static	esp_err_t stream_handler(httpd_req_t* req) {
	camera_fb_t* fb = NULL;
	esp_err_t res = ESP_OK;
	size_t _jpg_buf_len = 0;
	uint8_t* _jpg_buf = NULL;
	char* part_buf[64];
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin","*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
  httpd_resp_set_hdr(req, "Connection", "close");

	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);

	if (res != ESP_OK) {
		return res;
	}

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        }
        else {
            //Serial.printf("Buffer width->  %uB.. \n", (uint32_t) fb->width);

            if (fb->format != PIXFORMAT_JPEG) {
              bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
              esp_camera_fb_return(fb);
              fb = NULL;          
              if (!jpeg_converted) {
                Serial.println("JPEG compression failed");
                res = ESP_FAIL;
              }
            }
            else {
              _jpg_buf_len = fb->len;
              _jpg_buf = fb->buf;
              //httpd_resp_set_hdr(req, "Content-Length", String(fb->len).c_str());
            }
        
            if (res == ESP_OK) {
              size_t hlen = snprintf((char*)part_buf, 64, _STREAM_PART, _jpg_buf_len);
              res = httpd_resp_send_chunk(req, (const char*)part_buf, hlen);
            }
            if (res == ESP_OK) {
              res = httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len);
            }
            if (res == ESP_OK) {
              res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            }
            if (fb) {
                esp_camera_fb_return(fb);
                fb = NULL;
                _jpg_buf = NULL;
            }  
            else if (_jpg_buf) {
              free(_jpg_buf);
              _jpg_buf = NULL;
            }
            if (res != ESP_OK) {
              break;
            }
            //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
        }   
    }

    return res;
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            *obuf = buf;
            return ESP_OK;
        }
        free(buf);
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t locator_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char x[32];
    char y[32];
    char manual[16];
    
    sensor_t *s = esp_camera_sensor_get();

    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_query_key_value(buf, "x", x, sizeof(x)) != ESP_OK ||
        httpd_query_key_value(buf, "y", y, sizeof(y)) != ESP_OK ||
        httpd_query_key_value(buf, "manual", manual, sizeof(manual)) != ESP_OK) {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    free(buf);

    if (atoi(manual) == 1) {
      Serial.printf("Rotating manually\n");
      rotate_man(atoi(x), atoi(y));
    }
    else {
      Serial.printf("X->%d, Y->%d\n", atoi(x), atoi(y));
      rotate(atoi(x), atoi(y));
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);    
}

static	esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin","*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    httpd_resp_set_hdr(req, "Connection", "close");

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        return httpd_resp_send(req, (const char *)INDEX_HTML, sizeof(INDEX_HTML));
    } else {
        ESP_LOGE(TAG, "Camera sensor not found");
        return httpd_resp_send_500(req);
    }
}

void startWebServer() {
    // Set up servo motors
    //rotator.setup(1, 2, 320, 240, 0, 0);
    //rotator.rotate(2, 200);
    int servoIndex1 = 2;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
    };

    httpd_uri_t stream_uri {
      .uri      = "/stream",
      .method   = HTTP_GET,
      .handler  = stream_handler,
      .user_ctx = NULL
    };

    
    httpd_uri_t locator_uri {
      .uri      = "/location",
      .method   = HTTP_GET,
      .handler  = locator_handler,
      .user_ctx = NULL
    };
  
    //Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&index_httpd, &config) == ESP_OK) { // index server (OpenCV server)
      httpd_register_uri_handler(index_httpd, &index_uri);
      httpd_register_uri_handler(index_httpd, &locator_uri);
    }
    
    config.server_port += 1;
    config.ctrl_port += 1;
    if (httpd_start(&stream_httpd, &config) == ESP_OK) // stream server
    {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}


