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
#include "id_generator.h"




using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace ai::dbc;



namespace ai
{
    namespace dbc
    {
        ai_power_requestor_service::ai_power_requestor_service()
            : m_req_training_task_db()
        {

        }

        int32_t ai_power_requestor_service::init_conf()
        {
            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::service_init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            ret = init_db();

            return ret;
        }


        void ai_power_requestor_service::init_subscription()
        {
            TOPIC_MANAGER->subscribe(typeid(cmd_start_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_start_training_req(msg); });

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

            //list training resp
            invoker = std::bind(&ai_power_requestor_service::on_list_training_resp, this, std::placeholders::_1);
            m_invokers.insert({ LIST_TRAINING_RESP,{ invoker } });

        }

        int32_t ai_power_requestor_service::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options options;
            options.create_if_missing = true;

            //get db path
            fs::path task_db_path = env_manager::get_db_path();
            if (false == fs::exists(task_db_path))
            {
                LOG_DEBUG << "db directory path does not exist and create db directory";
                fs::create_directory(task_db_path);
            }

            //check db directory
            if (false == fs::is_directory(task_db_path))
            {
                LOG_ERROR << "db directory path does not exist and exit";
                return E_DEFAULT;
            }

            task_db_path /= fs::path("req_training_task.db");
            LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

            //open db
            leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
            if (false == status.ok())
            {
                LOG_ERROR << "ai power requestor service init training task db error: " << status.ToString();
                return E_DEFAULT;
            }

            //smart point auto close db
            m_req_training_task_db.reset(db);

            return E_SUCCESS;
        }

        void ai_power_requestor_service::init_timer()
        {
            m_timer_invokers[LIST_TRAINING_TIMER] = std::bind(&ai_power_requestor_service::on_list_training_timer, this, std::placeholders::_1);
        }

        int32_t ai_power_requestor_service::on_cmd_start_training_req(std::shared_ptr<message> &msg)
        {
            bpo::variables_map vm;

            std::shared_ptr<base> content = msg->get_content();
            std::shared_ptr<cmd_start_training_req> req = std::dynamic_pointer_cast<cmd_start_training_req>(content);
            assert(nullptr != req && nullptr != content);

            std::shared_ptr<ai::dbc::cmd_start_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->task_id = "";

            //check file exist
            fs::path task_file_path = fs::system_complete(fs::path(req->task_file_path.c_str()));
            if (false == fs::exists(task_file_path) || false == fs::is_regular_file(task_file_path))
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "training task file does not exist";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            //parse task config file
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
                bpo::store(bpo::parse_config_file(conf_task, task_config_opts), vm);
                bpo::notify(vm);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "parse ai training task error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            //validate parameters
            if (E_DEFAULT == validate_cmd_training_task_conf(vm))
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "parse ai training task parameters error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            //prepare broadcast req
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::start_training_req> broadcast_req_content = std::make_shared<matrix::service_core::start_training_req>();

            id_generator gen;
            broadcast_req_content->header.magic = TEST_NET;
            broadcast_req_content->header.msg_name = AI_TRAINING_NOTIFICATION_REQ;
            broadcast_req_content->header.__set_nonce(gen.generate_nonce());

            broadcast_req_content->body.task_id = gen.generate_task_id();
            broadcast_req_content->body.select_mode = vm["select_mode"].as<int8_t>();
            broadcast_req_content->body.master = vm["master"].as<std::string>();
            broadcast_req_content->body.peer_nodes_list = vm["peer_nodes_list"].as<std::vector<std::string>>();
            broadcast_req_content->body.server_specification = vm["server_specification"].as<std::string>();
            broadcast_req_content->body.server_count = vm["server_count"].as<int32_t>();
            broadcast_req_content->body.training_engine = vm["training_engine"].as<int32_t>();
            broadcast_req_content->body.code_dir = vm["code_dir"].as<std::string>();
            broadcast_req_content->body.entry_file = vm["entry_file"].as<std::string>();
            broadcast_req_content->body.data_dir = vm["data_dir"].as<std::string>();
            broadcast_req_content->body.checkpoint_dir = vm["checkpoint_dir"].as<std::string>();
            broadcast_req_content->body.hyper_parameters = vm["hyper_parameters"].as<std::string>();

            req_msg->set_content(std::dynamic_pointer_cast<base>(broadcast_req_content));
            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);

            LOG_DEBUG << "ai power requestor service broadcast start training msg, nonce: " << broadcast_req_content->header.nonce;

            CONNECTION_MANAGER->broadcast_message(req_msg);

            //peer won't reply, so public resp directly
            cmd_resp->result = E_SUCCESS;
            cmd_resp->task_id = broadcast_req_content->body.task_id;
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

            //flush to db
            write_task_info_to_db(broadcast_req_content->body.task_id);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::validate_cmd_training_task_conf(const bpo::variables_map &vm)
        {
            if (0 == vm.count("select_mode")
                || 0 == vm.count("master")
                || 0 == vm.count("peer_nodes_list")
                || 0 == vm.count("server_specification")
                || 0 == vm.count("server_count")
                || 0 == vm.count("training_engine")
                || 0 == vm.count("code_dir")
                || 0 == vm.count("entry_file")
                || 0 == vm.count("data_dir")
                || 0 == vm.count("checkpoint_dir")
                || 0 == vm.count("hyper_parameters")
                )
            {
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        int32_t ai_power_requestor_service::on_cmd_start_multi_training_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<base> content = msg->get_content();
            std::shared_ptr<cmd_start_multi_training_req> cmd_req_content = std::dynamic_pointer_cast<cmd_start_multi_training_req>(content);

            //cmd resp
            std::shared_ptr<ai::dbc::cmd_start_multi_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_multi_training_resp>();

            if (!cmd_req_content)
            {
                LOG_ERROR << "ai power requestor service on cmd start multi training received null msg";

                //error resp
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "start multi training error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            bpo::variables_map multi_vm;
            bpo::options_description multi_opts("multi task config file options");
            multi_opts.add_options()
                ("trainig_file", bpo::value<std::vector<std::string>>(), "");

            try
            {
                //parse multi task config file
                std::ifstream multi_task_conf(cmd_req_content->mulit_task_file_path);
                bpo::store(bpo::parse_config_file(multi_task_conf, multi_opts), multi_vm);
                bpo::notify(multi_vm);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "multi task config file parse error: " << diagnostic_information(e);

                //error resp
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "multi task config file parse error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            //parse task config empty
            if (0 == multi_vm.count("trainig_file") || 0 == multi_vm["trainig_file"].as<std::vector<std::string>>().size())
            {
                //error resp
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "multi task config file parse error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            //parse each task config
            bpo::variables_map vm;
            bpo::options_description opts("task config file options");
            add_task_config_opts(opts);

            const std::vector<std::string> & files = multi_vm["trainig_file"].as<std::vector<std::string>>();

            for (auto &file : files) 
            {
                auto req_msg = create_task_msg_from_file(file, opts);

                if (nullptr == req_msg)
                {
                    LOG_ERROR << "ai power requestor service create task msg from file error: " << file;
                    cmd_resp->task_list.push_back(file + ":  parse task config error");
                }
                else
                {
                    CONNECTION_MANAGER->broadcast_message(req_msg);

                    std::shared_ptr<matrix::service_core::start_training_req> req_content = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(req_msg->content);
                    assert(nullptr != req_content);

                    cmd_resp->task_list.push_back(req_content->body.task_id);

                    //flush to db
                    write_task_info_to_db(req_content->body.task_id);
                }
            }

            //public resp directly
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_cmd_stop_training_req(const std::shared_ptr<message> &msg)
        {
            std::shared_ptr<cmd_stop_training_req> cmd_req_content = std::dynamic_pointer_cast<cmd_stop_training_req>(msg->get_content());
            assert(nullptr != cmd_req_content);

            const std::string &task_id = cmd_req_content->task_id;

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::stop_training_req> req_content = std::make_shared<matrix::service_core::stop_training_req>();

            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = STOP_TRAINING_REQ;
            req_content->header.__set_nonce(id_generator().generate_nonce());

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
            req_content->header.__set_nonce(id_generator().generate_nonce());
            req_content->header.__set_session_id(id_generator().generate_session_id());
            
            //body
            if (1 == cmd_req->list_type) //0: list all tasks; 1: list specific tasks
            {
                //check task id exists 
                for (auto task_id : cmd_req->task_list)
                {
                    std::string task_value;
                    leveldb::Status status = m_req_training_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
                    if (!status.ok())
                    {
                        LOG_ERROR << "ai power requestor service cmd list task check task iderror: " << task_id;
                        continue;
                    }

                    //add to list task
                    req_content->body.task_list.push_back(task_id);
                    LOG_DEBUG << "ai power requestor service cmd list task: " << task_id;
                }
            }
            else
            {
                read_task_info_from_db(req_content->body.task_list);
            }

            //task list is empty
            if (req_content->body.task_list.empty())
            {
                std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task list is empty";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);

            //add to timer
            uint32_t timer_id = add_timer(LIST_TRAINING_TIMER, LIST_TRAINING_TIMER_INTERVAL, ONLY_ONE_TIME, req_content->header.session_id);
            assert(INVALID_TIMER_ID != timer_id);

            //service session
            std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, req_content->header.session_id);

            //session context
            variable_value val;
            val.value() = req_msg;
            session->get_context().add("req_msg", val);

            variable_value task_ids;
            task_ids.value() = std::make_shared<std::unordered_map<std::string, int8_t>>();
            session->get_context().add("task_ids", task_ids);

            //add to session
            int32_t ret = this->add_session(session->get_session_id(), session);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power requestor service list training add session error: " << session->get_session_id();
                return E_DEFAULT;
            }
            
            LOG_DEBUG << "ai power requestor service list training add session: " << session->get_session_id();

            //ok, broadcast
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

            std::shared_ptr<matrix::service_core::list_training_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::list_training_resp>(msg->content);
            if (!rsp_content)
            {
                LOG_ERROR << "recv list_training_resp but ctn is nullptr";
                return E_DEFAULT;
            }

            //broadcast resp
            CONNECTION_MANAGER->broadcast_message(msg);

            //get session
            std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
            if (nullptr == session)
            {
                LOG_DEBUG << "ai power requestor service get session null: " << rsp_content->header.session_id;
                return E_DEFAULT;
            }

            variables_map & vm = session->get_context().get_args();
            assert(vm.count("req_msg") > 0);
            assert(vm.count("task_ids") > 0);
            std::shared_ptr<std::unordered_map<std::string, int8_t>> task_ids = vm["task_ids"].as< std::shared_ptr<std::unordered_map<std::string, int8_t>>>();
            std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();

            for (auto ts : rsp_content->body.task_status_list)
            {
                LOG_DEBUG << "recv list_training_resp: " << ts.task_id << " status: " << to_training_task_status_string(ts.status);
                task_ids->insert({ts.task_id, ts.status});
            }

            auto req_content = std::dynamic_pointer_cast<matrix::service_core::list_training_req>(req_msg->get_content());

            if (task_ids->size() < req_content->body.task_list.size())
            {
                LOG_DEBUG << "ai power requestor service list task id less than " << req_content->body.task_list.size() << " and continue to wait";
                return E_SUCCESS;
            }

            //publish cmd resp
            std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            for (auto it = task_ids->begin(); it != task_ids->end(); it++)
            {
                cmd_task_status cts;
                cts.task_id = it->first;
                cts.status = it->second;
                cmd_resp->task_status_list.push_back(std::move(cts));
            }

            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);

            //remember: remove timer
            this->remove_timer(session->get_timer_id());

            //remember: remove session
            session->clear();
            this->remove_session(session->get_session_id());

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_list_training_timer(std::shared_ptr<core_timer> timer)
        {
            assert(nullptr != timer);

            //get session
            const string &session_id = timer->get_session_id();
            std::shared_ptr<service_session> session = get_session(session_id);
            if (nullptr == session)
            {
                LOG_ERROR << "ai power requestor service list training timer get session null: " << session_id;
                return E_DEFAULT;
            }

            variables_map & vm = session->get_context().get_args();
            assert(vm.count("task_ids") > 0);
            std::shared_ptr<std::unordered_map<std::string, int8_t>> task_ids = vm["task_ids"].as< std::shared_ptr<std::unordered_map<std::string, int8_t>>>();

            //publish cmd resp
            std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            for (auto it = task_ids->begin(); it != task_ids->end(); it++)
            {
                cmd_task_status cts;
                cts.task_id = it->first;
                cts.status = it->second;
                cmd_resp->task_status_list.push_back(std::move(cts));
            }

            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);

            //remember: remove session
            LOG_DEBUG << "ai power requestor service list training timer time out remove session: " << session_id;
            session->clear();
            this->remove_session(session_id);

            return E_SUCCESS;
        }

        void ai_power_requestor_service::add_task_config_opts(bpo::options_description &opts) const
        {
            opts.add_options()
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
            bpo::variables_map vm;
            
            try
            {
                std::ifstream conf_task(task_file);
                bpo::store(bpo::parse_config_file(conf_task, opts), vm);
                bpo::notify(vm);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);
                return nullptr;
            }

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::start_training_req> req_content = std::make_shared<matrix::service_core::start_training_req>();

            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = AI_TRAINING_NOTIFICATION_REQ;
            req_content->header.__set_nonce(id_generator().generate_nonce());

            try
            {
                req_content->body.task_id = id_generator().generate_task_id();
                req_content->body.select_mode = vm["select_mode"].as<int8_t>();
                req_content->body.master = vm["master"].as<std::string>();
                req_content->body.peer_nodes_list = vm["peer_nodes_list"].as<std::vector<std::string>>();
                req_content->body.server_specification = vm["server_specification"].as<std::string>();
                req_content->body.server_count = vm["server_count"].as<int32_t>();
                req_content->body.training_engine = vm["training_engine"].as<int32_t>();
                req_content->body.code_dir = vm["code_dir"].as<std::string>();
                req_content->body.entry_file = vm["entry_file"].as<std::string>();
                req_content->body.data_dir = vm["data_dir"].as<std::string>();
                req_content->body.checkpoint_dir = vm["checkpoint_dir"].as<std::string>();
                req_content->body.hyper_parameters = vm["hyper_parameters"].as<std::string>();
            }
            catch (...)
            {
                LOG_ERROR << "maybe some keys not exist in " << task_file;
                return nullptr;
            }
 
            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);
            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            return req_msg;
        }

        bool ai_power_requestor_service::write_task_info_to_db(std::string taskid)
        {
            if (!m_req_training_task_db || taskid.empty())
            {
                LOG_ERROR << "null ptr or null task_id.";
                return false;
            }

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;
            leveldb::Status s = m_req_training_task_db->Put(write_options, taskid, taskid);
            if (!s.ok())
            {
                LOG_ERROR << "write task(" << taskid << ") failed.";
                return false;
            }
            return true;
        }

        bool ai_power_requestor_service::read_task_info_from_db(std::vector<std::string> &vec)
        {
            if (!m_req_training_task_db)
            {
                LOG_ERROR << "level db not initialized.";
                return false;
            }

            //read from db
            std::string task_id;
            vec.clear();

            //iterate task in db
            std::unique_ptr<leveldb::Iterator> it;
            it.reset(m_req_training_task_db->NewIterator(leveldb::ReadOptions()));
            for (it->SeekToFirst(); it->Valid(); it->Next())
            {
                task_id = it->key().ToString();
                vec.push_back(task_id);

                LOG_DEBUG << "ai power requestor service read task: " << task_id;
            }

            return true;
        }

    }//service_core
}