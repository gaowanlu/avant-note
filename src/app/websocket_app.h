#pragma once

#include "connection/websocket_ctx.h"

namespace avant
{
    namespace server
    {
        class server;
        class worker;
    }
    namespace app
    {
        class websocket_app
        {
        public:
            static void on_main_init(avant::server::server &server_obj);

            static void on_worker_init(avant::worker::worker &worker_obj);

            static void on_main_stop(avant::server::server &server_obj);

            static void on_worker_stop(avant::worker::worker &worker_obj);

            static void on_main_tick(avant::server::server &server_obj);

            static void on_worker_tick(avant::worker::worker &worker_obj);

            static void on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package);

            static void on_new_connection(avant::connection::websocket_ctx &ctx);
            static void on_close_connection(avant::connection::websocket_ctx &ctx);
            static void on_process_connection(avant::connection::websocket_ctx &ctx);
        };
    }
}