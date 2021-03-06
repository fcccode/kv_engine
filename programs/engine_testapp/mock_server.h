#pragma once

#include <memcached/engine_testapp.h>

void mock_init_alloc_hooks();

SERVER_HANDLE_V1* get_mock_server_api();

void init_mock_server();

void mock_time_travel(int by);

void mock_set_pre_link_function(PreLinkFunction function);
