/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ��p2p_net_service.cpp
* description    ��p2p net service
* date                  : 2018.01.28
* author            ��Bruce Feng
**********************************************************************************/
#include "p2p_net_service.h"
#include <cassert>
#include "server.h"
#include "conf_manager.h"
#include "tcp_acceptor.h"
#include "service_message_id.h"
#include "service_message_def.h"
#include "matrix_types.h"
#include "matrix_client_socket_channel_handler.h"
#include "matrix_server_socket_channel_handler.h"
#include "handler_create_functor.h"
#include "channel.h"

using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;


namespace matrix
{
    namespace service_core
    {

        void p2p_net_service::init_conf()
        {
            conf_manager *manager = (conf_manager *)g_server->get_module_manager()->get(conf_manager_name).get();
            assert(manager != NULL);

            //get listen ip and port conf
            m_host_ip = manager->count("host_ip") ? (*manager)["host_ip"].as<std::string>() : ip::address_v4::any().to_string(); 
            m_main_net_listen_port = manager->count("main_net_listen_port") ? (*manager)["main_net_listen_port"].as<uint16_t>() : DEFAULT_MAIN_NET_LISTEN_PORT;
            m_test_net_listen_port = manager->count("test_net_listen_port") ? (*manager)["test_net_listen_port"].as<uint16_t>() : DEFAULT_TEST_NET_LISTEN_PORT;
        }

        void p2p_net_service::init_acceptor()
        {
            //init ip and port
            init_conf();

            //ipv4 default
            tcp::endpoint ep(ip::address::from_string(m_host_ip), m_main_net_listen_port);

            //main net
            LOG_DEBUG << "p2p net service init main net, ip: " << m_host_ip << " port: " << m_main_net_listen_port;
            CONNECTION_MANAGER->start_listen(ep, &matrix_server_socket_channel_handler::create);

            //test net
            ep.port(m_test_net_listen_port);
            LOG_DEBUG << "p2p net service init test net, ip: " << m_host_ip << " port: " << m_test_net_listen_port;
            CONNECTION_MANAGER->start_listen(ep, &matrix_server_socket_channel_handler::create);

            //ipv6 left to later

        }

        void p2p_net_service::init_connector()
        {
            conf_manager *manager = (conf_manager *)g_server->get_module_manager()->get(conf_manager_name).get();

            if (!manager->count("peer"))
            {
                LOG_DEBUG << "local peer address is empty and will not connect peer nodes by conf";
                return;
            }

            //config format: peer address=117.30.51.196:11107
            std::vector<std::string> str_address = (*manager)["peer"].as<std::vector<std::string>>();

            int count = 0;
            for (auto it = str_address.begin(); it != str_address.end() && count <DEFAULT_CONNECT_PEER_NODE; it++)
            {
                std::string &addr = *it;
                size_t pos = addr.find(':');
                if (pos == std::string::npos)
                {
                    LOG_ERROR << "p2p net conf file invalid format: " << addr;
                    continue;
                }

                //get ip and port
                std::string ip = addr.substr(0, pos);
                std::string str_port = addr.substr(pos + 1, std::string::npos);

                //later check ip format later!!!

                try
                {
                    uint16_t port = (uint16_t)std::stoul(str_port);

                    tcp::endpoint ep(address_v4::from_string(ip), port);
                    m_peer_addresses.push_back(ep);

                    //start connect
                    LOG_DEBUG << "matrix connect peer address, ip: " << addr << " port: " << str_port;
                    CONNECTION_MANAGER->start_connect(ep, &matrix_client_socket_channel_handler::create);
                    count++;
                }
                catch (const std::exception &e)
                {
                    LOG_DEBUG << "invalid peer address, ip: " << addr << " port: " << str_port << "exception: " << e.what();
                    continue;
                }
            }
        }

        int32_t p2p_net_service::service_init(bpo::variables_map &options)
        {
            //init topic
            init_subscription();

            //init invoker
            init_invoker();

            //init listen
            init_acceptor();

            //init connect
            init_connector();

            return E_SUCCESS;
        }

        void p2p_net_service::init_subscription()
        {
            TOPIC_MANAGER->subscribe(TCP_CHANNEL_ERROR, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(CLIENT_CONNECT_NOTIFICATION, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(VER_REQ, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(VER_RESP, [this](std::shared_ptr<message> &msg) {return send(msg);});
        }

        void p2p_net_service::init_invoker()
        {
            invoker_type invoker;

            //tcp channel error
            invoker = std::bind(&p2p_net_service::on_tcp_channel_error, this, std::placeholders::_1);
            m_invokers.insert({ TCP_CHANNEL_ERROR, { invoker } });

            //client tcp connect success
            invoker = std::bind(&p2p_net_service::on_client_tcp_connect_notification, this, std::placeholders::_1);
            m_invokers.insert({ CLIENT_CONNECT_NOTIFICATION, { invoker } });

            //ver req
            invoker = std::bind(&p2p_net_service::on_ver_req, this, std::placeholders::_1);
            m_invokers.insert({ VER_REQ, { invoker } });

            //ver resp
            invoker = std::bind(&p2p_net_service::on_ver_resp, this, std::placeholders::_1);
            m_invokers.insert({ VER_RESP, { invoker } });
        }

        int32_t p2p_net_service::on_time_out(std::shared_ptr<core_timer> timer)
        {
            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_ver_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::make_shared<matrix::service_core::ver_resp>();

            //header
            resp_content->header.length = 0;
            resp_content->header.magic = TEST_NET;
            resp_content->header.msg_name = VER_RESP;
            resp_content->header.check_sum = 0;
            resp_content->header.session_id = 0;

            //body
            resp_content->body.version = PROTOCO_VERSION;

            resp_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
            resp_msg->set_name(VER_RESP);
            resp_msg->header.dst_sid = msg->header.src_sid;

            CONNECTION_MANAGER->send_message(resp_msg->header.dst_sid, resp_msg);
            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_ver_resp(std::shared_ptr<message> &msg)
        {
            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_tcp_channel_error(std::shared_ptr<message> &msg)
        {
            socket_id  sid = msg->header.src_sid;
            LOG_ERROR << "p2p net service received tcp channel error msg, " << sid.to_string();

            //if client, connect next
            if (CLIENT_SOCKET == sid.get_type())
            {

            }
            else
            {
                //
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_client_tcp_connect_notification(std::shared_ptr<message> &msg)
        {
            auto notification_content = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg);

            if (CLIENT_CONNECT_SUCCESS == notification_content->status)
            {
                //create ver_req message
                std::shared_ptr<message> req_msg = std::make_shared<message>();
                std::shared_ptr<matrix::service_core::ver_req> req_content = std::make_shared<matrix::service_core::ver_req>();

                //header
                req_content->header.length = 0;
                req_content->header.magic = TEST_NET;
                req_content->header.msg_name = VER_REQ;
                req_content->header.check_sum = 0;
                req_content->header.session_id = 0;

                //body
                req_content->body.version = PROTOCO_VERSION;
                req_content->body.time_stamp = std::time(nullptr);
                req_content->body.addr_me.ip = g_server->get_p2p_net_service()->get_host_ip();
                req_content->body.addr_me.port = g_server->get_p2p_net_service()->get_main_net_listen_port();

                tcp::endpoint ep = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg)->ep;
                req_content->body.addr_you.ip = ep.address().to_string();
                req_content->body.addr_you.port = ep.port();
                req_content->body.nonce = 0;                        //later
                req_content->body.start_height = 0;              //later

                req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
                req_msg->set_name(VER_REQ);
                req_msg->header.dst_sid = msg->header.src_sid;

                CONNECTION_MANAGER->send_message(req_msg->header.dst_sid, req_msg);
                return E_SUCCESS;
            }
            else
            {
                //cancel connect and connect next
                return E_SUCCESS;
            } 
        }
    }

}