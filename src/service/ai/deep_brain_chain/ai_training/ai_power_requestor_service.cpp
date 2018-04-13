/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ��ai_power_requestor_service.cpp
* description    ��ai_power_requestor_service
* date                  : 2018.01.28
* author            ��Bruce Feng
**********************************************************************************/
#include <cassert>
#include "server.h"
#include "api_call_handler.h"
#include "conf_manager.h"
#include "tcp_acceptor.h"
#include "service_message_id.h"
#include "service_message_def.h"
#include "matrix_types.h"
#include "matrix_coder.h"
#include "matrix_client_socket_channel_handler.h"
#include "matrix_server_socket_channel_handler.h"
#include "handler_create_functor.h"
#include "channel.h"
#include "ip_validator.h"
#include "port_validator.h"
#include <boost/exception/all.hpp>
#include <iostream>
#include "ai_power_requestor_service.h"




using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace ai::dbc;



namespace matrix
{
	namespace service_core
	{

		int32_t ai_power_requestor_service::init_conf()
		{
			return E_SUCCESS;
		}

		int32_t ai_power_requestor_service::service_init(bpo::variables_map &options)
		{
			return E_SUCCESS;
		}


		void ai_power_requestor_service::init_subscription()
		{
			TOPIC_MANAGER->subscribe(typeid(cmd_start_training_req).name(), [this](std::shared_ptr<message> &msg) { return cmd_on_start_training_req(msg); });
			
            //cmd start multi training
            TOPIC_MANAGER->subscribe(typeid(cmd_start_multi_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_start_multi_training_req(msg);});

            //cmd stop training
            TOPIC_MANAGER->subscribe(typeid(cmd_stop_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_stop_training_req(msg); });

            //cmd list training
            TOPIC_MANAGER->subscribe(typeid(cmd_list_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_list_training_req(msg); });
            //list training resp
            TOPIC_MANAGER->subscribe(LIST_TRAINING_RESP, [this](std::shared_ptr<message> &msg) {return send(msg); });
		}

		void ai_power_requestor_service::init_invoker()
		{
			invoker_type invoker;

			invoker = std::bind(&ai_power_requestor_service::cmd_on_start_training_req, this, std::placeholders::_1);
			m_invokers.insert({ CMD_AI_TRAINING_NOTIFICATION_REQ,{ invoker } });

            //list training resp
            invoker = std::bind(&ai_power_requestor_service::on_list_training_resp, this, std::placeholders::_1);
            m_invokers.insert({ LIST_TRAINING_RESP,{ invoker } });

		}

		int32_t ai_power_requestor_service::cmd_on_start_training_req(std::shared_ptr<message> &msg)
		{
			std::shared_ptr<base> content = msg->get_content();
			std::shared_ptr<cmd_start_training_req> req = std::dynamic_pointer_cast<cmd_start_training_req>(content);
			assert(nullptr != req);

			bpo::options_description task_config_opts("task config file options");
			task_config_opts.add_options()
				("task_id", bpo::value<std::string>(), "")
				("select_mode", bpo::value<int8_t>()->default_value(0), "")
				("master", bpo::value<std::string>(), "")
				("peer_nodes_list", bpo::value<std::vector<std::string>>(), "")
				("server_specification", bpo::value<std::string>(), "")
				("server_count", bpo::value<int32_t>(), "")
				("training_engine", bpo::value<int32_t>(), "")
				("code_dir", bpo::value<std::string>(), "")
				("entry_file", bpo::value<std::string>(), "")
				("data_dir", bpo::value<std::string>(), "")
				("checkpoint_dir", bpo::value<std::string>(), "")
				("hyper_parameters", bpo::value<std::string>(), "");

			try
			{
				std::ifstream conf_task(req->task_file_path);
				::store(bpo::parse_config_file(conf_task, task_config_opts), vm);
				bpo::notify(vm);
			}
			catch (const boost::exception & e)
			{
				LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);
			}
			std::shared_ptr<message> req_msg = std::make_shared<message>();
			std::shared_ptr<matrix::service_core::start_training_req> resp_content = std::make_shared<matrix::service_core::start_training_req>();

			resp_content->header.magic = TEST_NET;
			resp_content->header.msg_name = AI_TRAINING_NOTIFICATION_REQ;
			resp_content->header.check_sum = 0;
			resp_content->header.session_id = 0;

			resp_content->body.task_id = vm["task_id"].as<std::string>();
			resp_content->body.select_mode = vm["select_mode"].as<int8_t>();
			resp_content->body.master = vm["master"].as<std::string>();
			resp_content->body.peer_nodes_list = vm["peer_nodes_list"].as<std::vector<std::string>>();
			resp_content->body.server_specification = vm["server_specification"].as<std::string>();
			resp_content->body.server_count = vm["server_count"].as<int32_t>();
			resp_content->body.training_engine = vm["training_engine"].as<int32_t>();
			resp_content->body.code_dir = vm["code_dir"].as<std::string>();
			resp_content->body.entry_file = vm["entry_file"].as<std::string>();
			resp_content->body.data_dir = vm["data_dir"].as<std::string>();
			resp_content->body.checkpoint_dir = vm["checkpoint_dir"].as<std::string>();
			resp_content->body.hyper_parameters = vm["hyper_parameters"].as<std::string>();

			req_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
			req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);

			CONNECTION_MANAGER->broadcast_message(req_msg);

            //peer won't reply, so public resp directly
            std::shared_ptr<ai::dbc::cmd_start_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            cmd_resp->task_id = resp_content->body.task_id;
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

			return E_SUCCESS;
		}

		int32_t ai_power_requestor_service::on_cmd_start_multi_training_req(std::shared_ptr<message> &msg)
		{
			std::shared_ptr<base> content = msg->get_content();
			std::shared_ptr<cmd_start_multi_training_req> req = std::dynamic_pointer_cast<cmd_start_multi_training_req>(content);
			assert(nullptr != req);

			bpo::options_description opts("task config file options");
			add_task_config_opts(opts);

			std::vector<std::string> files;
            string_util::split(req->mulit_task_file_path, ",", files);
			for (auto &file : files) {
				auto req_msg = create_task_msg_from_file(file, opts);
				CONNECTION_MANAGER->broadcast_message(req_msg);
			}
			return E_SUCCESS;
		}

        int32_t ai_power_requestor_service::on_cmd_stop_training_req(const std::shared_ptr<message> &msg)
        {
            const std::string &task_id = std::dynamic_pointer_cast<cmd_stop_training_req>(msg->get_content())->task_id;
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::stop_training_req> req_content = std::make_shared<matrix::service_core::stop_training_req>();
            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = STOP_TRAINING_REQ;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;
            //body
            req_content->body.task_id = task_id;

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);
            CONNECTION_MANAGER->broadcast_message(req_msg);

            //there's no reply, so public resp directly
            std::shared_ptr<ai::dbc::cmd_stop_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_stop_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_cmd_list_training_req(const std::shared_ptr<message> &msg)
        {
            auto cmd_req = std::dynamic_pointer_cast<cmd_list_training_req>(msg->get_content());
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::list_training_req>();
            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = LIST_TRAINING_REQ;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;
            //body
            if (cmd_req->list_type == 1) {
                req_content->body.task_list.assign(cmd_req->task_list.begin(), cmd_req->task_list.end());
            }

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);
            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_list_training_resp(std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "recv list_training_resp but msg is nullptr";
                return E_DEFAULT;
            }
            std::shared_ptr<matrix::service_core::list_training_resp> rsp_ctn = std::dynamic_pointer_cast<matrix::service_core::list_training_resp>(msg->content);
            if (!rsp_ctn)
            {
                LOG_ERROR << "recv list_training_resp but ctn is nullptr";
                return E_DEFAULT;
            }
            //broadcast resp
            CONNECTION_MANAGER->broadcast_message(msg);
            
            //public cmd resp
            std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            for (auto ts : rsp_ctn->body.task_status_list)
            {
                LOG_DEBUG << "recv list_training_resp: " << ts.task_id << " : " << ts.status;
                cmd_task_status cts;
                cts.task_id = ts.task_id;
                cts.status = ts.status;
                cmd_resp->task_status_list.push_back(std::move(cts));
            }
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        void ai_power_requestor_service::add_task_config_opts(bpo::options_description &opts) const
		{
			opts.add_options()
					("task_id", bpo::value<std::string>(), "")
					("select_mode", bpo::value<int8_t>()->default_value(0), "")
					("master", bpo::value<std::string>(), "")
					("peer_nodes_list", bpo::value<std::vector<std::string>>(), "")
					("server_specification", bpo::value<std::string>(), "")
					("server_count", bpo::value<int32_t>(), "")
					("training_engine", bpo::value<int32_t>(), "")
					("code_dir", bpo::value<std::string>(), "")
					("entry_file", bpo::value<std::string>(), "")
					("data_dir", bpo::value<std::string>(), "")
					("checkpoint_dir", bpo::value<std::string>(), "")
					("hyper_parameters", bpo::value<std::string>(), "");
		}

		std::shared_ptr<message> ai_power_requestor_service::create_task_msg_from_file(const std::string &task_file, const bpo::options_description &opts)
		{
			try
			{
				std::ifstream conf_task(task_file);
				::store(bpo::parse_config_file(conf_task, opts), vm);
				bpo::notify(vm);
			}
			catch (const boost::exception & e)
			{
				LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);
			}
			std::shared_ptr<message> req_msg = std::make_shared<message>();
			std::shared_ptr<matrix::service_core::start_training_req> resp_content = std::make_shared<matrix::service_core::start_training_req>();

			resp_content->header.magic = TEST_NET;
			resp_content->header.msg_name = AI_TRAINING_NOTIFICATION_REQ;
			resp_content->header.check_sum = 0;
			resp_content->header.session_id = 0;

			resp_content->body.task_id = vm["task_id"].as<std::string>();
			resp_content->body.select_mode = vm["select_mode"].as<int8_t>();
			resp_content->body.master = vm["master"].as<std::string>();
			resp_content->body.peer_nodes_list = vm["peer_nodes_list"].as<std::vector<std::string>>();
			resp_content->body.server_specification = vm["server_specification"].as<std::string>();
			resp_content->body.server_count = vm["server_count"].as<int32_t>();
			resp_content->body.training_engine = vm["training_engine"].as<int32_t>();
			resp_content->body.code_dir = vm["code_dir"].as<std::string>();
			resp_content->body.entry_file = vm["entry_file"].as<std::string>();
			resp_content->body.data_dir = vm["data_dir"].as<std::string>();
			resp_content->body.checkpoint_dir = vm["checkpoint_dir"].as<std::string>();
			resp_content->body.hyper_parameters = vm["hyper_parameters"].as<std::string>();

			req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);
			req_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
            return req_msg;
		}
	}

}