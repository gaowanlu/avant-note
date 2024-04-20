#pragma once
#include <atomic>
#include "event/event_poller.h"
#include "socket/socket_pair.h"
#include "connection/connection_mgr.h"
#include <memory>

namespace avant::worker
{
    class worker
    {
    public:
        worker();
        ~worker();
        void operator()();

        bool to_stop{false};
        bool is_stoped{false};
        int worker_id{-1};
        size_t max_client_num{0};
        bool use_ssl{false};

        std::shared_ptr<std::atomic<int>> curr_connection_num{nullptr};

        avant::socket::socket_pair *main_worker_tunnel{nullptr};

        avant::event::event_poller epoller;
        std::shared_ptr<avant::connection::connection_mgr> worker_connection_mgr{nullptr};

    private:
        void on_tunnel_event(uint32_t event);
        void on_client_event(int fd, uint32_t event);
    };
};
