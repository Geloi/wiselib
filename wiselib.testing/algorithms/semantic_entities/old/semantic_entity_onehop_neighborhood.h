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

#ifndef SEMANTIC_ENTITY_ONEHOP_NEIGHBORHOOD_H
#define SEMANTIC_ENTITY_ONEHOP_NEIGHBORHOOD_H

//#include <util/pstl/list_dynamic.h>
#include <util/pstl/map_static_vector.h>
#include "semantic_entity_id.h"
#include "semantic_entity.h"
#include <util/pstl/highscore_set.h>

#ifndef INSE_ROOT_NODE_ID
	#define INSE_ROOT_NODE_ID 0
#endif

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
		typename Radio_P,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock
	>
	class SemanticEntityOnehopNeighborhood {
		public:
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef Radio_P Radio;
			typedef typename Radio::node_id_t node_id_t;
			typedef Debug_P Debug;
			typedef Clock_P Clock;
			typedef typename Clock::time_t time_t;
			
			typedef SemanticEntity<OsModel> SemanticEntityT;
			typedef ::uint32_t abs_millis_t;
			typedef ::uint16_t link_metric_t;
			
			enum { npos = (size_type)(-1) };
			
			enum SpecialNodeIds {
				NULL_NODE_ID = Radio::NULL_NODE_ID,
				BROADCAST_ADDRESS = Radio::BROADCAST_ADDRESS,
				ROOT_NODE_ID = INSE_ROOT_NODE_ID
			};
			
			enum Restrictions {
				MAX_NEIGHBORS = INSE_MAX_NEIGHBORS,
				MAX_CHILDS = MAX_NEIGHBORS - 1,
				MAX_SEMANTIC_ENTITIES = INSE_MAX_SEMANTIC_ENTITIES
			};
			
			enum MessageOrientation {
				UP, DOWN
			};
			
			enum SemanticEntityState {
				UNAFFECTED = SemanticEntityT::UNAFFECTED,
				JOINED = SemanticEntityT::JOINED,
				ADOPTED = SemanticEntityT::ADOPTED
			};
			
			enum NeighborClass {
				CLASS_UNKNOWN = 0,
				CLASS_CHILD = 1,
				CLASS_PARENT = 2,
				CLASS_SHORTCUT = 3
			};
			
		#if defined(SHAWN)
			enum {
				LM_THRESHOLD_LOW = 0,
				LM_THRESHOLD_HIGH = 0
			};
		#else // Contiki/Sky
			enum {
				LM_THRESHOLD_LOW = 180,
				LM_THRESHOLD_HIGH = 240
			};
		#endif
			
			class NeighborEntity {
				// {{{
				public:
					NeighborEntity() :
						semantic_entity_id_(),
						semantic_entity_state_(UNAFFECTED),
						fresh_(true)
						//token_count_(0),
						//prev_token_count_(0)
					{
					}
					
					NeighborEntity(SemanticEntityId id, SemanticEntityState state) :
						semantic_entity_id_(id),
						semantic_entity_state_(state),
						fresh_(true)
						//token_count_(0),
						//prev_token_count_(0)
					{
					}
					
					bool operator<(NeighborEntity& other) { return semantic_entity_id_ < other.semantic_entity_id_; }
					
					SemanticEntityId semantic_entity_id() { return semantic_entity_id_; }
					void set_semantic_entity_id(SemanticEntityId x) { semantic_entity_id_ = x; }
					
					::uint8_t semantic_entity_state() { return semantic_entity_state_; }
					void set_semantic_entity_state(::uint8_t x) { semantic_entity_state_ = x; }
					
					void refresh() { fresh_ = true; }
					void outdate() { fresh_ = false; }
					bool fresh() { return fresh_; }
					
					//::uint8_t token_count() { return token_count_; }
					//void set_token_count(::uint8_t x) { token_count_ = x; }
					
					//::uint8_t prev_token_count() { return prev_token_count_; }
					//void set_prev_token_count(::uint8_t x) { prev_token_count_ = x; }
					
					// TODO
				private:
					SemanticEntityId semantic_entity_id_;
					::uint8_t semantic_entity_state_ : 2;
					::uint8_t fresh_ : 1;
					//::uint8_t token_count_;
					//::uint8_t prev_token_count_;
				// }}}
			};
			
			class Neighbor {
				// {{{
					//typedef list_dynamic<OsModel, NeighborEntity> Entities;
					typedef vector_static<OsModel, NeighborEntity, MAX_SEMANTIC_ENTITIES> Entities;
				
				public:
					Neighbor() : id_(NULL_NODE_ID) {
					}
					
					Neighbor(node_id_t id) : id_(id) {
					}
					
					void init(node_id_t id) {
						id_ = id;
					}
					
					node_id_t id() { return id_; }
					void set_id(node_id_t x) { id_ = x; }
					
					//abs_millis_t last_beacon_received() { return last_beacon_received_; }
					//void set_last_beacon_received(abs_millis_t x) { last_beacon_received_ = x; }

					abs_millis_t last_confirmed_ti_start() { return last_confirmed_ti_start_; }
					void set_last_confirmed_ti_start(abs_millis_t x) { last_confirmed_ti_start_ = x; }
					
					node_id_t parent() { return parent_; }
					void set_parent(node_id_t p) { parent_ = p; }
					
					::uint8_t root_distance() { return root_distance_; }
					void set_root_distance(::uint8_t d) { root_distance_ = d; }
					
					bool is_valid() { return id_ != NULL_NODE_ID; }
					
					bool operator<(Neighbor& other) { return id_ < other.id_; }
					
					::uint8_t semantic_entity_state(SemanticEntityId id) {
						if(id.is_all()) {
							return JOINED;
						}

						for(typename Entities::iterator iter = entities_.begin(); iter != entities_.end(); ++iter) {
							if(iter->semantic_entity_id() == id) {
								return iter->semantic_entity_state();
							}
						}
						return UNAFFECTED;
					}
					
					NeighborEntity& find_or_create_semantic_entity(SemanticEntityId id) {
				//printf("[foc_se]\n");
						for(typename Entities::iterator iter = entities_.begin(); iter != entities_.end(); ++iter) {
							if(iter->semantic_entity_id() == id) {
								return *iter;
							}
						}
				//printf("[foc_se c S%lx.%lx]\n", (unsigned long)id.rule(), (unsigned long)id.value());
						NeighborEntity ne(id, UNAFFECTED);
						typename Entities::iterator it = entities_.insert(ne);
						return *it;
					}
					
					void outdate_semantic_entities() {
				//printf("[outdate_ses]\n");
						for(typename Entities::iterator iter = entities_.begin(); iter != entities_.end(); ++iter) {
							iter->outdate();
						}
					}
					
					void erase_outdated_semantic_entities() {
				//printf("[erase_oses]\n");
						for(typename Entities::iterator iter = entities_.begin(); iter != entities_.end(); ) {
							if(iter->fresh()) { ++iter; }
							else { iter = entities_.erase(iter); }
						}
					}
					
					link_metric_t score() { return link_metric_; }
					
					link_metric_t link_metric() { return link_metric_; }
					void set_link_metric(link_metric_t x) { link_metric_ = x; }
					
					void update_link_metric(link_metric_t x) {
						link_metric_ = (1.0 - INSE_LINK_METRIC_ALPHA) * (float)link_metric_ + INSE_LINK_METRIC_ALPHA * (float)x;
					}
					
					
				private:
					//list_dynamic<OsModel, NeighborEntity> entities_;
					Entities entities_;
					node_id_t id_;
					//abs_millis_t last_beacon_received_;
					abs_millis_t last_confirmed_ti_start_;
					node_id_t parent_;
					link_metric_t link_metric_;
					::uint8_t root_distance_;
				// }}}
			};
			
			//typedef vector_static<OsModel, Neighbor, MAX_NEIGHBORS> Neighbors;
			//typedef MapStaticVector<OsModel, node_id_t, Neighbor, MAX_NEIGHBORS> Neighbors;
			typedef HighscoreSet<OsModel, Neighbor, link_metric_t, MAX_NEIGHBORS> Neighbors;
			typedef typename Neighbors::iterator iterator;
			
			typedef MapStaticVector<OsModel, SemanticEntityId, SemanticEntityT, MAX_SEMANTIC_ENTITIES> SemanticEntities;
			typedef typename SemanticEntities::iterator semantic_entity_iterator;
			
			SemanticEntityOnehopNeighborhood() : radio_(0), parent_(NULL_NODE_ID) {
			}
			
			void init(typename Radio::self_pointer_t radio, typename Debug::self_pointer_t debug, typename Clock::self_pointer_t clock) {
				radio_ = radio;
				debug_ = debug;
				clock_ = clock;
				parent_ = NULL_NODE_ID;
				
				check();
			}
			
			
		///@{
		///@name Entity properties
			
			bool is_joined(SemanticEntityId id) {
				return semantic_entities_.contains(id) && semantic_entities_[id].is_joined();
			}
			
			bool is_in_subtree(SemanticEntityId id) {
				return first_child(id) != NULL_NODE_ID;
			}
			
			bool is_adopted(SemanticEntityId id) {
				return is_in_subtree(id) && !is_joined(id);
			}
			
		///@}
		
		///@{
		///@name This nodes routing properties
			
			bool is_leaf(SemanticEntityId id) {
				for(iterator iter = begin(); iter != end(); ++iter) {
					if(classify(iter) == CLASS_CHILD && iter->semantic_entity_state(id) != UNAFFECTED) {
						//debug_->debug("@%lu no leaf because of %lu", (unsigned long)radio_->id(), (unsigned long)iter->id());
						return false;
					}
				}
				return true;
			}
			
			bool is_root() { return radio_->id() == ROOT_NODE_ID; }
			bool is_connected() { return is_root() || (parent_ != NULL_NODE_ID); }
			
			::uint8_t root_distance() {
				if(is_root()) { return 0; }
				if(!is_connected()) { return -1; }
				return parent().root_distance() + 1;
			}
			
			node_id_t parent_id() { return parent_; }
			void set_parent_id(node_id_t x) { parent_ = x; }
			
			Neighbor& parent() { return *find_neighbor(parent_id()); }
			
		///@}
		
		///@{
		///@name Neighbors
			
			iterator begin() { return neighbors_.begin(); }
			iterator end() { return neighbors_.end(); }
			
			iterator find_neighbor(node_id_t id) {
				//debug_->debug("[findn]");
				for(iterator iter = begin(); iter != end(); ++iter) {
					if(iter->id() == id) { return iter; }
				}
				return end();
			}
			
			iterator create_or_update_neighbor(node_id_t id, link_metric_t lm) {
				//debug_->debug("[corun]");
				iterator r = find_neighbor(id);
				if(r == end()) {
					if(lm >= LM_THRESHOLD_HIGH) {
						//debug_->debug("@%lu N+ %lu l%lu t%lu", (unsigned long)radio_->id(), (unsigned long)id, (unsigned long)lm, (unsigned long)now());
						//debug_->debug("N+ %lu l%lu", (unsigned long)id, (unsigned long)lm);
						r = neighbors_.insert(id, lm);
						r->set_link_metric(lm);
						return r;
					}
					return end();
				}
				else {
					r->update_link_metric(lm);
					if(r->link_metric() < LM_THRESHOLD_LOW) {
						//debug_->debug("@%lu N- %lu l%lu L%d t%lu", (unsigned long)radio_->id(), (unsigned long)id, (unsigned long)lm, (unsigned long)r->link_metric(), (unsigned long)now());
						//debug_->debug("N- %lu l%lu L%lu", (unsigned long)id, (unsigned long)lm, (unsigned long)r->link_metric()); 
						neighbors_.erase(r);
						return end();
					}
					
					//debug_->debug("@%lu N= %lu l%lu L%lu t%lu", (unsigned long)radio_->id(), (unsigned long)id, (unsigned long)lm, (unsigned long)r->link_metric(), (unsigned long)now());
					//debug_->debug("N= %lu l%lu L%lu", (unsigned long)id, (unsigned long)lm, (unsigned long)r->link_metric()); 
					return r;
				}
			}
				

			int classify(iterator iter) {
				if(iter == end()) { return CLASS_UNKNOWN; }
				
				node_id_t n = iter->id();
				if(n == NULL_NODE_ID || n == BROADCAST_ADDRESS) { return CLASS_UNKNOWN; }
				if(!is_root() && n == parent_) { return CLASS_PARENT; }
				
				return iter->parent() == radio_->id() ? CLASS_CHILD : CLASS_SHORTCUT;
			}

			
			int classify(node_id_t n) {
				if(n == NULL_NODE_ID || n == BROADCAST_ADDRESS) { return CLASS_UNKNOWN; }
				if(!is_root() && n == parent_) { return CLASS_PARENT; }
				
				iterator iter = find_neighbor(n);
				if(iter == end()) { return CLASS_UNKNOWN; }
				
				return iter->parent() == radio_->id() ? CLASS_CHILD : CLASS_SHORTCUT;
			}

			/*
			bool is_child(node_id_t n) {
				if(n == parent_ || n == NULL_NODE_ID || n == BROADCAST_ADDRESS) { return false; }
				
				for(typename Neighbors::iterator iter = neighbors_.begin(); iter != neighbors_.end(); ++iter) {
					if(n == iter->id()) { return true; }
				}
				return false;
			}
			*/
			
			
		///@}
		
		///@{
		///@name Semantic Entities
			
			semantic_entity_iterator begin_semantic_entities() { return semantic_entities_.begin(); }
			semantic_entity_iterator end_semantic_entities() { return semantic_entities_.end(); }
			
			void add_semantic_entity(SemanticEntityId id) {
				check();
				
				SemanticEntityT &se = semantic_entities_[id];
				se.set_id(id);
				//se.set_source(radio_->id());
				se.set_state(SemanticEntityT::JOINED);
				
				check();
				assert(semantic_entities_.contains(id));
			}
			
			SemanticEntityT& get_semantic_entity(SemanticEntityId se_id) { return semantic_entities_[se_id]; }
			
		///@}
			
			void process_token(SemanticEntityId se_id, node_id_t source, ::uint8_t token_count) {
				//debug_->debug("[proct]");
				SemanticEntityT &se = semantic_entities_[se_id];
				
				debug_->debug("PT F%lu C%d c%d:%d,%d",
					(unsigned long)source,
					(int)classify(source),
					(int)token_count,
					(int)se.prev_token_count(),
					(int)se.token_count()
				);
				/*
				debug_->debug("@%lu PT F%lu C%d c%d:%d,%d",
					(unsigned long)radio_->id(),
					(unsigned long)source,
					(int)classify(source),
					(int)token_count,
					(int)se.prev_token_count(),
					(int)se.token_count()
				);
				*/
				
				switch(classify(source)) {
					case CLASS_PARENT: {
						// if we get the token from our parent, we consider it
						// activating iff the conditions below are met.
						// 
						// We will then set se.prev_token_count :=
						// parent.token_count, handle the complete subtree in that
						// state (with orientation == DOWN), and finally set
						// se.token_count := se.prev_token_count when the subtree
						// has been handled completely.
						
						// We got it from our parent, ergo we can't be root (or
						// something must be fishy)
						
						// If the system is stable, one of the following should
						// hold:
						//   (a) c == se.prev_token_count == se.token_count, i.e.
						//       there is no activity around here right now,
						//       nothing to be done in this case.
						//       
						//   (b) c != se.prev_token_count == se.token_count, i.e. our parent sends
						//       us a new (increased) token value so we shall
						//       be active and then activate our subtree
						//       
						// If the system is not yet stablilized, it might also be
						// that
						//   (c) c != se.prev_token_count, c == se.token_count
						//
						//   In this case we should just set
						//   s.prev_token_count := c, as its the turn of an
						//   upwards successor
						
						
						assert(!is_root());
						assert(source == parent_);

						//if(token_count != se.prev_token_count() && token_count == se.token_count()) {
							//se.set_prev_token_count(se.token_count());
							//se.set_source(radio_->id());
							//se.set_orientation(SemanticEntityT::UP);
							//se.set_activity_rounds(0);
						//}
						if(token_count != se.prev_token_count()) {
							int activity = is_leaf(se_id) ? 2 : 1;
							
							//debug_->debug("@%lu tok v F%lu c%d,%d a%d t%lu",
									//(unsigned long)radio_->id(),
									//(unsigned long)source,
									//(int)token_count,
									//(int)se.token_count(),
									//(int)activity,
									//(unsigned long)now()
							//);
							debug_->debug("tok v F%lu c%d:%d,%d a%d",
									(unsigned long)source,
									(int)token_count,
									(int)se.prev_token_count(),
									(int)se.token_count(),
									(int)activity
							);
							//debug_->debug("tok v F%lu c%d,%d a%d",
									//(unsigned long)source,
									//(int)token_count,
									//(int)se.token_count(),
									//(int)activity
							//);
							//debug_->debug("tok v F%lu c%d,%d a%d",
									//(unsigned long)source,
									//(int)token_count,
									//(int)se.token_count(),
									//(int)activity
							//);
							
							//se.set_source(source);
							//se.set_orientation(SemanticEntityT::DOWN);
							se.set_prev_token_count(token_count);
							// Token is activating
							se.set_activity_rounds(activity);
						}
						break;
					}
					
					case CLASS_CHILD: {
						assert(source != parent_);
						assert(!is_leaf());
						
						// The last child of the subtree sends us a token count c.
						// If the system is stable, one of the following should
						// hold:
						//   (a) c == se.prev_token_count == se.token_count, i.e.
						//       there is no activity around here right now,
						//       nothing to be done in this case.
						//       
						//   (b) c == se.prev_token_count != se.token_count, i.e.
						//       the token is arriving back from the subtree.
						//       We should be active for one round and set
						//       se.token_count := c to finalize our subtree wrt
						//       to scheduling this epoch.
						//       
						// If the system is not yet stablilized, it might also be
						// that
						//   (c) c != se.prev_token_count
						//       
						//       This means, we have gotten an update from our
						//       logical (parent-side) predecessor while
						//       circulating the token in our subtree.
						//       Thus ignore the subtree value (which is c)
						//       and care for distributing the most recent
						//       update from parent side!
						//
						//       (That is, just ignore this token count!)
						

						// Note: I know this if-tree does some unnecssary
						// evaluations which might not even get completely
						// optimized out, for now I want to leave it like this
						// for clarity. Once I'm confident it does the right
						// thing, I might simplify / speedup!

						// case (c)
						if(!is_root() && token_count != se.prev_token_count() && se.prev_token_count() != se.token_count()) {

							// We could pass this on, however prev_token_count
							// is still something else then we just received
							// so we are currently
							// inducing activity in our subtree (prev tc != tc)
						}

						// case (c) for root node
						//else if(is_root() && token_count != se.token_count() && token_count != se.prev_token_count()) {
							//// same here
						//}

						else if(!is_root() && token_count != se.token_count()) {
							//debug_->debug("@%lu tok ^ F%lu c%d,%d a1 t%lu",
									//(unsigned long)radio_->id(),
									//(unsigned long)source,
									//(int)se.prev_token_count(),
									//(int)token_count,
									//(unsigned long)now()
							//);
							debug_->debug("tok ^ F%lu c%d:%d,%d a1",
									(unsigned long)source,
									(int)token_count,
									(int)se.prev_token_count(),
									(int)se.token_count()
							);
							
							//se.set_source(source);
							//se.set_orientation(SemanticEntityT::UP);
							se.set_token_count(token_count);
							//se.set_token_count(token_count);
							// We just received a token from a child, so we can't
							// be a leaf!
							se.set_activity_rounds(1);

							debug_->debug("tok2 ^ F%lu c%d:%d,%d a1",
									(unsigned long)source,
									(int)token_count,
									(int)se.prev_token_count(),
									(int)se.token_count()
							);
						}
						else if(is_root() && token_count == se.token_count()) {
							//debug_->debug("@%lu tok ^ F%lu c%d,%d a0 t%lu R",
									//(unsigned long)radio_->id(),
									//(unsigned long)source,
									//(int)se.prev_token_count(),
									//(int)token_count,
									//(unsigned long)now()
							//);
							debug_->debug("tok < F%lu c:%d:%d,%d a0",
									(unsigned long)source,
									(int)token_count,
									(int)se.prev_token_count(),
									(int)se.token_count()
							);
							
							//se.set_source(source);
							//se.set_orientation(SemanticEntityT::UP);
							se.set_token_count(token_count + 1);
							se.set_activity_rounds(0);
						}
						break;
					} // CLASS_CHILD
					
					case CLASS_UNKNOWN:
					case CLASS_SHORTCUT:
					default:
						break;
				}
				
				/*
				debug_->debug("@%lu /process_token src %lu cls %d c' %d c%d,%d",
					(unsigned long)radio_->id(),
					(unsigned long)source,
					(int)classify(source),
					(int)token_count,
					(int)se.prev_token_count(),
					(int)se.token_count()
				);
				*/
			} // process_token
			
			void update_tree_state() {
				//debug_->debug("[uptree]");
				check();
				
				if(is_root()) {
					parent_ = NULL_NODE_ID;
					changed_parent_ = false;
					
					debug_topo();
					check();
					return;
				}
				
				// 
				// Determine parent
				// 
				
				::uint8_t min_dist = -1;
				node_id_t parent = NULL_NODE_ID;
				
				for(typename Neighbors::iterator iter = neighbors_.begin(); iter != neighbors_.end(); ++iter) {
					node_id_t addr = iter->id();
					Neighbor& neigh = *iter;
					
					if( (neigh.root_distance() != (::uint8_t)(-1)) && (
							(neigh.root_distance() < min_dist) ||
							(neigh.root_distance() == min_dist && addr < parent)) ) {
						
						min_dist = neigh.root_distance();
						parent = addr;
					}
				}
				
				changed_parent_ = (parent != parent_);
				parent_ = parent;
				
				if(changed_parent_) {
					debug_->debug("@%lu par%lu t%lu", (unsigned long)radio_->id(), (unsigned long)parent_, (unsigned long)now());
				}

				debug_topo();

				check();
			}

			void debug_topo() {
				/*
				debug_->debug("topo<%lu:", (unsigned long)parent_);
				//debug_->debug("ch<");
				
				node_id_t addr = 0;
				int i = 0;
				while(true) {
					addr = next_child(SemanticEntityId::all(), addr);
					if(addr == NULL_NODE_ID) { break; }
					debug_->debug("ch%d %lu:", (int)i, (unsigned long)addr);
					i++;
				}

				debug_->debug("topo>%d:", (int)i);
				*/
			}
			
			bool changed_parent() { return changed_parent_; }
			
			
			/**
			 * Return the maximum number of hops a message that will be
			 * routed through us from a member of a SE to its successor
			 * somewhere else in our subtree.
			 */
			::uint8_t max_hops_for_adopted() {
				// TODO
				return 2;
			}

			::uint8_t orientation(SemanticEntityT& se) {
				if(is_root()) { return DOWN; }
				return (se.prev_token_count() == se.token_count()) ? UP : DOWN;
			}
			
			node_id_t next_hop(SemanticEntityId id, node_id_t src = NULL_NODE_ID) { //, node_id_t source, MessageOrientation orientation) {
				//debug_->debug("[nextho]");
				assert(semantic_entities_.contains(id));
				check();
				
				// Orientation is only relevant if the token comes from
				// ourselves (source == radio_->id())
				// and tells us whether it should travel upwards or downwards
				// now.
				//::uint8_t orientation = semantic_entities_[id].orientation();
				SemanticEntityT &s = semantic_entities_[id];
				node_id_t source = (src != NULL_NODE_ID) ? src : radio_->id(); //s.source();
				
				debug_->debug("NH src %lu",  (unsigned long)source);
				
				
				if(is_root()) {
					if(classify(source) != CLASS_CHILD && source != radio_->id()) {
						//debug_->debug("@%lu next_hop source %lu not child but %d", (unsigned long)radio_->id(), (unsigned long)source, classify(source));
						return NULL_NODE_ID;
					}
					
					// if source == radio_->id() (initial case), this will try
					// to search the next child to radio_->id().
					// Since the root id is 0, this will be the first child.
					
					if(source == radio_->id()) {
						return first_child(id);
					}
					
					node_id_t nxt = next_child(id, source);
					if(nxt == NULL_NODE_ID) {
						//debug_->debug("@%lu next_hop last child", (unsigned long)radio_->id());
						return first_child(id);
					}
					return nxt;
				}
				
				
				if(source == radio_->id()) {
					if(s.activity_rounds()) {
						// While active for the token,
						// send it it to ourselves
						return radio_->id();
					}
					//debug_->debug("@%lu next_hop: from ourselves o=%d in_subtree=%d", (unsigned long)radio_->id(), (int)orientation, (int)is_in_subtree(id));
					if(orientation(s) == UP) { return parent_id(); }
					return is_in_subtree(id) ? first_child(id) : parent_id();
				}
				else {
					if(source == parent_id()) { return radio_->id(); }
					
					//debug_->debug("@%lu next_hop: not parent", (unsigned long)radio_->id());
					
					if(classify(source) == CLASS_CHILD) {
						node_id_t nxt = next_child(id, source);
						// source was our last child, its for us
						if(nxt == NULL_NODE_ID) { return radio_->id(); }
						return nxt;
					}
					//debug_->debug("@%lu next_hop: not child", (unsigned long)radio_->id());
				}
				
				check();
				return NULL_NODE_ID;
			} // next_hop()
			
			
			/**
			 * If there is at least one entity that has an activity round
			 * scheduled, return true and decrease all the activity round
			 * schedules on SEs by one (where they are > 0).
			 * Else, return false.
			 */
			bool be_active() {
				//debug_->debug("[beact]");
				
				// TODO
				bool r = false;
				for(typename SemanticEntities::iterator iter = semantic_entities_.begin(); iter != semantic_entities_.end(); ++iter) {
					SemanticEntityT &se = iter->second;
					if(se.activity_rounds()) {
						//debug_->debug("@%lu be_active ar %d leaf %d up %d rt %d prevtc %d",
								//(unsigned long)radio_->id(),
								//(int)se.activity_rounds(),
								//(int)is_leaf(se.id()),
								//(int)(se.orientation() == SemanticEntityT::UP),
								//(int)is_root(),
								//(int)se.prev_token_count()
						//);
						debug_->debug("ACT a%d rt%d c%d,%d l%d",
								(int)se.activity_rounds(),
								(int)is_root(),
								(int)se.prev_token_count(),
								(int)se.token_count(),
								(int)is_leaf(se.id())
						);
						
						r = true;
						se.set_activity_rounds(se.activity_rounds() - 1);
						if(se.activity_rounds() == 0) {
							//se.set_source(radio_->id());
							if(is_leaf(se.id())) {
								se.set_token_count(se.prev_token_count() + (is_root() ? 1 : 0));
							}
						} // if done
					} // if active


					else {
						// We might become a leaf suddenly due to topology
						// changes. In that case stop waiting for childs
						// (which is denoted by prev_token_count !=
						// token_count)!

						if(is_leaf(se.id()) && !is_root()) {
							se.set_token_count(se.prev_token_count());
						}
					}

					//else {
						//// Ensure consistency, when prevcount == count, send
						//// up, else down

						//if(is_leaf(se.id()) || is_root() || se.token_count() == se.prev_token_count()) {
							//se.set_orientation(SemanticEntityT::UP);
							//if(is_root()) {
								//se.set_token_count(se.prev_token_count() + 1);
							//}
						//}
						//else {
							//se.set_orientation(SemanticEntityT::DOWN);
						//}
						//se.set_source(radio_->id());
					//}

						debug_->debug("act a%d rt%d c%d,%d",
								(int)se.activity_rounds(),
								(int)is_root(),
								(int)se.prev_token_count(),
								(int)se.token_count()
						);

				} // for SEs
				return r;
			}
			
			/**
			 * Return true iff this node shall produce a new token count,
			 * after being active for the given se.
			 * This is not always true when we received an activating token.
			 * Eg. when receiving it from our parent we communicate the
			 * prospective now token state to our childs and only change it
			 * after all childs have been processed.
			 */
			bool owns_token(SemanticEntityId se_id) {
				return is_root() || is_leaf(se_id);
			}
		
			///@{
			///@name Child operations.
			
			/**
			 * Will not report childs that have ID 0! 
			 * Conveniently fixed by
			 * defining 0 as the root or disallowing the ID 0 in the network!
			 * In real networks usually the latter is true, in eg. shawn just
			 * choose ID 0 as root.
			 */
			node_id_t first_child(SemanticEntityId id) { return next_child(id, 0); }
			
			node_id_t next_child(SemanticEntityId id, node_id_t n) {
				//debug_->debug("[nextch]");
				//debug_->debug("@%lu next_child(%lx.%lx, %lu)", (unsigned long)radio_->id(),
						//(unsigned long)id.rule(), (unsigned long)id.value(), (unsigned long)n);
				
				node_id_t m = NULL_NODE_ID;
				
				for(typename Neighbors::iterator iter = neighbors_.begin(); iter != neighbors_.end(); ++iter) {
					node_id_t addr = iter->id();
					
					/*
					debug_->debug("@%lu next_child %lu m%lu n%lu p%lu s%d %d%d%d%d",
							(unsigned long)radio_->id(),
							(unsigned long)addr,
							(unsigned long)m,
							(unsigned long)n,
							(unsigned long)parent_,
							(int)iter->semantic_entity_state(id),
							(int)(addr > n),
							(int)(addr < m),
							(int)(addr != parent_),
							(int)(iter->semantic_entity_state(id) != UNAFFECTED)
							);
					*/

					bool x = (addr > n) &&
						(m == NULL_NODE_ID || addr < m) &&
						(classify(iter) == CLASS_CHILD) &&
						iter->semantic_entity_state(id) != UNAFFECTED;
					
					if(x) {
						m = addr;
					}
				}
				return m;
			}
			
			///@}
			
		private:
			
			void check() {
#if defined(SHAWN)
				assert(radio_ != 0);
				assert(!is_root() || (parent_ == NULL_NODE_ID));
				
				// There should be no SE in the SE container marked as
				// "UNAFFECTED" (as it wouldnt make sense to keep track of
				// those, so this signals some kind of error).
				
				for(typename SemanticEntities::iterator iter = semantic_entities_.begin(); iter != semantic_entities_.end(); ++iter) {
					assert(iter->second.state() != SemanticEntityT::UNAFFECTED);
				}
#endif
			}
			
			abs_millis_t absolute_millis(const time_t& t) { check(); return clock_->seconds(t) * 1000 + clock_->milliseconds(t); }
			abs_millis_t now() { check(); return absolute_millis(clock_->time()); }
			
			Neighbors neighbors_;
			SemanticEntities semantic_entities_;
			
			typename Radio::self_pointer_t radio_;
			typename Debug::self_pointer_t debug_;
			//typename Timer::self_pointer_t timer_;
			typename Clock::self_pointer_t clock_;
			//typename Rand::self_pointer_t rand_;
			
			node_id_t parent_;
			bool changed_parent_;
		
	}; // SemanticEntityOnehopNeighborhood
}

#endif // SEMANTIC_ENTITY_ONEHOP_NEIGHBORHOOD_H

