#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <avant-log/logger.h>
#include <chrono>
#include <vector>
#include "server/server.h"
#include "socket/socket.h"
#include "utility/singleton.h"
#include "task/task_type.h"
#include "socket/server_socket.h"
#include "utility/time.h"

using namespace std;
using namespace avant::server;
using namespace avant::socket;
using namespace avant::utility;
using namespace avant::task;
using namespace avant::worker;
using namespace avant::socket;

server::server()
{
}

server::~server()
{
    // release SSL_CTX
    if (get_use_ssl() && m_ssl_context)
    {
        SSL_CTX_free(m_ssl_context);
    }
    if (m_main_worker_tunnel)
    {
        delete[] m_main_worker_tunnel;
    }
    if (m_workers)
    {
        delete[] m_workers;
    }
}

void server::set_listen_info(const std::string &ip, int port)
{
    m_ip = ip;
    m_port = port;
}

void server::start()
{
    LOG_ERROR("server::start ...");

    // init OpenSSL CTX
    if (get_use_ssl())
    {
        LOG_ERROR("OpenSSL_version %s", OpenSSL_version(OPENSSL_VERSION));
        LOG_ERROR("SSLeay_version %s", SSLeay_version(SSLEAY_VERSION));
        SSL_library_init();
        SSL_load_error_strings();
        m_ssl_context = SSL_CTX_new(SSLv23_server_method());
        if (!m_ssl_context)
        {
            LOG_ERROR("SSL_CTX_new error");
            return;
        }
        SSL_CTX_set_options(m_ssl_context, SSL_OP_SINGLE_DH_USE);

        std::string crt_pem_path = get_crt_pem();
        int i_ret = SSL_CTX_use_certificate_file(m_ssl_context, crt_pem_path.c_str(), SSL_FILETYPE_PEM);
        if (1 != i_ret)
        {
            LOG_ERROR("SSL_CTX_use_certificate_file error: %s", ERR_error_string(ERR_get_error(), nullptr));
            return;
        }
        std::string key_pem_path = get_key_pem();
        i_ret = SSL_CTX_use_PrivateKey_file(m_ssl_context, key_pem_path.c_str(), SSL_FILETYPE_PEM);
        if (1 != i_ret)
        {
            LOG_ERROR("SSL_CTX_use_PrivateKey_file error: %s", ERR_error_string(ERR_get_error(), nullptr));
            return;
        }
    }

    {
        LOG_ERROR("m_app_id %s", m_app_id.c_str());
        LOG_ERROR("m_ip %s", m_ip.c_str());
        LOG_ERROR("m_port %d", m_port);
        LOG_ERROR("m_worker_cnt %d", m_worker_cnt);
        LOG_ERROR("m_max_client_cnt %d", m_max_client_cnt);
        LOG_ERROR("m_epoll_wait_time %d", m_epoll_wait_time);
        LOG_ERROR("m_accept_per_tick %d", m_accept_per_tick);
        LOG_ERROR("m_http_static_dir %s", m_http_static_dir.c_str());
        LOG_ERROR("m_lua_dir %s", m_lua_dir.c_str());
        LOG_ERROR("m_task_type %s", m_task_type.c_str());
        LOG_ERROR("m_use_ssl %d", (int)m_use_ssl);
        LOG_ERROR("m_crt_pem %s", m_crt_pem.c_str());
        LOG_ERROR("m_key_pem %s", m_key_pem.c_str());
    }

    on_start();
}

void server::set_worker_cnt(size_t worker_cnt)
{
    m_worker_cnt = worker_cnt;
}

void server::set_max_client_cnt(size_t max_client_cnt)
{
    m_max_client_cnt = max_client_cnt;
}

void server::set_epoll_wait_time(size_t epoll_wait_time)
{
    m_epoll_wait_time = epoll_wait_time;
}

void server::set_task_type(std::string task_type)
{
    m_task_type = task_type;
}

void server::set_http_static_dir(std::string http_static_dir)
{
    m_http_static_dir = http_static_dir;
}

const std::string &server::get_http_static_dir()
{
    return m_http_static_dir;
}

void server::set_lua_dir(std::string lua_dir)
{
    m_lua_dir = lua_dir;
}

const std::string &server::get_lua_dir()
{
    return m_lua_dir;
}

task_type server::get_task_type()
{
    return str2task_type(m_task_type);
}

void server::set_use_ssl(bool use_ssl)
{
    m_use_ssl = use_ssl;
}

void server::set_crt_pem(std::string crt_pem)
{
    m_crt_pem = crt_pem;
}

void server::set_key_pem(std::string key_pem)
{
    m_key_pem = key_pem;
}

bool server::get_use_ssl()
{
    return m_use_ssl;
}

std::string server::get_crt_pem()
{
    return m_crt_pem;
}

std::string server::get_key_pem()
{
    return m_key_pem;
}

void server::config(const std::string &app_id,
                    const std::string &ip,
                    int port,
                    size_t worker_cnt,
                    size_t max_client_cnt,
                    size_t epoll_wait_time,
                    size_t accept_per_tick,
                    std::string task_type,
                    std::string http_static_dir,
                    std::string lua_dir,
                    std::string crt_pem /*= ""*/,
                    std::string key_pem /*= ""*/,
                    bool use_ssl /*= false*/)
{
    set_app_id(app_id);
    set_listen_info(ip, port);
    set_worker_cnt(worker_cnt);
    set_max_client_cnt(max_client_cnt);
    set_epoll_wait_time(epoll_wait_time);
    set_accept_per_tick(accept_per_tick);
    set_task_type(task_type);
    set_http_static_dir(http_static_dir);
    set_lua_dir(lua_dir);
    set_crt_pem(crt_pem);
    set_key_pem(key_pem);
    set_use_ssl(use_ssl);
}

bool server::is_stop()
{
    return stop_flag;
}

bool server::on_stop()
{
    if (stop_flag)
    {
        return true; // main process close
    }
    return false;
}

void server::to_stop()
{
    stop_flag = true;
    for (size_t i = 0; i < m_worker_cnt; i++)
    {
        m_workers[i].to_stop = true;
    }
}

SSL_CTX *server::get_ssl_ctx()
{
    return m_ssl_context;
}

void server::on_start()
{
    // time
    utility::time server_time;
    server_time.update();
    uint64_t latest_tick_time = server_time.get_seconds();
    uint64_t gid_seq = 0;

    // main m_epoller
    int iret = 0;
    {
        iret = m_epoller.create(m_max_client_cnt * 2);
        if (iret != 0)
        {
            LOG_ERROR("m_epoller.create(%d) iret[%d]", (m_max_client_cnt * 2), iret);
            return;
        }
    }

    // main_connection_mgr
    {
        iret = m_main_connection_mgr.init(m_max_client_cnt);
        if (iret != 0)
        {
            LOG_ERROR("m_main_connection_mgr.init failed[%d]", iret);
            return;
        }
    }

    // m_curr_connection_num
    {
        m_curr_connection_num.reset(new std::atomic<int>(0));
        if (!m_curr_connection_num)
        {
            LOG_ERROR("new std::atomic<int> m_curr_connection_num err");
            return;
        }
    }

    // main_worker_tunnel
    {
        m_main_worker_tunnel = new (std::nothrow) avant::socket::socket_pair[m_worker_cnt];
        if (!m_main_worker_tunnel)
        {
            LOG_ERROR("new socket_pair err");
            return;
        }
        // init tunnel
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            iret = m_main_worker_tunnel[i].init();
            if (iret != 0)
            {
                LOG_ERROR("m_main_worker_tunnel[%d] init failed", i);
                return;
            }
        }
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            if (0 != m_epoller.add(m_main_worker_tunnel[i].get_me(), nullptr, EPOLLIN | EPOLLOUT | EPOLLERR, true))
            {
                LOG_ERROR("main_epoller.add m_workers.main_worker_tunnel->get_me() failed %d", errno);
                return;
            }
            // main alloc connection for tunnel
            {
                iret = m_main_connection_mgr.alloc_connection(m_main_worker_tunnel[i].get_me(), gen_gid(latest_tick_time, ++gid_seq));
                if (iret != 0)
                {
                    LOG_ERROR("m_main_connection_mgr.alloc_connection for m_main_worker_tunnel return %d", iret);
                    return;
                }
                connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(m_main_worker_tunnel[i].get_me());
                tunnel_conn->recv_buffer.reserve(10485760); // 10MB
                tunnel_conn->send_buffer.reserve(10485760); // 10MB
            }
            m_main_worker_tunnel_fd2index.emplace(m_main_worker_tunnel[i].get_me(), i);
        }
    }

    // m_third_party_tunnel
    {
        iret = m_third_party_tunnel.init();
        if (iret != 0)
        {
            LOG_ERROR("main m_third_party_tunnel failed iret=%d", iret);
            return;
        }
        if (0 != m_epoller.add(m_third_party_tunnel.get_me(), nullptr, EPOLLIN | EPOLLOUT | EPOLLERR, true))
        {
            LOG_ERROR("main_epoller.add m_third_party_tunnel.get_me() failed %d", errno);
            return;
        }
        // main alloc connection for tunnel
        {
            iret = m_main_connection_mgr.alloc_connection(m_third_party_tunnel.get_me(), gen_gid(latest_tick_time, ++gid_seq));
            if (iret != 0)
            {
                LOG_ERROR("m_main_connection_mgr.alloc_connection for m_third_party_tunnel return %d", iret);
                return;
            }
            connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(m_third_party_tunnel.get_me());
            tunnel_conn->recv_buffer.reserve(10485760); // 10MB
            tunnel_conn->send_buffer.reserve(10485760); // 10MB
        }
    }

    // worker init
    {
        worker::worker *worker_arr = new worker::worker[m_worker_cnt];
        m_workers = worker_arr;
        if (!worker_arr)
        {
            LOG_ERROR("new worker::worker failed");
            return;
        }
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            m_workers[i].worker_id = i;
            m_workers[i].curr_connection_num = m_curr_connection_num;
            m_workers[i].main_worker_tunnel = &m_main_worker_tunnel[i];
            m_workers[i].use_ssl = m_use_ssl;

            connection::connection_mgr *new_connection_mgr = new (std::nothrow) connection::connection_mgr;
            if (!new_connection_mgr)
            {
                LOG_ERROR("new (std::nothrow) connection::connection_mgr failed");
                return;
            }
            std::shared_ptr<connection::connection_mgr> new_connection_mgr_shared_ptr(new_connection_mgr);
            iret = new_connection_mgr->init(m_max_client_cnt);
            if (iret != 0)
            {
                LOG_ERROR("new_connection_mgr->init failed");
                return;
            }
            m_workers[i].worker_connection_mgr = new_connection_mgr_shared_ptr;
            iret = m_workers[i].epoller.create(m_max_client_cnt * 2);
            if (iret != 0)
            {
                LOG_ERROR("m_epoller.create(%d) iret[%d]", (m_max_client_cnt * 2), iret);
                return;
            }

            // tunnel to worker_epoller
            if (0 != m_workers[i].epoller.add(m_workers[i].main_worker_tunnel->get_other(), nullptr, EPOLLIN | EPOLLOUT | EPOLLERR, true))
            {
                LOG_ERROR("m_workers.epoller.add m_workers.main_worker_tunnel->get_other() failed");
                return;
            }
            // worker alloc connection for tunnel
            iret = m_workers[i].worker_connection_mgr->alloc_connection(m_workers[i].main_worker_tunnel->get_other(), gen_gid(latest_tick_time, ++gid_seq));
            if (iret != 0)
            {
                LOG_ERROR("worker_connection_mgr.alloc_connection return %d", iret);
                return;
            }
            connection::connection *tunnel_conn = m_workers[i].worker_connection_mgr->get_conn(m_workers[i].main_worker_tunnel->get_other());
            tunnel_conn->recv_buffer.reserve(10485760); // 10MB
            tunnel_conn->send_buffer.reserve(10485760); // 10MB
        }
    }

    // worker thread start
    {
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            std::thread t(std::ref(m_workers[i]));
            t.detach();
        }
    }

    // listen_socket init
    server_socket listen_socket(m_ip, m_port, m_max_client_cnt);
    {
        LOG_ERROR("IP %s PORT %d", m_ip.c_str(), m_port);
        if (0 > listen_socket.get_fd())
        {
            LOG_ERROR("listen_socket failed get_fd() < 0");
            return;
        }
        if (0 != m_epoller.add(listen_socket.get_fd(), nullptr, EPOLLIN | EPOLLERR, true))
        {
            LOG_ERROR("listen_socket m_epoller add failed");
            return;
        }
    }

    // main_event_loop begin

    while (true)
    {
        int num = m_epoller.wait(10);
        // time update
        {
            server_time.update();
            uint64_t now_tick_time = server_time.get_seconds();
            if (latest_tick_time != now_tick_time)
            {
                int curr_connection_num = m_curr_connection_num->load();
                LOG_ERROR("curr_connection_num %d", curr_connection_num);

                if (stop_flag)
                {
                    bool flag = true;
                    // checking all worker stoped
                    for (size_t i = 0; i < m_worker_cnt; i++)
                    {
                        if (!m_workers[i].is_stoped)
                        {
                            flag = false;
                        }
                    }
                    if (flag)
                    {
                        break;
                    }
                }
                gid_seq = 0;
                latest_tick_time = now_tick_time;
            }
        }

        if (num < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                LOG_ERROR("main epoller.wait return %num errno %d", num, errno);
                break;
            }
        }

        for (int i = 0; i < num; i++)
        {
            int evented_fd = m_epoller.m_events[i].data.fd;
            uint32_t event_come = m_epoller.m_events[i].events;
            // listen_fd
            if (evented_fd == listen_socket.get_fd())
            {
                std::vector<int> clients_fd;
                for (size_t loop = 0; loop < m_accept_per_tick; loop++)
                {
                    int new_client_fd = listen_socket.accept();
                    if (new_client_fd < 0)
                    {
                        break;
                    }
                    else
                    {
                        if (*m_curr_connection_num >= (int)m_max_client_cnt)
                        {
                            LOG_ERROR("m_curr_connection_num >= m_max_client_cnt");
                            ::close(new_client_fd);
                            break;
                        }
                        *m_curr_connection_num += 1;
                        clients_fd.push_back(new_client_fd);
                    }
                }
                // new_client_fd come here
                if (!clients_fd.empty())
                {
                    for (int fd : clients_fd)
                    {
                        on_listen_event(fd, gen_gid(latest_tick_time, ++gid_seq));
                    }
                }
            }
            // worker_tunnel_fd
            else if (m_main_worker_tunnel_fd2index.find(evented_fd) != m_main_worker_tunnel_fd2index.end())
            {
                on_tunnel_event(m_main_worker_tunnel[m_main_worker_tunnel_fd2index[evented_fd]], event_come);
            }
            // third-party tunnel
            else if (m_third_party_tunnel.get_me() == evented_fd)
            {
                on_tunnel_event(m_third_party_tunnel, event_come);
            }
            else
            {
                LOG_ERROR("main epoller, undefined type fd");
                m_epoller.del(evented_fd, nullptr, 0);
                ::close(evented_fd);
            }
        }
    }
}

uint64_t server::gen_gid(uint64_t time_seconds, uint64_t gid_seq)
{
    return (time_seconds << 32) | gid_seq;
}

void server::on_listen_event(int new_client_fd, uint64_t gid)
{
    ProtoTunnelMain2WorkerNewClient main2worker_new_client;
    main2worker_new_client.set_fd(new_client_fd);
    main2worker_new_client.set_gid(gid);
    std::string body_str;
    body_str.resize(main2worker_new_client.ByteSizeLong());
    main2worker_new_client.SerializeToString(&body_str);

    ProtoPackage message;
    message.set_cmd(ProtoCmd::TUNNEL_MAIN2WORKER_NEW_CLIENT);
    message.set_body(body_str);

    std::string data;
    message.SerializeToString(&data);

    {
        uint64_t data_size = data.size();
        data_size = ::htobe64(data_size);
        char *data_size_arr = (char *)&data_size;
        data.insert(0, data_size_arr, sizeof(data_size));
    }

    int target_worker_idx = gid % m_worker_cnt;

    // send data to worker[target_worker_idx]
    avant::socket::socket_pair &target_tunnel_sockpair = m_main_worker_tunnel[target_worker_idx];
    connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(target_tunnel_sockpair.get_me());
    if (!tunnel_conn)
    {
        LOG_ERROR("tunnel_conn is null target_worker_idx[%d]", target_worker_idx);
        ::close(new_client_fd);
        *m_curr_connection_num -= 1;
        return;
    }

    tunnel_conn->send_buffer.append(data.c_str(), data.size());

    m_epoller.mod(target_tunnel_sockpair.get_me(), nullptr, EPOLLIN | EPOLLOUT | EPOLLERR);
}

void server::on_tunnel_event(avant::socket::socket_pair &tunnel, uint32_t event)
{
    // find conn
    connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(tunnel.get_me());
    if (!tunnel_conn)
    {
        LOG_ERROR("on_tunnel_event tunnel_conn is null, fd[%d]", tunnel.get_me());
        return;
    }

    avant::socket::socket &sock = tunnel.get_me_socket();

    // check if there is any content that needs to be read
    if (event & EPOLLIN)
    {
        constexpr int buffer_size = 102400;
        char buffer[buffer_size]{0};
        int buffer_used_idx{0};

        while (buffer_used_idx < buffer_size)
        {
            int len = 0;
            int oper_errno = 0;
            len = sock.recv(buffer + buffer_used_idx, buffer_size - buffer_used_idx, oper_errno);
            if (len > 0)
            {
                buffer_used_idx += len;
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR)
                {
                    LOG_ERROR("on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    to_stop();
                }
                break;
            }
        }
        if (buffer_used_idx > 0)
        {
            tunnel_conn->recv_buffer.append(buffer, buffer_used_idx);
        }

        // parser protocol
        while (!tunnel_conn->recv_buffer.empty())
        {
            ProtoPackage protoPackage;
            uint64_t data_size = 0;
            if (tunnel_conn->recv_buffer.size() >= sizeof(data_size))
            {
                data_size = *((uint64_t *)tunnel_conn->recv_buffer.get_read_ptr());
                data_size = ::be64toh(data_size);
                if (data_size + sizeof(data_size) > tunnel_conn->recv_buffer.size())
                {
                    break;
                }
            }
            if (!protoPackage.ParseFromArray(tunnel_conn->recv_buffer.get_read_ptr() + sizeof(data_size), data_size))
            {
                break;
            }
            on_tunnel_process(protoPackage);
            tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
        }
    }

    // check if there is any content that needs to be sent
    while (event & EPOLLOUT)
    {
        if (tunnel_conn->send_buffer.empty())
        {
            m_epoller.mod(sock.get_fd(), nullptr, EPOLLIN | EPOLLERR);
            break;
        }

        while (!tunnel_conn->send_buffer.empty())
        {
            int oper_errno = 0;
            int len = sock.send(tunnel_conn->send_buffer.get_read_ptr(), tunnel_conn->send_buffer.size(), oper_errno);
            if (len > 0)
            {
                tunnel_conn->send_buffer.move_read_ptr_n(len);
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR)
                {
                    LOG_ERROR("on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    to_stop();
                }
                break;
            }
        }
        break;
    }
}

void server::on_tunnel_process(ProtoPackage &message)
{
    int cmd = message.cmd();
    LOG_ERROR("server::on_tunnel_process %d", cmd);
}