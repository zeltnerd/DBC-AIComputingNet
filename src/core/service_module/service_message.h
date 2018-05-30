﻿/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   service_message.h
* description    :   service message
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#ifndef _SERVICE_MESSAGE_H_
#define _SERVICE_MESSAGE_H_

#include "common.h"
#include "service_bus.h"
#include "protocol.h"
#include "socket_id.h"


typedef void * MESSAGE_PTR;


/*****************************************************************************

* 
* 
*                               定义发布到总线的消息
* 
* 

*****************************************************************************/

#ifdef __RTX

#define DEFAULT_MESSAGE_BODY_BYTES_LEN       256

struct message_header_t
{
    uint32_t message_id;
};

struct message_t
{
    message_header_t header;
    uint8_t body_bytes[DEFAULT_MESSAGE_BODY_BYTES_LEN];
};

//声明总线消息
TOPIC_DECLARE(message);

#else

namespace matrix
{
    namespace core
    {
        class inner_header
        {
        public:
            inner_header() : msg_name("unknown message"), msg_priority(0) {}

            std::string msg_name;
            uint32_t msg_priority;

            socket_id src_sid;
            socket_id dst_sid;
       };

        class message
        {
        public:
            message() = default;
            virtual ~message() = default;

            virtual std::string get_name() { return header.msg_name; }
            virtual void set_name(const std::string &name) { header.msg_name = name; }

            virtual uint32_t get_priority() { return header.msg_priority; }
            virtual void set_priority(uint32_t msg_priority) { header.msg_priority = msg_priority; }

            virtual std::shared_ptr<base> get_content() { return content; }
            virtual void set_content(std::shared_ptr<msg_base> content) { this->content = content; }

            virtual uint32_t validate() const { return content->validate(); }
            virtual uint32_t read(protocol * iprot) { return content->read(iprot); }
            virtual uint32_t write(protocol * oprot) const { return content->write(oprot); }

            inner_header header;
//            std::shared_ptr<base> content;
            std::shared_ptr<msg_base> content;
        };

    }

}

#endif


#endif


