/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name     : timer_def.h
* description   : define timer ids and timer intervals
* date          : 2018.04.09
* author        : Allan
**********************************************************************************/

#pragma once

/*--------------------timer name------------------------*/
#define TIMER_NAME_FILTER_CLEAN                         "common_timer_filter_clean"
//#define TIMER_NAME_P2P_PEER_INFO_EXCHANGE               "p2p_peer_info_exchange"
//#define TIMER_NAME_P2P_CONNECT_NEW_PEER                 "p2p_connect_new_peer"
#define TIMER_NAME_P2P_ONE_MINUTE                       "p2p_timer_one_minute"


/*--------------------timer interval----------------------*/
#define TIMER_INTERV_SEC_FILTER_CLEAN               120
#define TIMER_INTERV_MIN_P2P_PEER_INFO_EXCHANGE         3
#define TIMER_INTERV_MIN_P2P_CONNECT_NEW_PEER           1
#define TIMER_INTERV_MIN_P2P_ONE_MINUTE                 1
#define TIMER_INTERV_MIN_P2P_PEER_CANDIDATE_DUMP        10