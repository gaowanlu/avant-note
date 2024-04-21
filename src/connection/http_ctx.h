#pragma once

#include "worker/worker.h"
#include "connection/connection.h"
#include <http-parser/http_parser.h>
#include <memory>
#include <string>
#include <map>

namespace avant::connection
{
    class http_ctx
    {
    public:
        http_ctx();
        ~http_ctx();
        void on_create(connection &conn_obj, worker::worker &worker_obj);
        void on_close();
        void on_event(uint32_t event);

        void set_recv_end(bool recv_end);
        bool get_recv_end();
        void set_process_end(bool process_end);
        bool get_process_end();
        void set_response_end(bool response_end);
        bool get_response_end();
        void set_everything_end(bool everything_end);
        bool get_everything_end();

        void add_header(const std::string &key, const std::string &value);

    public:
        static std::shared_ptr<http_parser_settings> settings;
        static void init_http_settings();
        connection *conn_ptr{nullptr};
        worker::worker *worker_ptr{nullptr};

    public:
        std::string url{};
        std::string method{};
        std::map<std::string, std::vector<std::string>> headers{};
        std::string head_field_tmp{};
        std::function<void(http_ctx &ctx)> process_callback{nullptr};
        std::function<void(http_ctx &ctx)> write_end_callback{nullptr};
        std::function<void(http_ctx &ctx)> destory_callback{nullptr};
        void *ptr{nullptr};

    private:
        http_parser http_parser_obj;
        bool recv_end{false};
        bool process_end{false};
        bool response_end{false};
        bool everything_end{false};
    };
}