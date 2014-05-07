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

#include "semantic_entity.h"
#include "semantic_entity_id.h"

#ifndef TOKEN_STATE_FORWARD_MESSAGE_H
#define TOKEN_STATE_FORWARD_MESSAGE_H

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
		typename Radio_P
	>
	class TokenStateMessage {
		
		public:
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef Radio_P Radio;
			typedef typename Radio::message_id_t message_id_t;
			typedef typename Radio::node_id_t node_id_t;
			typedef SemanticEntity_P SemanticEntity;
			typedef typename SemanticEntity::TokenState TokenState;
			typedef ::uint32_t abs_millis_t;
			typedef ::uint8_t flags_t;
			
			enum Restrictions {
				MAX_MESSAGE_LENGTH = Radio::MAX_MESSAGE_LENGTH
			};
			
			enum {
				MESSAGE_TYPE = INSE_MESSAGE_TYPE_TOKEN_STATE
			};
			
			enum Positions {
				POS_MESSAGE_ID = 0,
				POS_TOKEN_STATE = POS_MESSAGE_ID + sizeof(message_id_t),
				POS_CYCLE_TIME = POS_TOKEN_STATE + sizeof(TokenState),
				POS_CYCLE_WINDOW = POS_CYCLE_TIME + sizeof(abs_millis_t),
				//POS_ENTITY_ID = POS_FLAGS + sizeof(flags_t), // sizeof(bool),
				//POS_TOKEN_STATE = POS_FLAGS + sizeof(flags_t),
				//POS_TIME_OFFSET = POS_TOKEN_STATE + sizeof(TokenState),
				
			// for debugging only
				POS_SRC = POS_CYCLE_WINDOW + sizeof(abs_millis_t),
				POS_SRCTIME = POS_SRC + sizeof(node_id_t),
				
				//POS_END = POS_CYCLE_WINDOW + sizeof(abs_millis_t)
				POS_END = POS_SRCTIME + sizeof(abs_millis_t)
			};
			
			//enum {
				//FLAGS_INFORMATIONAL = 1
			//};
			
			TokenStateMessage() {
				init();
			}
			
			void init() {
				set_type(MESSAGE_TYPE);
				set_cycle_time(0);
				set_cycle_window(0);
				//set_informational(false);
			}
			
			message_id_t type() {
				return wiselib::read<OsModel, block_data_t, message_id_t>(data_ + POS_MESSAGE_ID);
			}
			
			void set_type(message_id_t t) {
				wiselib::write<OsModel>(data_ + POS_MESSAGE_ID, t);
			}
			
			TokenState token_state() {
				return wiselib::read<OsModel, block_data_t, TokenState>(data_ + POS_TOKEN_STATE);
			}
			
			void set_token_state(const TokenState& s) {
				wiselib::write<OsModel>(data_ + POS_TOKEN_STATE, s);
			}
			
			abs_millis_t cycle_time() {
				return wiselib::read<OsModel, block_data_t, abs_millis_t>(data_ + POS_CYCLE_TIME);
			}
			
			void set_cycle_time(abs_millis_t t) {
				wiselib::write<OsModel>(data_ + POS_CYCLE_TIME, t);
			}
			
			abs_millis_t cycle_window() {
				return wiselib::read<OsModel, block_data_t, abs_millis_t>(data_ + POS_CYCLE_WINDOW);
			}
			
			void set_cycle_window(abs_millis_t t) {
				wiselib::write<OsModel>(data_ + POS_CYCLE_WINDOW, t);
			}
			
			node_id_t source() {
				return wiselib::read<OsModel, block_data_t, node_id_t>(data_ + POS_SRC);
			}
			
			void set_source(node_id_t src) {
				wiselib::write<OsModel>(data_ + POS_SRC, src);
			}
			
			abs_millis_t sourcetime() {
				return wiselib::read<OsModel, block_data_t, abs_millis_t>(data_ + POS_SRCTIME);
			}
			
			void set_sourcetime(abs_millis_t t) {
				wiselib::write<OsModel>(data_ + POS_SRCTIME, t);
			}
			
			block_data_t* data() {
				return data_;
			}
			
			size_type size() {
				return POS_END;
			}
			
			bool operator==(TokenStateMessage other) {
				return type() == other.type() &&
					token_state() == other.token_state() &&
					cycle_time() == other.cycle_time() &&
					cycle_window() == other.cycle_window() &&
					source() == other.source() &&
					sourcetime() == other.sourcetime();
			}
					
			
			/*
			void set_informational(bool i) {
				data_[POS_FLAGS] = (data_[POS_FLAGS] & ~FLAGS_INFORMATIONAL) | (i ? FLAGS_INFORMATIONAL : 0);
			}
			
			bool informational() { 
				return data_[POS_FLAGS] & FLAGS_INFORMATIONAL;
			}
			*/
			
			void check() {
				#if CHECK_INVARIANTS
					assert(type() == MESSAGE_TYPE);
				#endif
			}
		
		private:
			block_data_t data_[MAX_MESSAGE_LENGTH];
		
	}; // TokenStateMessage
}

#endif // TOKEN_STATE_FORWARD_MESSAGE_H

