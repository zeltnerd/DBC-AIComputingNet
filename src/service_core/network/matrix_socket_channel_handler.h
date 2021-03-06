/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   matrix_socket_channel_handler.h
* description    :   dbc socket channel handler for dbc network protocol layer
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#pragma once


#include <boost/asio/steady_timer.hpp>
#include "channel.h"
#include "tcp_socket_channel.h"
#include "socket_channel_handler.h"
#include "matrix_coder.h"
#include "server.h"
#include "service_message_id.h"
#include "flow_ctrl.h"


using namespace std;
using namespace matrix::core;
using namespace boost::asio;

#define SHAKE_HAND_INTERVAL                             5                  //shake hand interval

#define LOST_SHAKE_HAND_COUNT_MAX               3                  //max lost shake hand req
#define DEFAULT_WAIT_VER_REQ_INTERVAL           5                   //wait VER_REQ interval while socket accepted

namespace matrix
{
    namespace service_core
    {
        class msg_new_peer_node : public matrix::core::msg_base
        {
        public:
            socket_id       sid;
            std::string     node_id;
        };

        class matrix_socket_channel_handler : public socket_channel_handler
        {
        public:

            typedef void (timer_handler_type)(const boost::system::error_code &);

            matrix_socket_channel_handler(std::shared_ptr<channel> ch);

            virtual ~matrix_socket_channel_handler();

        public:

            virtual int32_t start() { return E_SUCCESS; }

            virtual int32_t stop();

            virtual int32_t on_read(channel_handler_context &ctx, byte_buf &in);

            virtual int32_t on_write(channel_handler_context &ctx, message &msg, byte_buf &buf);

            virtual int32_t on_error();

            virtual int32_t on_before_msg_send(message &msg) { return E_SUCCESS; }

            virtual int32_t on_after_msg_sent(message &msg) { return E_SUCCESS; }

            virtual int32_t on_after_msg_received(message &msg) { return E_SUCCESS; }

            virtual int32_t on_before_msg_receive() { return E_SUCCESS; }

            virtual bool is_logined() { return m_login_success; }

            virtual void on_shake_hand_timer_expired(const boost::system::error_code& error) {}

        protected:

            virtual void start_shake_hand_timer();

            virtual void stop_shake_hand_timer();

            virtual void start_shake_hand_timer_ext() {}

            void set_has_message(message &msg);

            void reset_has_message() { m_has_message = false; }

            void set_encode_context(channel_handler_context &ctx);

            void set_decode_context(channel_handler_context &ctx);

        private:
            bool validate_req_path(std::string msg_name, std::vector<std::string>& path);
            bool validate_resp_path(std::string msg_name, std::vector<std::string>& path);

        protected:

            bool m_stopped;

            std::shared_ptr<matrix_coder> m_coder;
            //fix coding and decoding conflict when p2p communication
            std::shared_ptr<matrix_coder> m_decoder;


            std::weak_ptr<channel> m_channel;
            steady_timer m_shake_hand_timer;

            //std::function<timer_handler_type> m_shake_hand_timer_handler;

            bool m_has_message;

            bool m_login_success;

            socket_id m_sid;

            std::shared_ptr<flow_ctrl> m_f_ctl = nullptr;
        };

    }

}