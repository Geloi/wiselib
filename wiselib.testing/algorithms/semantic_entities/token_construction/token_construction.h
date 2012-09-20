/***************************************************************************
 ** This file is part of the generic algorithm library Wiselib.           **
 ** Copyright (C) 2008,2009 by the Wisebed (www.wisebed.eu) project.      **
 **                                                                       **
 ** The Wiselib is free software: you can redistribute it and/or modify   **
 ** it under the terms of the GNU Lesser General Public License as        **
 ** published by the Free Software Foundation, either version 3 of the    **
 ** License, or (at your option) any later version.                       **
 **                                                                       **
 ** The Wiselib is distributed in the hope that it will be useful,        **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of        **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
 ** GNU Lesser General Public License for more details.                   **
 **                                                                       **
 ** You should have received a copy of the GNU Lesser General Public      **
 ** License along with the Wiselib.                                       **
 ** If not, see <http://www.gnu.org/licenses/>.                           **
 ***************************************************************************/

#ifndef TOKEN_CONSTRUCTION_H
#define TOKEN_CONSTRUCTION_H

#include <util/pstl/list_static.h>
#include <util/pstl/map_static_vector.h>
#include <util/pstl/utility.h>
#include <util/pstl/pair.h>
#include <util/standalone_math.h>
#include "message_types.h"

namespace wiselib {
	
	template<
		typename OsModel_P,
		typename Radio_P,
		typename Timer_P = typename OsModel_P::Timer,
		typename Clock_P = typename OsModel_P::Clock,
		typename Rand_P = typename OsModel_P::Rand,
		typename Debug_P = typename OsModel_P::Debug
	>
	class TokenConstruction {
		public:
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef Radio_P Radio;
			typedef Debug_P Debug;
			typedef Timer_P Timer;
			typedef Clock_P Clock;
			typedef Rand_P Rand;
			
			typedef TokenConstruction<OsModel, Radio, Timer, Clock, Rand, Debug> self_type;
			
			typedef BridgeRequestMessage<Os, Radio> BridgeRequestMessageT;
			typedef DestructDetourMessage<Os, Radio> DestructDetourMessageT;
			typedef RenumberMessage<Os, Radio> RenumberMessageT;
			typedef EdgeRequestMessage<Os, Radio> EdgeRequestMessageT;
			typedef DropEdgeRequestMessage<Os, Radio> DropEdgeRequestMessageT;
			
			typedef typename Radio::node_id_t node_id_t;
			typedef uint8_t position_t;
			typedef StandaloneMath<OsModel> Math;
			typedef typename OsModel::size_t size_t;
			
			enum { MSG_TOKEN = 80, MSG_SURROGATE_TOKEN, MSG_BEACON, MSG_MERGE_REQUEST };
			static const double SHORTCUT_PROBABILITY = .3;
			
			enum Restrictions { MAX_CHANNELS = 8 };
			enum { BEACON_INTERVAL = 1000, TOKEN_STAY_INTERVAL = 5000 };
			
		private:
			
			enum { OPTION_END, OPTION_RENUMBER, OPTION_SHORTCUT };
			struct Option {
				
				Option(uint8_t type) : option_type(type) {
				}
				
				uint8_t option_type;
			};
			
			typedef Option EndOption;
			typedef Option RenumberOption;
			
			struct ShortcutOption : public Option {
				
				ShortcutOption() : Option(OPTION_SHORTCUT) {
				}
				
				node_id_t from;
				node_id_t to;
			};
			
			struct Token {
				
				Token() : message_type(MSG_TOKEN) {
				}
				
				uint8_t message_type;
				
				uint16_t real :1;
				uint16_t renumber :1;
				uint16_t stay_awake :1;
				
				uint16_t call_again;
				position_t nodes;
				position_t last_seen_number;
				node_id_t ring_id;
				
				Option options[0];
			};
			
			enum { SURROGATE_WAKE_ALL };
			
			
			struct Channel {
				node_id_t in_, out_;
				position_t number_;
				
				Channel() : in_(Radio::NULL_NODE_ID) {
				}
				
				bool used() { return in_ != Radio::NULL_NODE_ID; }
				void erase() { in_ = Radio::NULL_NODE_ID; }
				
				node_id_t in() { return in_; }
				void set_in(node_id_t i) { in_ = i; }
				node_id_t out() { return out_; }
				void set_out(node_id_t o) { out_ = o; }
				position_t number() { return number_; }
				void set_number(position_t n) { number_ = n; }
			};
			
			/// Messages
			// {{{
			
			struct Message {
				Message(uint8_t type) : message_type_(type) { }
				uint8_t message_type_;
			};
			
			struct Beacon : public Message {
				Beacon() : Message(MSG_BEACON) { }
				
				node_id_t ring_id() { return ring_id_; }
				void set_ring_id(node_id_t r) { ring_id_ = r; }
				node_id_t ring_id_;
			};
			
			struct MergeRequest : public Message {
				MergeRequest() : Message(MSG_MERGE_REQUEST) { }
			};
			
			// }}}
			
		public:
			typedef Channel Channels[MAX_CHANNELS];
			
			TokenConstruction() : received_token_(0), sending_token_(0) {
			}
			
			int init() {
				return OsModel::SUCCESS;
			}
			
			int init(typename Radio::self_pointer_t radio, typename Timer::self_pointer_t timer, typename Clock::self_pointer_t clock, typename Debug::self_pointer_t debug) {
				radio_ = radio;
				timer_ = timer;
				clock_ = clock;
				debug_ = debug;
				
				radio_->template reg_recv_callback<self_type, &self_type::on_receive>(this);
				radio_->enable_radio();
				
				channel_count_ = 0;
				
				return OsModel::SUCCESS;
			}
			
			int destruct() {
				return OsModel::SUCCESS;
			}
			
			void start_construction() {
				debug_->debug("starting token ring construction\n");
				//last_receive_ = clock_->seconds(clock_->time());
				//timer_->template set_timer<self_type, &self_type::on_time>(100, this, 0);
				
				set_ring_id(radio_->id());
				
				
				if(sending_token_) { get_allocator().free(sending_token_); sending_token_ = 0; }
				
				sending_token_ = get_allocator().allocate<Token>().raw();
				//sending_token_.real = true;
				//sending_token_.stay_awake = false;
				//sending_token_.ring_id = radio_->id();
				//sending_token_.nodes = 1;
				
				current_channel_ = add_channel(radio_->id(), radio_->id());
				awake_ = true;
				
				send_beacon();
			}
			
			/// Message sending
			// {{{
			
			void send_beacon(void* _=0) {
				if(awake()) {
					radio_->send(Radio::BROADCAST_ADDRESS, sizeof(beacon_), (block_data_t*)&beacon_);
				}
				
				timer_->template set_timer<self_type, &self_type::send_beacon>(BEACON_INTERVAL, this, 0);
			}
			
			void send_token(void   *_=0){
				// this will be called by timer set when receiving token
				// that will also set another timer that will trigger wakeup
				// and a timeout for the next token
				
				
				// TODO: if surrogate mode is active, send surrogate instead,
				
				node_id_t to = current_channel_->to;
				if(to == radio_->id()) {
					on_receive(to, sending_token_->length(), (typename Radio::block_data_t*)sending_token_);
				}
				else {
					// TODO: send via radio
				}
			}
			
			void send_merge_request(node_id_t to) {
				MergeRequest msg;
				radio_->send(to, sizeof(msg), (block_data_t*)&msg);
			}
			
			// }}}
			
			
			// }}}
			
			/// Message retrieval: on_receive_XXX
			// {{{
			
			void on_receive(typename Radio::node_id_t source, typename Radio::size_t len, typename Radio::block_data_t* data) {
				if(len < 1) { return; }
				if(!awake()) { return; }
				
				switch(data[0]) {
					case MSG_BEACON:
						{
							//debug_->debug("recv beacon %d->%d\n", source, radio_->id());
							Beacon beacon;
							memcpy(&beacon, data, sizeof(beacon));
							on_receive_beacon(source, beacon);
						}
						break;
						
					case MSG_MERGE_REQUEST:
						{
							debug_->debug("recv merge request %d->%d\n", source, radio_->id());
							MergeRequest merge_request;
							memcpy(&merge_request, data, sizeof(merge_request));
							on_receive_merge_request(source, merge_request);
						}
						break;
						
					case MSG_TOKEN:
						{
							debug_->debug("recv token %d->%d\n", source, radio_->id());
							if(received_token_) { get_allocator().free(received_token_); received_token_ = 0; }
							received_token_ = (Token*)get_allocator().allocate_array<block_data_t>(len).raw();
							memcpy(received_token_, data, len);
							on_receive_token(source, *received_token_);
						}
						break;
				}
				
				// IF state broadcast received from different ring, send MERGE_RINGS
				
				// IF state broadcast received from same ring, add shortcut
				// option with certain probability
				 
				// IF token received, process token
				
				
				//switch(data[0]) {
					//case MSG_CONSTRUCT: {
						////debug_->debug("%d recv: CONSTRUCT from %d\n", radio_->id(), source);
								
						////State state;
						////memcpy(&state, data, Math::template min<typename Radio::size_t>(len, sizeof(state)));
						////on_neighbor_state_received(source, state);
						//break;
					//}
					//default:
						//return;
				//}
				
				last_receive_ = clock_->seconds(clock_->time());
			}
			
			void on_receive_beacon(node_id_t source, Beacon& beacon) {
				if(beacon.ring_id() == ring_id()) {
					// possible shortcut
					if(rand_->operator()() <= SHORTCUT_PROBABILITY * Rand::RANDOM_MAX) {
						token_add_shortcut_option(ring_id(), beacon.ring_id());
					}
				}
				
				// ring with smaller id found, request its token
				else if(ring_id() > beacon.ring_id()) {
					start_stay_awake();
					send_merge_request(source);
					start_merge_master(source);
					start_surrogate_mode(SURROGATE_WAKE_ALL);
				}
			}
			
			void on_receive_merge_request(node_id_t source, MergeRequest& merge_request) {
				start_merge_slave(source);
				
				if(have_token()) {
					start_surrogate_mode(SURROGATE_WAKE_ALL);
					radio_->send(merge_partner_, received_token_size_, (block_data_t*)received_token_);
					end_merge_slave();
				}
			}
			
			void on_receive_token(node_id_t source, Token& token) {
				// insert time-sync code here some day
				
				// apply permanent wake state & new expected token time
				
				// stay awake for given wake time, then pass on token
				
				if(is_merge_master() && source == merge_partner_) {
					on_receive_merge_token(source, token);
				}
				else {
					on_receive_regular_token(source, token);
				}
				
				// start timer for passing on token
			}
			
			void on_receive_regular_token(node_id_t source, Token& token) {
				if(is_merge_slave()) {
					radio_->send(merge_partner_, received_token_size_, (block_data_t*)received_token_);
					start_surrogate_mode(SURROGATE_WAKE_ALL);
					end_merge_slave();
					add_channel(source, source);
				}
				
				if(is_wait_for_reschedule()) {
					// add timing info to token
				}
				
			}
			
			void on_receive_merge_token(node_id_t source, Token& token) {
				end_surrogate_mode(SURROGATE_WAKE_ALL);
				end_merge_master();
				
				add_channel(source, source);
				token_set_reschedule(); // set renumber flag on token, will make this node position 0, and count nodes,
				start_wait_for_reschedule();
				// next round it will disable stay awake mode and provide a
				// proper round time (which we will measure)
			}
			
			// }}}
			 
			/// Token manipulation & Options
			// {{{
			 
			void token_add_shortcut_option(node_id_t from, node_id_t to) {
			}
			
			void token_set_reschedule() {
			}
			
			void process_token_option(Option& option) {
				// apply renumbering
				// apply shortcuts
			}
			
			// }}}
			
			/// Sleep/wake states
			// {{{
			
			void wakeup() { awake_ = true; }
			void sleep() { awake_ = false; }
			bool awake() { return awake_; }
			
			void start_stay_awake() {
				stay_awake_ = true;
				wakeup();
			}
			void stop_stay_awake() { stay_awake_ = false; }
			bool staying_awake() { return stay_awake_; }
			
			// }}}
			
			/// Wait states
			// {{{
			
			void start_merge_master(node_id_t partner) {
				merge_partner_ = partner;
				merge_master_ = true;
			}
			void end_merge_master() { merge_master_ = false; }
			bool is_merge_master() { return merge_master_; }
			
			void start_merge_slave(node_id_t partner) {
				merge_partner_ = partner;
				merge_slave_ = true;
			}
			void end_merge_slave() { merge_slave_ = false; }
			bool is_merge_slave() { return merge_slave_; }
			
			void start_wait_for_reschedule() { wait_for_reschedule_ = true; }
			void end_wait_for_reschedule() { wait_for_reschedule_ = false; }
			bool is_wait_for_reschedule() { return wait_for_reschedule_; }
			
			void start_surrogate_mode(uint8_t) { surrogate_mode_ = true; }
			void end_surrogate_mode(uint8_t mode = 0) { surrogate_mode_ = false; }
			bool is_surrogate_mode() { return surrogate_mode_; }
			
			// }}}
			
			void set_ring_id(node_id_t id) { beacon_.set_ring_id(id); }
			node_id_t ring_id() { return beacon_.ring_id(); }
			
			bool have_token() { return have_token_; }
			
		private:
			/**
			 * CHANNEL_NEXT: add channel so that its out port will be next in
			 * line.
			 */
			Channel* add_channel(node_id_t in, node_id_t out) {
				Channel *ch;
				
				// if there is a this/this channel, remove it
				for(ch = channels_; ch < channels_ + MAX_CHANNELS; ch++) {
					if(ch->in() == radio_->id() && ch->out() == radio_->id()) {
						ch->erase();
						channel_count_--;
						break;
					}
				}
				
				if(channel_count_ == 0) {
					ch = &channels_[0];
					ch->set_in(in);
					ch->set_out(out);
				}
				else {
					node_id_t out_tmp = Radio::NULL_NODE_ID;
					
					ch = current_channel_;
					out_tmp = ch->out();
					ch->set_out(out);
					
					ch = next_free_channel(ch);
					ch->set_in(in);
					ch->set_out(out_tmp);
				}
				channel_count_++;
				
				return ch;
			}
			
			Channel* find_channel_by_in(node_id_t in) {
			}
			
			Channel* next_used_channel(Channel* ch) {
				Channel *end = ch;
				ch++;
				while(!ch->used() && ch != end) {
					ch++;
					if(ch >= channels_ + MAX_CHANNELS) { ch = channels_; }
				}
				return ch;
			}
			
			Channel* next_free_channel(Channel* ch) {
				Channel *end = ch;
				ch++;
				while(ch->used() && ch != end) {
					ch++;
					if(ch >= channels_ + MAX_CHANNELS) { ch = channels_; }
				}
				return ch;
			}
			
			
			typename Radio::self_pointer_t radio_;
			typename Timer::self_pointer_t timer_;
			typename Clock::self_pointer_t clock_;
			typename Debug::self_pointer_t debug_;
			typename Rand::self_pointer_t rand_;
			
			Token *received_token_, *sending_token_;
			size_t received_token_size_;
			
			Channel *current_channel_;
			Channels channels_;
			size_t channel_count_;
			
			Beacon beacon_;
			
			bool awake_;
			bool stay_awake_;
			bool have_token_;
			bool wait_for_reschedule_;
			bool merge_slave_;
			bool merge_master_;
			bool surrogate_mode_;
			
			node_id_t merge_partner_;
			
			uint32_t last_receive_;
			
	}; // TokenConstruction
}

#endif // TOKEN_CONSTRUCTION_H

