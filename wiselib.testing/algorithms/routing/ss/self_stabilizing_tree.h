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

#ifndef SELF_STABILIZING_TREE_H
#define SELF_STABILIZING_TREE_H

#include <external_interface/external_interface.h>
#include <external_interface/external_interface_testing.h>
#include <util/serialization/serialization.h>
#include <algorithms/semantic_entities/token_construction/nap_control.h>
#include "tree_state_message.h"
#include <algorithms/semantic_entities/token_construction/regular_event.h>
#include <util/types.h>
#include <util/pstl/set_static.h>
#include <util/pstl/map_static_vector.h>
#include <util/string_util.h>

//#define INSE_USE_LINK_METRIC 1

#ifndef INSE_BCAST_INTERVAL
	#define INSE_BCAST_INTERVAL (10000 * WISELIB_TIME_FACTOR)
#endif

namespace wiselib {
	
	/**
	 * @brief
	 * 
	 * @ingroup Neighborhood_concept
	 * 
	 * @tparam 
	 */
	template<
		typename OsModel_P,
		typename UserData_P,
		typename Radio_P,
		typename Clock_P,
		typename Timer_P,
		typename Debug_P,
		typename NapControl_P,
		int MAX_NEIGHBORS_P,
		int MAX_EVENT_LISTENERS_P
	>
	class SelfStabilizingTree {
		public:
			typedef SelfStabilizingTree self_type;
			typedef self_type* self_pointer_t;
				
			typedef OsModel_P OsModel;
			typedef Radio_P Radio;
			typedef Clock_P Clock;
			typedef Timer_P Timer;
			typedef NapControl_P NapControlT;
			typedef UserData_P UserData;
			typedef Debug_P Debug;
			
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			//typedef typename Radio::ExtendedData::
			typedef ::uint16_t link_metric_t;
			typedef typename Radio::node_id_t node_id_t;
			typedef typename Radio::message_id_t message_id_t;
			typedef typename Clock::time_t time_t;
			typedef ::uint32_t abs_millis_t;
			typedef TreeStateMessage<OsModel, Radio, UserData> TreeStateMessageT;
			typedef typename TreeStateMessageT::TreeStateT TreeStateT;
			typedef RegularEvent<OsModel, Radio, Clock, Timer> RegularEventT;
			
			enum State { IN_EDGE = 1, OUT_EDGE = 2, BIDI_EDGE = IN_EDGE | OUT_EDGE  };
			enum Timing {
				PUSH_INTERVAL = 500 * WISELIB_TIME_FACTOR, ///< limit bcasts to this interval when things change all the time
				BCAST_INTERVAL = INSE_BCAST_INTERVAL, ///< send regular bcasts in this interval
				BCAST_TIMES_OUT = false,
				BCAST_TIMEOUT = 1000 * WISELIB_TIME_FACTOR, ///< stay awake at most this long waiting for bcasts
				BCAST_KEEP_AWAKE = 200 * WISELIB_TIME_FACTOR, ///< stay awake at least this long waiting for bcasts (so others have a chance to contact us)
				DEAD_INTERVAL = (unsigned long)(3 * BCAST_INTERVAL), ///< consider neighbor bcasts as missed after this time
				MAX_ROOT_DISTANCE = 10,
			};
			enum SpecialNodeIds {
				NULL_NODE_ID = Radio::NULL_NODE_ID,
				BROADCAST_ADDRESS = Radio::BROADCAST_ADDRESS
			};
			enum { npos = (size_type)(-1) };
			enum Restrictions {
				MAX_NEIGHBORS = MAX_NEIGHBORS_P,
				MAX_EVENT_LISTENERS = MAX_EVENT_LISTENERS_P
			};
			enum EventType {
				SEEN_NEIGHBOR, NEW_NEIGHBOR, LOST_NEIGHBOR, UPDATED_NEIGHBOR, UPDATED_STATE
			};
			
			typedef delegate2<void, EventType, node_id_t> event_callback_t;
			typedef vector_static<OsModel, event_callback_t, MAX_EVENT_LISTENERS> EventCallbacks;
			typedef MapStaticVector<OsModel, node_id_t, RegularEventT, MAX_NEIGHBORS> RegularEvents;
			
			struct NeighborEntry {
				// {{{
				
				enum {
					ALPHA = 10,
					LINK_METRIC_BECOMES_STABLE = 240,
					LINK_METRIC_LOOSES_STABLE = 180,
					LINK_METRIC_BLAME = (int)(LINK_METRIC_LOOSES_STABLE * 0.9),
					LINK_METRIC_PRAISE = (int)(LINK_METRIC_BECOMES_STABLE * 1.1),
					LINK_METRIC_WORSHIP = (int)(LINK_METRIC_BECOMES_STABLE * 1.1)
				};
				
				NeighborEntry() : address_(NULL_NODE_ID), stable_(false), liked_(false) {
				}
				
				~NeighborEntry() {
					address_ = NULL_NODE_ID;
				}
				
				NeighborEntry(node_id_t addr)
					: address_(addr), stable_(false), liked_(false) {
				}
					
				NeighborEntry(node_id_t addr, const TreeStateMessageT& msg, abs_millis_t t, link_metric_t m)
					: message_(msg), last_update_(t), address_(addr), link_metric_(m), stable_(false), liked_(false) {
					stable_ = becomes_stable();
				}
				
				/*
				void from_message(node_id_t addr, const TreeStateMessageT& msg, abs_millis_t t, link_metric_t m) {
					message_ = msg;
					last_update_ = t;
					address_ = addr;
					link_metric_ = m;
					stable_ = becomes_stable();
					liked_ = false;
				}
				*/
				
				bool used() const { return address_ != NULL_NODE_ID; }
				TreeStateT tree_state() { return message_.tree_state(); }
				UserData& user_data() { return message_.user_data(); }
				node_id_t root() { return message_.tree_state().root(); }
				node_id_t parent() { return message_.tree_state().parent(); }
				node_id_t distance() { return message_.tree_state().distance(); }
				
				bool operator==(const NeighborEntry& other) {
					return address_ == other.address_;
				}
				
				bool same_content(TreeStateMessageT& msg) {
					bool r = //(address_ == other.address_) &&
						(message_.tree_state() == msg.tree_state()) &&
						(message_.user_data() ==  msg.user_data());
					return r;
				}
				
				node_id_t id() { return address_; }
				
			#if INSE_USE_LINK_METRIC
				bool becomes_stable() { return link_metric_ >= LINK_METRIC_BECOMES_STABLE; }
				bool looses_stable() { return link_metric_ <= LINK_METRIC_LOOSES_STABLE; }
				bool stable() {
					return stable_ || liked_;
				}
			#else
				bool becomes_stable() { return true; }
				bool looses_stable() { return false; }
				bool stable() { return true; }
			#endif
				
				void seen_link_metric(link_metric_t l, int alpha = NeighborEntry::ALPHA) {
				#if INSE_USE_LINK_METRIC
					
					link_metric_ = ((100 - alpha) * link_metric_ + alpha * l) / 100;
					if(stable_ && looses_stable()) { stable_ = false; }
					else if(!stable_ && becomes_stable()) { stable_ = true; }
				#endif
				}
				void set_link_metric(link_metric_t l) { link_metric_ = l; }
				link_metric_t link_metric() { return link_metric_; }
				
				void update(TreeStateMessageT& msg, abs_millis_t t) {
					message_ = msg;
					last_update_ = t;
				}
				
				// TODO: calling one of those should trigger notify_event when
				// appropriate!
				void like() { liked_ = true; }
				void clear_like() { liked_ = false; }
				bool liked() { return liked_; }
				
				TreeStateMessageT message_;
				abs_millis_t last_update_;
				node_id_t address_;
				
				link_metric_t link_metric_;
				bool stable_;
				bool liked_;
				// }}}
			};
			
			/**
			 * @ingroup Neighbor_concept
			 */
			class Neighbor {
				// {{{
				public:
					Neighbor() : entry_(0) {
					}
					
					Neighbor(NeighborEntry* p) : entry_(p) {
					}
					
					node_id_t id() const {
						if(!entry_) { return NULL_NODE_ID; }
						return entry_->address_;
					}
					State state() const { return state_; }
					UserData user_data() const {
						return entry_->message_.user_data();
					}
					
					void invalidate() {
						entry_ = 0;
					}
					
					State state_;
					NeighborEntry *entry_;
				// }}}
			};
			
			typedef set_static<OsModel, NeighborEntry, MAX_NEIGHBORS> NeighborEntries;
			typedef vector_static<OsModel, Neighbor, MAX_NEIGHBORS> Neighbors;
			
			typedef typename Neighbors::iterator iterator;
			
			SelfStabilizingTree() : radio_(0), clock_(0), timer_(0), debug_(0), nap_control_(0) {
			}
			
			void init(typename Radio::self_pointer_t radio, typename Clock::self_pointer_t clock, typename Timer::self_pointer_t timer, typename Debug::self_pointer_t debug, typename NapControlT::self_pointer_t nap_control) {
				radio_ = radio;
				radio_->template reg_recv_callback<self_type, &self_type::on_receive>(this);
				clock_ = clock;
				timer_ = timer;
				debug_ = debug;
				new_neighbors_ = false;
				lost_neighbors_ = false;
				updated_neighbors_ = false;
				nap_control_ = nap_control;
				
				clear_neighbors();
				
				last_push_ = 0;
				check();
				update_state();
				broadcast_state_regular();
				check();
			}
			
			void reg_event_callback(event_callback_t cb) {
				event_callbacks_.push_back(cb);
			}
			
			iterator begin_neighbors() {
				iterator r = neighbors_.begin();
				if(r != neighbors_.end() && r->id() == NULL_NODE_ID) {
					++r;
				}
				return r;
			}
			iterator end_neighbors() { return neighbors_.end(); }
			
			node_id_t parent() { return tree_state_.parent(); }
			node_id_t root() { return tree_state_.root(); }
			
			node_id_t child(size_type c_idx) {
				check();
				assert(c_idx < childs());
				return neighbors_[c_idx + 1].id();
			}
			
			UserData& child_user_data(size_type c_idx) {
				check();
				assert(c_idx < childs());
				return neighbors_[c_idx + 1].entry_->message_.user_data();
			}
			
			TreeStateT& tree_state() { return tree_state_; }
			
			/**
			 * @return The number of childs with a lower node id than n.
			 */
			size_type child_index(node_id_t n) {
				check();
				size_type idx = find_neighbor_position(n);
				return (neighbors_[idx].id() == n) ? idx - 1 : npos;
			}
			
			size_type childs() {
				return neighbors_.size() - 1;
			}
			
			size_type size() {
				return neighbors_.size();
			}
			
			UserData& user_data() {
				return user_data_;
			}
			
			void set_user_data(const UserData& ud) {
				user_data_ = ud;
			}
			
			void check() {
				#if !WISELIB_DISABLE_DEBUG
				/*
					assert(neighbors_.size() >= 1);
					
					typename Neighbors::iterator it = neighbors_.begin();
					++it;
					
					node_id_t prev = NULL_NODE_ID;
					for( ; it != neighbors_.end(); ++it) {
						assert(prev == NULL_NODE_ID || it->id() > prev);
						prev = it->id();
					}
				*/
				#endif
			}
			
			/**
			 * Reduce stability estimation for given neighbor.
			 * Call this when you expected a packet from the neighbor which
			 * hasnt arrived or you just dont like him and dont want to talk
			 * with him anymore.
			 */
			void blame(node_id_t addr) {
				typename NeighborEntries::iterator iter = find_neighbor_entry(addr);
				blame(iter);
			}
			void blame(typename NeighborEntries::iterator iter) {
				if(iter != neighbor_entries_.end()) {
					neighbor_sees_metric(iter, NeighborEntry::LINK_METRIC_BLAME);
				}
				update_state();
			}
			
			void praise(node_id_t addr) {
				typename NeighborEntries::iterator iter = find_neighbor_entry(addr);
				praise(iter);
			}
			void praise(typename NeighborEntries::iterator iter) {
				if(iter != neighbor_entries_.end()) {
					neighbor_sees_metric(iter, NeighborEntry::LINK_METRIC_PRAISE);
				}
				update_state();
			}
			
			void worship(node_id_t addr) {
				typename NeighborEntries::iterator iter = find_neighbor_entry(addr);
				worship(iter);
			}
			void worship(typename NeighborEntries::iterator iter) {
				if(iter != neighbor_entries_.end()) {
					neighbor_sees_metric(iter, NeighborEntry::LINK_METRIC_WORSHIP, 100);
				}
				update_state();
			}
		
		private:
			
			void clear_neighbors() {
				neighbors_.clear();
				neighbors_.push_back(Neighbor());
			}
			
			void broadcast_state(int reason) {
				TreeStateMessageT msg;
				msg.init();
				msg.set_reason(reason);
				msg.set_tree_state(tree_state_);
				msg.set_user_data(user_data());
				
				#if INSE_DEBUG_STATE || APP_BLINK
					debug_->debug("bc");
				#endif
				nap_control_->push_caffeine("bc");
				radio_->send(BROADCAST_ADDRESS, msg.size(), msg.data());
				//radio_->send(BROADCAST_ADDRESS, msg.size(), msg.data());
				//debug_->debug("T SS");
			#if !NAP_CONTROL_ALWAYS_ON
				timer_->template set_timer<self_type, &self_type::end_broadcast_state>(BCAST_KEEP_AWAKE, this, 0);
			#endif
				
			}
			
			void end_broadcast_state(void*) {
				nap_control_->pop_caffeine("/bc");
				#if INSE_DEBUG_STATE
					debug_->debug("/bc");
				#endif
			}
			
			void broadcast_state_regular(void* = 0) {
				broadcast_state(TreeStateMessageT::REASON_REGULAR_BCAST);
				last_push_ = now();
				//debug_->debug("T BSS");
				timer_->template set_timer<self_type, &self_type::broadcast_state_regular>(BCAST_INTERVAL, this, 0);
			}
			
			bool is_node_id_sane(node_id_t n) {
				return (n != NULL_NODE_ID) && (n != BROADCAST_ADDRESS) && (n != radio_->id());
			}
			
			void erase_regular_broadcast(node_id_t n) {
				//if(regular_broadcasts_.contains(n)) {
					//regular_broadcasts_[n].cancel();
					//regular_broadcasts_.erase(n);
				//}
			}
			
			typename NeighborEntries::iterator assess_link_metric(node_id_t from, TreeStateMessageT& msg, abs_millis_t t, link_metric_t m) {
				typename NeighborEntries::iterator ne =  find_neighbor_entry(from);
				if(ne != neighbor_entries_.end()) {
					return update_entry(ne, from, msg, t, m);
				}
				
				if(!neighbor_entries_.full()) {
					return create_entry(from, msg, t, m);
				}
				
				for(ne = neighbor_entries_.begin(); ne != neighbor_entries_.end(); ++ne) {
					if(!ne->stable() && (
								(ne->link_metric() < m) ||
								((ne->parent() != radio_->id()) && msg.tree_state().parent() == radio_->id()))
					) {
						erase_regular_broadcast(ne->address_);
						neighbor_entries_.erase(ne);
						return create_entry(from, msg, t, m);
					}
				}
				return neighbor_entries_.end();
			}
			
		#ifdef SHAWN
			void on_receive(node_id_t from, typename Radio::size_t len, block_data_t* data) {
				typename Radio::ExtendedData ex;
				ex.set_link_metric(NeighborEntry::LINK_METRIC_BECOMES_STABLE);
		#else
			void on_receive(node_id_t from, typename Radio::size_t len, block_data_t* data, const typename Radio::ExtendedData& ex) {
		#endif
				
				check();
				
				if(!is_node_id_sane(from)) {
					DBG("not sane: %lu", (unsigned long)from);
					return; }
				if(!nap_control_->on()) {
					DBG("not on!");
					return; }
				
				abs_millis_t t = now();
				
				message_id_t message_type = wiselib::read<OsModel, block_data_t, message_id_t>(data);
				if(message_type != TreeStateMessageT::MESSAGE_TYPE) {
					//GET_OS.debug("not treestate msg");
					return;
				}

				#ifdef ISENSE
					GET_OS.debug("bc %lu l%d t%lu m%d",
							(unsigned long)from, (int)len, (unsigned long)now(), (int)ex.link_metric());
				#endif
					
				assert((size_type)len >= (size_type)sizeof(TreeStateMessageT));
				TreeStateMessageT &msg = *reinterpret_cast<TreeStateMessageT*>(data);
				msg.check();
				
				if(msg.reason() == TreeStateMessageT::REASON_REGULAR_BCAST) {
					//GET_OS.debug("@%llx bc from %llx t %lu", (unsigned long long)radio_->id(),
							//(unsigned long long)from, (unsigned long)now());
					typename NeighborEntries::iterator ne = assess_link_metric(from, msg, t, ex.link_metric());
					if(ne != neighbor_entries_.end() && ne->stable()) {
						notify_event(SEEN_NEIGHBOR, from);
					}
				
				}
				
				update_state();
				
				check();
			}
			
			void begin_wait_for_regular_broadcast(void* ev) {
				#if INSE_DEBUG_STATE
					debug_->debug("bcwait");
				#endif
				nap_control_->push_caffeine("bcw");
				
				if(BCAST_TIMES_OUT) {
					//timer_->template set_timer<self_type, &self_type::give_up_wait_for_regular_broadcast>(
							//BCAST_TIMEOUT, this, ev);
				}
			}
			
			void end_wait_for_regular_broadcast(void* ev) {
				#if INSE_DEBUG_STATE
					debug_->debug("/bcwait");
				#endif
				nap_control_->pop_caffeine("/bcw");
			}
			
/*
			node_id_t find_event(RegularEventT& ev) {
				for(typename RegularEvents::iterator it = regular_broadcasts_.begin(); it != regular_broadcasts_.end(); ++it) {
					if(&(it->second) == &ev) {
						return it->first;
					}
				}
				return Radio::NULL_NODE_ID;
			}
*/
			
			void give_up_wait_for_regular_broadcast(void* v) {
/*
				RegularEventT& ev = *reinterpret_cast<RegularEventT*>(v);
				DBG("@%lu give up bc waiting %d t%lu a %lu ev %p", (unsigned long)radio_->id(), (int)ev.waiting(),
						(unsigned long)now(), (unsigned long)find_event(ev), v);
				
				// will call end_wait_for_regular_broadcast implicetely
				if(ev.waiting()) {
					ev.end_waiting();
					//debug_->debug("T GUWFRB");
					ev.template start_waiting_timer<
						self_type, &self_type::begin_wait_for_regular_broadcast,
						&self_type::end_wait_for_regular_broadcast>(clock_, timer_, this, v);
				}
				//nap_control_->pop_caffeine("/bcw");
*/
			}
			
			size_type find_neighbor_position(node_id_t a, bool allow_parent = true) {
				check();
				if(allow_parent && neighbors_[0].id() == a) {
					return 0;
				}
				
				if(childs() == 0) {
					return 1;
				}
				
				size_type l = 1;
				size_type r = 1 + childs();
				
				while(l + 1 < r) {
					size_type m = (l + r) / 2;
					if(neighbors_[m].id() == a) {
						assert(m > 0);
						assert(m < 1 + childs());
						return m;
					}
					else if(neighbors_[m].id() < a) { l = m; }
					else { r = m; }
				}
				if(neighbors_[l].id() >= a) {
					assert(l > 0);
					assert(l < 1 + childs());
					return l;
				}
				//if(neighbors_[r].address_ == a) { return r; }
				//return npos;
				assert(r > 0);
				assert(r < 2 + childs());
				return r;
			}
			
			void set_parent(Neighbor& n) {
				check();
				
				if(neighbors_[0].id() != NULL_NODE_ID) {
					insert_child(neighbors_[0]);
				}
				
				n.state_ = OUT_EDGE;
				neighbors_[0] = n;
				
				check();
			}
			
			
			typename NeighborEntries::iterator create_entry(node_id_t addr, TreeStateMessageT& msg, abs_millis_t t, link_metric_t m) {
				NeighborEntry e(addr, msg, t, m);
				if(msg.tree_state().parent() == radio_->id()) {
					e.like();
				}
				else {
					e.clear_like();
				}
				
				#if INSE_DEBUG_STATE
					debug_->debug("N+ %lu", (unsigned long)addr);
				#endif
				
				typename NeighborEntries::iterator r = neighbor_entries_.insert(e);
				if(e.stable()) {
					new_neighbors_ = true;
					
					notify_event(NEW_NEIGHBOR, addr);
		/*		
					RegularEventT &event = regular_broadcasts_[addr];
					event.hit(t, clock_, radio_->id());
					event.end_waiting();
			#if !NAP_CONTROL_ALWAYS_ON
				//debug_->debug("T CR");
					event.template start_waiting_timer<
						self_type, &self_type::begin_wait_for_regular_broadcast,
						&self_type::end_wait_for_regular_broadcast>(clock_, timer_, this, &event);
			#endif
					
					
					debug_->debug("@%lu bc %lu w%lu i%lu t%lu e%d",
							(unsigned long)radio_->id(),
							(unsigned long)addr,
							(unsigned long)event.window(),
							(unsigned long)event.interval(),
							(unsigned long)now(),
							(int)event.early());
				}
				return r;
			}
			
			void neighbor_sees_metric(typename NeighborEntries::iterator it, link_metric_t m, int alpha = NeighborEntry::ALPHA) {
				if(it == neighbor_entries_.end()) { return; }
				
				link_metric_t m_before = it->link_metric();
				
				bool stable_before = it->stable();
				it->seen_link_metric(m, alpha);
				bool stable_now = it->stable();
				
				if(stable_now > stable_before) {
					new_neighbors_ = true;
					notify_event(NEW_NEIGHBOR, it->address_);
				}
				else if(stable_now < stable_before) {
					lost_neighbors_ = true;
					notify_event(LOST_NEIGHBOR, it->address_);
					erase_regular_broadcast(it->address_);
				}
			}
			
			typename NeighborEntries::iterator update_entry(typename NeighborEntries::iterator ne, node_id_t addr, TreeStateMessageT& msg, abs_millis_t t, link_metric_t m) {
				assert(ne->address_ == addr);
				if(msg.tree_state().parent() == radio_->id()) {
					ne->like();
				}
				else {
					ne->clear_like();
				}
				
				neighbor_sees_metric(ne, m);
				bool stable_now = ne->stable();
				
				if(stable_now && !ne->same_content(msg)) {
					updated_neighbors_ = true;
					notify_event(UPDATED_NEIGHBOR, addr);
				}
				ne->update(msg, t);
				
				if(stable_now) {
					//RegularEventT &event = regular_broadcasts_[addr];
					//event.hit(t, clock_, radio_->id());
					//event.end_waiting();
					//event.template start_waiting_timer<
						//self_type, &self_type::begin_wait_for_regular_broadcast,
						//&self_type::end_wait_for_regular_broadcast>(clock_, timer_, this, &event);
				}
				return ne;
			}
			
			typename NeighborEntries::iterator find_neighbor_entry(node_id_t addr) {
				check();
				NeighborEntry e(addr);
				return neighbor_entries_.find(e);
			}
			
			void insert_child(Neighbor& n) {
				check();
				
				size_type pos = find_neighbor_position(n.id(), false);
				n.state_ = IN_EDGE;
				neighbors_.insert(typename Neighbors::iterator(&neighbors_[pos]), n);
				
				check();
			}
			
			void cleanup_dead_neighbors() {
				check();
				
				for(typename NeighborEntries::iterator iter = neighbor_entries_.begin(); iter != neighbor_entries_.end(); ++iter) {
					if(iter->last_update_ + DEAD_INTERVAL <= now()/* && iter->stable()*/) {
						iter->last_update_ = now();
						blame(iter);
						//neighbor_sees_metric(iter, 0);
					}
				}
				
				check();
			}
			
			void notify_event(EventType t, node_id_t addr) {
				check();
				for(typename EventCallbacks::iterator iter = event_callbacks_.begin(); iter != event_callbacks_.end(); ++iter) {
					(*iter)(t, addr);
				}
				check();
			}
			
			bool update_state() {
				check();
				
				cleanup_dead_neighbors();
				clear_neighbors();
				
				::uint8_t distance = -1;
				//node_id_t mindist_min_parent = NULL_NODE_ID;
				node_id_t previous_parent = tree_state().parent();
				node_id_t parent = radio_->id(); assert(parent != NULL_NODE_ID);
				node_id_t root = radio_->id(); assert(root != NULL_NODE_ID);
				NeighborEntry *parent_ptr = 0;
				
				//#ifdef ISENSE
				//GET_OS.debug("update state");
				//#endif
				
				for(typename NeighborEntries::iterator iter = neighbor_entries_.begin(); iter != neighbor_entries_.end(); ++iter) {
						
					if(iter->tree_state().root() == NULL_NODE_ID || (iter->tree_state().distance() + 1) == 0) { continue; }
					if(iter->tree_state().parent() == radio_->id()) {
						#ifdef SHAWN
							DBG("->child");
						#endif
						Neighbor n(&*iter);
						insert_child(n);
						continue;
					}
					
					if(!iter->stable()) {
						continue;
					}
					
					
					if(
							(
								/* neigh has a better root OR */
								(iter->tree_state().root() < root) || (
								
									/* neigh has the same root */
									(iter->tree_state().root() == root) && (
										
										// ...but is closer OR
										((iter->tree_state().distance() + 1) < distance) || (
										
											// ... is not closer, but has the
											// lowest id among equally close potential
											// parents
											((iter->tree_state().distance() + 1) == distance) && (
												(iter->id() < parent) || (parent == radio_->id())
											)
										)
									)
								)
							) &&
							((iter->tree_state().distance() + 1) <= MAX_ROOT_DISTANCE)
					) {
						parent_ptr = &*iter;
						parent = iter->id();
						root = iter->tree_state().root();
						distance = iter->tree_state().distance() + 1;
					}
				}
				
				if(root == radio_->id()) {
					distance = 0;
					parent_ptr = 0;
					parent = radio_->id();
				}
				
				if(parent_ptr) {
					Neighbor n(parent_ptr);
					set_parent(n);
				}
				
				bool c_a = tree_state().set_distance(distance);
				bool c_b = tree_state().set_parent(parent);
				bool c_c = tree_state().set_root(root);
				bool c = new_neighbors_ || lost_neighbors_ || c_a || c_b || c_c;
				
				if(c) {
					changed();
				}
				
				if(c || updated_neighbors_) {
					//#if (INSE_DEBUG_STATE || INSE_DEBUG_TOPOLOGY || INSE_DEBUG_TREE)
						//debug_->debug("@%lu p%lu d%d rt%lu c%d t%lu nn%d ln%d",
									//(unsigned long)radio_->id(), (unsigned long)parent, (int)distance, (unsigned long)root, (int)c, (unsigned long)now(), (int)new_neighbors_, (int)lost_neighbors_); //(int)(now() % 65536)[>, hex<]);
					//#endif
						
					notify_event(UPDATED_STATE, previous_parent);
						
					// <DEBUG>
					//#if INSE_DEBUG_TREE
						//char hex[sizeof(UserData) * 2 + 1];
						//for(size_type i = 0; i < sizeof(UserData); i++) {
							//hex[2 * i] = hexchar(((block_data_t*)&user_data_)[i] >> 4);
							//hex[2 * i + 1] = hexchar(((block_data_t*)&user_data_)[i] & 0x0f);
						//}
						//hex[sizeof(UserData) * 2] = '\0';
						
						////debug_->debug("node %d root %d parent %d distance %d filter %s",
								////(int)radio_->id(), (int)root, (int)parent, (int)distance, hex);
						
						////for(size_type i = 0; i < childs(); i++) {
							////debug_->debug("node %d child %d t %d // update_state", (int)radio_->id(), (int)child(i), (int)now());
						////}
						
					//#endif
					// </DEBUG>
					
				}
				
				new_neighbors_ = false;
				lost_neighbors_ = false;
				updated_neighbors_ = false;
				
				check();
				return c;
			}
			
			void print_filter(const UserData& ud) {
				char hex[sizeof(UserData) * 2 + 1];
				for(size_type i = 0; i < sizeof(UserData); i++) {
					hex[2 * i] = hexchar(((block_data_t*)&user_data_)[i] >> 4);
					hex[2 * i + 1] = hexchar(((block_data_t*)&user_data_)[i] & 0x0f);
				}
				hex[sizeof(UserData) * 2] = '\0';
				debug_->debug(hex);
			}
			
			abs_millis_t absolute_millis(const time_t& t) {
				return clock_->seconds(t) * 1000 + clock_->milliseconds(t);
			}
			
			abs_millis_t now() {
				return absolute_millis(clock_->time());
			}
			
			void changed(void* p = 0) {
				if(p) {
					// we have been called by timer
					timer_pending_ = false;
				}
				abs_millis_t n = now() + (abs_millis_t)(0.1 * PUSH_INTERVAL);
				if(nap_control_->on() && last_push_ + PUSH_INTERVAL <= n) {
					broadcast_state(TreeStateMessageT::REASON_PUSH_BCAST);
					last_push_ = now();
				}
				else if(!timer_pending_) {
					timer_pending_ = true;
				//debug_->debug("T PUSH");
					timer_->template set_timer<self_type, &self_type::changed>(PUSH_INTERVAL, this, (void*)(::uint8_t*)1);
				}
			}
				
				
			typename Radio::self_pointer_t radio_;
			typename Clock::self_pointer_t clock_;
			typename Timer::self_pointer_t timer_;
			typename Debug::self_pointer_t debug_;
			typename NapControlT::self_pointer_t nap_control_;
			//TreeStateMessageT tree_state_message_;
			TreeStateT tree_state_;
			UserData user_data_;
			abs_millis_t last_push_;
			
			Neighbors neighbors_;
			
			NeighborEntries neighbor_entries_;
			RegularEvents regular_broadcasts_;
			
			
			EventCallbacks event_callbacks_;
			
			::uint8_t new_neighbors_ : 1;
			::uint8_t lost_neighbors_ : 1;
			::uint8_t updated_neighbors_ : 1;
			::uint8_t timer_pending_ : 1;
		
	}; // SelfStabilizingTree
}

#endif // SELF_STABILIZING_TREE_H

