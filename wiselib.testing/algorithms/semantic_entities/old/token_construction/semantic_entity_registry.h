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

#ifndef SEMANTIC_ENTITY_REGISTRY_H
#define SEMANTIC_ENTITY_REGISTRY_H

#include "semantic_entity_id.h"

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
		typename SemanticEntity_P,
		typename GlobalTree_P,
		size_t MAX_SEMANTIC_ENTITIES_P
	>
	class SemanticEntityRegistry {
		public:
			typedef SemanticEntityRegistry self_type;
			typedef self_type* self_pointer_t;
			
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef SemanticEntity_P SemanticEntityT;
			typedef GlobalTree_P GlobalTreeT;
			
			enum { MAX_SEMANTIC_ENTITIES = MAX_SEMANTIC_ENTITIES_P };
			
			typedef MapStaticVector<OsModel, SemanticEntityId, SemanticEntityT, MAX_SEMANTIC_ENTITIES> SemanticEntityMapT;
			typedef typename SemanticEntityMapT::iterator iterator;
			
			SemanticEntityRegistry() :  global_tree_(0) {
			}
			
			void init(typename GlobalTreeT::self_pointer_t gt) {
				global_tree_ = gt;
				
				check();
			}
			
			SemanticEntityT& add(const SemanticEntityId& id) {
				check();
				
				map_[id] = SemanticEntityT(id, global_tree_);
				return map_[id];
			}
			
			void erase(const SemanticEntityId& id) {
				check();
				map_.erase(id);
			}
			
			bool contains(const SemanticEntityId& id) {
				if(id.is_all()) { return true; }
				if(id.is_invalid()) { return false; }
				return map_.contains(id);
			}
			
			SemanticEntityT* get(const SemanticEntityId& id) {
				check();
				
				if(map_.contains(id)) {
					return &map_[id];
				}
				return 0;
			}
			
			iterator begin() { return map_.begin(); }
			iterator end() { return map_.end(); }
			
			void check() {
				#if !WISELIB_DISABLE_DEBUG
					assert(global_tree_ != 0);
				#endif
			}
		
		private:
			SemanticEntityMapT map_;
			typename GlobalTreeT::self_pointer_t global_tree_;
		
	}; // SemanticEntityRegistry
}

#endif // SEMANTIC_ENTITY_REGISTRY_H

