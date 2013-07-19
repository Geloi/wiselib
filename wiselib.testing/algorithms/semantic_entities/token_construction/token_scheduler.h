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

#ifndef TOKEN_SCHEDULER_H
#define TOKEN_SCHEDULER_H

#include <external_interface/external_interface.h>
#include <external_interface/external_interface_testing.h>
#include <util/pstl/vector_dynamic.h>
#include <util/pstl/map_static_vector.h>
#include <util/pstl/list_dynamic.h>

#include <algorithms/protocols/reliable_transport/reliable_transport.h>
#include <algorithms/routing/ss/self_stabilizing_tree.h>
#include <algorithms/bloom_filter/bloom_filter.h>

#include "regular_event.h"
#include "semantic_entity.h"
#include "semantic_entity_aggregator.h"
#include "semantic_entity_amq_neighborhood.h"
#include "semantic_entity_forwarding.h"
#include "semantic_entity_id.h"
#include "semantic_entity_registry.h"
#include "token_state_message.h"

namespace wiselib {
	
	/**
	 * @brief
	 * 
	 * @ingroup
	 * 
	 * @tparam 
	 */
	template<
		typename OsModel_P,
		typename TupleStore_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Clock_P = typename OsModel_P::Clock,
		typename Debug_P = typename OsModel_P::Debug,
		typename Rand_P = typename OsModel_P::Rand
	>
	class TokenScheduler {
		
		public:
			// Typedefs & Enums
			// {{{
			
			typedef TokenScheduler<
				OsModel_P, TupleStore_P, Radio_P, Timer_P, Clock_P, Debug_P, Rand_P
			> self_type;
			typedef self_type* self_pointer_t;
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef Radio_P Radio;
			typedef typename Radio::node_id_t node_id_t;
			typedef typename Radio::message_id_t message_id_t;
			typedef Timer_P Timer;
			typedef Clock_P Clock;
			typedef typename Clock::time_t time_t;
			typedef ::uint32_t abs_millis_t;
			typedef Debug_P Debug;
			typedef Rand_P Rand;
			typedef TupleStore_P TupleStore;
			
			enum Restrictions {
				MAX_NEIGHBORS = 8
			};
			
			typedef NapControl<OsModel, Radio> NapControlT;
			typedef BloomFilter<OsModel, SemanticEntityId, 256> AmqT;
			typedef SelfStabilizingTree<OsModel, AmqT, Radio, Clock, Timer, Debug, NapControlT> GlobalTreeT;
			typedef ReliableTransport<OsModel, SemanticEntityId, Radio, Timer, Clock, Rand, Debug> ReliableTransportT;
			typedef SemanticEntity<OsModel, GlobalTreeT, Radio, Clock, Timer, MAX_NEIGHBORS> SemanticEntityT;
			typedef SemanticEntityAmqNeighborhood<OsModel, GlobalTreeT, AmqT, Radio> SemanticEntityNeighborhoodT;
			typedef SemanticEntityForwarding<OsModel, SemanticEntityNeighborhoodT, ReliableTransportT, Radio> SemanticEntityForwardingT;
			typedef SemanticEntityAggregator<OsModel, TupleStore, ::uint32_t> SemanticEntityAggregatorT;
			typedef SemanticEntityRegistry<OsModel, SemanticEntityT, GlobalTreeT> SemanticEntityRegistryT;
			typedef delegate2<void, SemanticEntityT&, SemanticEntityAggregatorT&> end_activity_callback_t;
			typedef TokenStateMessage<OsModel, SemanticEntityT, Radio> TokenStateMessageT;
			typedef typename TokenStateMessageT::TokenState TokenState;
			
			enum SpecialAddresses {
				BROADCAST_ADDRESS = Radio::BROADCAST_ADDRESS,
				NULL_NODE_ID = Radio::NULL_NODE_ID
			};
			
			enum SpecialValues {
				npos = (size_type)(-1)
			};
			
			enum ReturnValues {
				SUCCESS = OsModel::SUCCESS,
				ERR_UNSPEC = OsModel::ERR_UNSPEC
			};
			
			enum Timings {
				HANDOVER_RETRY_INTERVAL = 1000,
				
				/**
				 * How long should we keep the token once we have it?
				 */
				ACTIVITY_PERIOD = 1000,
			};
			
			// }}}
			
			class PacketInfo {
				// {{{
				public:
					static PacketInfo* create(time_t received, node_id_t from, typename Radio::size_t len, block_data_t* data) {
						PacketInfo *r = reinterpret_cast<PacketInfo*>(
							::get_allocator().template allocate_array<block_data_t>(sizeof(PacketInfo) + len).raw()
						);
						r->received_ = received;
						r->from_ = from;
						r->len_ = len;
						memcpy(r->data_, data, len);
						return r;
					}
					
					void destroy() {
						::get_allocator().template free_array(reinterpret_cast<block_data_t*>(this));
					}
					
					time_t& received() { return received_; }
					node_id_t& from() { return from_; }
					typename Radio::size_t& length() { return len_; }
					block_data_t *data() { return data_; }
				
				private:
					time_t received_;
					typename Radio::size_t len_;
					node_id_t from_;
					block_data_t data_[0];
				// }}}
			};
			
			void init(typename TupleStore::self_pointer_t tuplestore, typename Radio::self_pointer_t radio, typename Timer::self_pointer_t timer, typename Clock::self_pointer_t clock, typename Debug::self_pointer_t debug, typename Rand::self_pointer_t rand) {
				radio_ = radio;
				timer_ = timer;
				clock_ = clock;
				debug_ = debug;
				rand_ = rand;
				
				nap_control_.init(radio_);
				radio_->template reg_recv_callback<self_type, &self_type::on_receive>(this);
				radio_->enable_radio();
				transport_.init(radio_, timer_, clock_, rand_, debug_, false);
				end_activity_callback_ = end_activity_callback_t();
				
				global_tree_.init(radio_, clock_, timer_, debug_, &nap_control_);
				global_tree_.reg_event_callback(
						GlobalTreeT::event_callback_t::template from_method<self_type, &self_type::on_global_tree_event>(this)
				);
				registry_.init(&global_tree_);
				
				neighborhood_.init(&global_tree_, radio_);
				forwarding_.init(radio_, &neighborhood_);
				
				aggregator_.init(tuplestore);
			}
			
			void set_end_activity_callback(end_activity_callback_t cb) {
				end_activity_callback_ = cb;
			}
			
			void add_entity(const SemanticEntityId& se_id) {
				SemanticEntityT& se = registry_.add(se_id);
				
				transport_.register_endpoint(
						NULL_NODE_ID, se_id, true,
						ReliableTransportT::produce_callback_t::template from_method<self_type, &self_type::produce_handover_initiator>(this),
						ReliableTransportT::consume_callback_t::template from_method<self_type, &self_type::consume_handover_initiator>(this),
						ReliableTransportT::event_callback_t::template from_method<self_type, &self_type::event_handover_initiator>(this)
				);
				
				transport_.register_endpoint(
						NULL_NODE_ID, se_id, false,
						ReliableTransportT::produce_callback_t::template from_method<self_type, &self_type::produce_handover_recepient>(this),
						ReliableTransportT::consume_callback_t::template from_method<self_type, &self_type::consume_handover_recepient>(this),
						ReliableTransportT::event_callback_t::template from_method<self_type, &self_type::event_handover_recepient>(this)
				);
				
				se.template schedule_activating_token<
					self_type, &self_type::begin_wait_for_token, &self_type::end_wait_for_token
				>(clock_, timer_, this, &se);
				
				debug_->debug("node %d // added SE %x.%x is_active %d",
						(int)radio_->id(), (int)se.id().rule(), (int)se.id().value(),
						(int)se.is_active(radio_->id()));
				
				if(se.is_active(radio_->id())) {
					begin_activity(se);
				}
			}
		
		private:
			
			void on_receive(node_id_t from, typename Radio::size_t len, block_data_t* data) {
				message_id_t message_type = wiselib::read<OsModel, block_data_t, message_id_t>(data);
				if(message_type != ReliableTransportT::Message::MESSAGE_TYPE) {
					return;
				}
				
				time_t now = clock_->time();
				PacketInfo *p = PacketInfo::create(now, from, len, data);
				timer_->template set_timer<self_type, &self_type::on_receive_task>(0, this, (void*)p);
			}
			
			void on_receive_task(void *p) {
				PacketInfo *packet_info = reinterpret_cast<PacketInfo*>(p);
				//abs_millis_t t_recv = absolute_millis(packet_info->received());
				const node_id_t &from = packet_info->from();
				const typename Radio::size_t &len = packet_info->length();
				block_data_t *data = packet_info->data();
				//message_id_t message_type = wiselib::read<OsModel, block_data_t, message_id_t>(data);
				
				bool r = forwarding_.on_receive(from, len, data);
				if(!r) {
					transport_.on_receive(from, len, data);
				}
				
				packet_info->destroy();
			}
			
			void on_global_tree_event(typename GlobalTreeT::EventType e) {
				//if(e & GlobalTreeT::TOPOLOGY_CHANGES) {
					for(typename SemanticEntityRegistryT::iterator iter = registry_.begin(); iter != registry_.end(); ++iter) {
						SemanticEntityT &se = iter->second;
						debug_->debug("node %d SE %x.%x active %d // because tree change",
								(int)radio_->id(), (int)se.id().rule(), (int)se.id().value(),
								(int)se.is_active(radio_->id()));
						
						//if(se.is_active(radio_->id())) { begin_activity(se); }
						//else { end_activity(se); }
						transport_.set_remote_address(se.id(), true, neighborhood_.next_token_node(se.id()));
						transport_.set_remote_address(se.id(), false, neighborhood_.prev_token_node(se.id()));
						
						nap_control_.push_caffeine();
						initiate_handover(se); // tree has changed, (re-)send token info
					} // for
				//}
			} // global_tree_event()
			
			///@name Token Handover
			///@{
			//{{{
			
			
			//@{ Initiator (Token sending side)
			
			void initiate_handover(void *se_) {
				initiate_handover(*reinterpret_cast<SemanticEntityT*>(se_));
			}
			
			void initiate_handover(SemanticEntityT& se) {
				bool found;
				typename ReliableTransportT::Endpoint &ep = transport_.get_endpoint(se.id(), true, found);
				
				if(found &&
						ep.remote_address() != NULL_NODE_ID &&
						ep.remote_address() != radio_->id() &&
						transport_.open(ep, true) == SUCCESS
				) {
					debug_->debug("// initiating token handover %d -> %d SE %x.%x",
							(int)radio_->id(), (int)ep.remote_address(), (int)se.id().rule(),
							(int)se.id().value());
					se.set_handover_state_initiator(SemanticEntityT::INIT);
					transport_.flush();
				}
				else {
					nap_control_.pop_caffeine();
				}
			}
			
			bool produce_handover_initiator(typename ReliableTransportT::Message& message, typename ReliableTransportT::Endpoint& endpoint) {
				if(endpoint.remote_address() == radio_->id()) { return false; }
				const SemanticEntityId &id = endpoint.channel();
				SemanticEntityT *se = registry_.get(id);
				if(!se) { return false; }
				
				switch(se->handover_state_initiator()) {
					case SemanticEntityT::INIT: {
						TokenStateMessageT &msg = *reinterpret_cast<TokenStateMessageT*>(message.payload());
						msg.set_token_state(se->token());
						message.set_payload_size(msg.size());
						transport_.expect_answer(endpoint);
						return true;
					}
					
					case SemanticEntityT::SEND_AGGREGATES_START: {
						bool call_again;
						size_type sz = aggregator_.fill_buffer_start(id, message.payload(), ReliableTransportT::Message::MAX_PAYLOAD_SIZE, call_again);
						message.set_payload_size(sz);
						if(call_again) {
							se->set_handover_state_initiator(SemanticEntityT::SEND_AGGREGATES);
							endpoint.request_send();
						}
						else {
							endpoint.request_close();
						}
						return true;
					}
					
					case SemanticEntityT::SEND_AGGREGATES: {
						bool call_again;
						size_type sz = aggregator_.fill_buffer(id, message.payload(), ReliableTransportT::Message::MAX_PAYLOAD_SIZE, call_again);
						message.set_payload_size(sz);
						if(call_again) {
							se->set_handover_state_initiator(SemanticEntityT::SEND_AGGREGATES);
						}
						else {
							endpoint.request_close();
						}
						endpoint.request_send();
						return true;
					}
					
					case SemanticEntityT::CLOSE: {
						message.set_payload_size(0);
						endpoint.request_close();
						return false;
					}
				} // switch();
				
				return false;
			}
			
			void consume_handover_initiator(typename ReliableTransportT::Message& message, typename ReliableTransportT::Endpoint& endpoint) {
				if(endpoint.remote_address() == radio_->id()) { return; }
				const SemanticEntityId &id = endpoint.channel();
				SemanticEntityT *se = registry_.get(id);
				if(!se) { return; }
				
				if(*message.payload() == 'a') {
					se->set_handover_state_initiator(SemanticEntityT::SEND_AGGREGATES_START);
					endpoint.request_send();
				}
				else {
					endpoint.request_close();
				}
			}
			
			void event_handover_initiator(int event, typename ReliableTransportT::Endpoint& endpoint) {
				if(endpoint.remote_address() == radio_->id()) { return; }
				const SemanticEntityId &id = endpoint.channel();
				SemanticEntityT *se = registry_.get(id);
				if(!se) { return; }
				
				switch(event) {
					case ReliableTransportT::EVENT_ABORT:
						nap_control_.push_caffeine();
						timer_->template set_timer<self_type, &self_type::initiate_handover>(HANDOVER_RETRY_INTERVAL, this, se);
						break;
						
					case ReliableTransportT::EVENT_OPEN:
						se->set_handover_state_initiator(SemanticEntityT::INIT);
						nap_control_.push_caffeine();
						break;
						
					case ReliableTransportT::EVENT_CLOSE:
						se->set_handover_state_initiator(SemanticEntityT::INIT);
						nap_control_.pop_caffeine();
						nap_control_.pop_caffeine();
						break;
				}
			}
			
			//@}
			
			//@{ Recepient (Token receiving side)
			
			bool produce_handover_recepient(typename ReliableTransportT::Message& message, typename ReliableTransportT::Endpoint& endpoint) {
				if(endpoint.remote_address() == radio_->id()) { return false; }
				const SemanticEntityId &id = endpoint.channel();
				SemanticEntityT *se = registry_.get(id);
				if(!se) { return false; }
				
				switch(se->handover_state_recepient()) {
					case SemanticEntityT::SEND_ACTIVATING:
						se->set_handover_state_recepient(SemanticEntityT::RECV_AGGREGATES);
						*message.payload() = 'a';
						message.set_payload_size(1);
						transport_.expect_answer(endpoint);
						return true;
						
					case SemanticEntityT::SEND_NONACTIVATING:
						*message.payload() = 'n';
						message.set_payload_size(1);
						transport_.expect_answer(endpoint);
						return true;
				} // switch()
				return false;
			}
			
			void consume_handover_recepient(typename ReliableTransportT::Message& message, typename ReliableTransportT::Endpoint& endpoint) {
				if(endpoint.remote_address() == radio_->id()) { return; }
				const SemanticEntityId &id = endpoint.channel();
				SemanticEntityT *se = registry_.get(id);
				if(!se) { return; }
				
				switch(se->handover_state_recepient()) {
					case SemanticEntityT::INIT: {
						TokenStateMessageT &msg = *reinterpret_cast<TokenStateMessageT*>(message.payload());
						bool activating = process_token_state(msg, *se, endpoint.remote_address(), now(), message.delay());
						se->set_handover_state_recepient(activating ? SemanticEntityT::SEND_ACTIVATING : SemanticEntityT::SEND_NONACTIVATING);
						endpoint.request_send();
						break;
					}
					
					case SemanticEntityT::RECV_AGGREGATES: {
						aggregator_.read_buffer(message.channel(), message.payload(), message.payload_size());
						break;
					}
				} // switch()
			}
			
			void event_handover_recepient(int event, typename ReliableTransportT::Endpoint& endpoint) {
				if(endpoint.remote_address() == radio_->id()) { return; }
				const SemanticEntityId &id = endpoint.channel();
				SemanticEntityT *se = registry_.get(id);
				if(!se) { return; }
				
				switch(event) {
					case ReliableTransportT::EVENT_OPEN:
						nap_control_.push_caffeine();
						se->set_handover_state_recepient(SemanticEntityT::INIT);
						break;
						
					case ReliableTransportT::EVENT_CLOSE:
						nap_control_.pop_caffeine();
						se->set_handover_state_recepient(SemanticEntityT::INIT);
						break;
				}
			}
			
			//@}
			
			//}}}
			///@}
			
			bool process_token_state(TokenStateMessageT msg, SemanticEntityT& se, node_id_t from, abs_millis_t t_recv, abs_millis_t delay = 0) {
				TokenState s = msg.token_state();
				bool activating = false;
				bool active_before = se.is_active(radio_->id());
				se.set_prev_token_count(s.count());
				if(se.is_active(radio_->id()) && !active_before) {
					activating = true;
					se.learn_activating_token(clock_, radio_->id(), t_recv - delay);
					begin_activity(se);
				}
				else if(!se.is_active(radio_->id()) && active_before) {
					end_activity(&se);
				}
				return activating;
			}
			
			void begin_activity(void* se_)  {
				SemanticEntityT &se = *reinterpret_cast<SemanticEntityT*>(se_);
				begin_activity(se);
			}
				
			void begin_activity(SemanticEntityT &se) {
				
				// begin_activity might have been called at beginning
				// and then again (during the actual activity)
				
				if(se.in_activity_phase()) { return; }
				se.begin_activity_phase();
				nap_control_.push_caffeine();
				
				debug_->debug("node %d SE %x.%x active %d",
						(int)radio_->id(), (int)se.id().rule(), (int)se.id().value(),
						(int)se.in_activity_phase());
				
				timer_->template set_timer<self_type, &self_type::end_activity>(ACTIVITY_PERIOD, this, reinterpret_cast<void*>(&se));
			}
			
			/**
			 * Called by timeout at the end of an activity period.
			 */
			void end_activity(SemanticEntityT& se) {
				if(!se.in_activity_phase()) { return; }
				se.end_activity_phase();
				if(end_activity_callback_) {
					end_activity_callback_(se, aggregator_);
				}
				
				se.update_token_state(radio_->id());
				assert(!se.is_active(radio_->id()));
				
				initiate_handover(se);
				se.end_wait_for_activating_token();
				
				debug_->debug("node %d SE %x.%x active %d",
						(int)radio_->id(), (int)se.id().rule(), (int)se.id().value(),
						(int)se.in_activity_phase());
				
				se.template schedule_activating_token<self_type, &self_type::begin_wait_for_token, &self_type::end_wait_for_token>(clock_, timer_, this, &se);
			}
			
			/// ditto.
			void end_activity(void* se_) { end_activity(*reinterpret_cast<SemanticEntityT*>(se_)); }
			
			
			void begin_wait_for_token(void* se_) {
				nap_control_.push_caffeine();
			}
			
			void end_wait_for_token(void* se_) {
				nap_control_.pop_caffeine();
			}
			
			abs_millis_t absolute_millis(const time_t& t) {
				return clock_->seconds(t) * 1000 + clock_->milliseconds(t);
			}
			
			abs_millis_t now() {
				return absolute_millis(clock_->time());
			}
			
			typename Radio::self_pointer_t radio_;
			typename Timer::self_pointer_t timer_;
			typename Clock::self_pointer_t clock_;
			typename Debug::self_pointer_t debug_;
			typename Rand::self_pointer_t rand_;
			
			SemanticEntityNeighborhoodT neighborhood_;
			SemanticEntityForwardingT forwarding_;
			SemanticEntityAggregatorT aggregator_;
			SemanticEntityRegistryT registry_;
			ReliableTransportT transport_;
			GlobalTreeT global_tree_;
			NapControlT nap_control_;
			
			end_activity_callback_t end_activity_callback_;
			
	}; // TokenScheduler
}

#endif // TOKEN_SCHEDULER_H

