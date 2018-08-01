/**
 * Autogenerated by Thrift Compiler (0.11.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef peers_db_TYPES_H
#define peers_db_TYPES_H

#include <iosfwd>
#include "protocol.h"
#include "TToString.h"
using namespace matrix::core;


namespace matrix { namespace service_core {

class db_peer_candidate;


class db_peer_candidate : public virtual ::apache::thrift::TBase {
 public:

  db_peer_candidate(const db_peer_candidate&);
  db_peer_candidate& operator=(const db_peer_candidate&);
  db_peer_candidate() : ip(), port(0), net_state(0), reconn_cnt(0), last_conn_tm(0), score(0), node_id(), node_type(0) {
  }

  virtual ~db_peer_candidate() throw();
  std::string ip;
  int16_t port;
  int8_t net_state;
  int32_t reconn_cnt;
  int64_t last_conn_tm;
  int32_t score;
  std::string node_id;
  int8_t node_type;

  void __set_ip(const std::string& val);

  void __set_port(const int16_t val);

  void __set_net_state(const int8_t val);

  void __set_reconn_cnt(const int32_t val);

  void __set_last_conn_tm(const int64_t val);

  void __set_score(const int32_t val);

  void __set_node_id(const std::string& val);

  void __set_node_type(const int8_t val);

  bool operator == (const db_peer_candidate & rhs) const
  {
    if (!(ip == rhs.ip))
      return false;
    if (!(port == rhs.port))
      return false;
    if (!(net_state == rhs.net_state))
      return false;
    if (!(reconn_cnt == rhs.reconn_cnt))
      return false;
    if (!(last_conn_tm == rhs.last_conn_tm))
      return false;
    if (!(score == rhs.score))
      return false;
    if (!(node_id == rhs.node_id))
      return false;
    if (!(node_type == rhs.node_type))
      return false;
    return true;
  }
  bool operator != (const db_peer_candidate &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const db_peer_candidate & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(db_peer_candidate &a, db_peer_candidate &b);

std::ostream& operator<<(std::ostream& out, const db_peer_candidate& obj);

}} // namespace

#endif