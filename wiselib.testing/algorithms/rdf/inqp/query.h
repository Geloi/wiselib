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

#ifndef QUERY_H
#define QUERY_H

#include <util/pstl/map_static_vector.h>
#include "operators/operator.h"
#include "operator_descriptions/operator_description.h"

#include "query_message.h"

namespace wiselib {
	
	/**
	 * @brief
	 * 
	 * TODO: Store received packets here, order them by sequence number
	 * e.g. using a priority queue, construct incrementally as possible.
	 * Use a special "# of packets" packet that signifies when final
	 * construction/execution can take place.
	 * 
	 * @ingroup
	 * 
	 * @tparam 
	 */
	template<
		typename OsModel_P,
		typename QueryProcessor_P
	>
	class INQPQuery {
		
		public:
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef QueryProcessor_P QueryProcessor;
			
			typedef INQPQuery<OsModel_P, QueryProcessor_P> self_type;
			
			typedef typename QueryProcessor::BasicOperatorDescription BasicOperatorDescription;
			typedef BasicOperatorDescription BOD;
			typedef typename QueryProcessor::BasicOperator BasicOperator;
			typedef MapStaticVector<OsModel, size_type, BasicOperator*, 16> Operators;
			
			typedef ::uint8_t query_id_t;
			typedef ::uint8_t sequence_number_t;
			
			void init(QueryProcessor* processor, query_id_t id) {
				processor_ = processor;
				query_id_ = id;
				expected_operators_set_ = false;
			}
			
			Operators& operators() { return operators_; }
			
			template<typename OperatorT>
			void add_operator(OperatorT* op) {
				operators_[op->id()] = reinterpret_cast<BasicOperator*>(op);
			}
			
			template<typename DescriptionT, typename OperatorT>
			void add_operator(BOD *bod) {
				DescriptionT *description = reinterpret_cast<DescriptionT*>(bod);
				OperatorT *op = ::get_allocator().template allocate<OperatorT>().raw();
				op->init(description, this);
				operators_[bod->id()] = reinterpret_cast<BasicOperator*>(op);
			}
			
			void build_tree() {
				for(typename Operators::iterator iter = operators_.begin(); iter != operators_.end(); ++iter) {
					
					BasicOperator* op = iter->second;
					if(op->parent_id() != 0) {
						op->attach_to(operators_[op->parent_id()]);
					}
				}
			}
			
			QueryProcessor& processor() { return *processor_; }
			query_id_t id() { return query_id_; }
			BasicOperator* get_operator(size_type i) { return operators_[i]; }
			
			bool ready() {
				return expected_operators_set_ && (expected_operators_ == operators_.size());
			}
			
			void set_expected_operators(size_type n) {
				expected_operators_ = n;
				expected_operators_set_ = true;
			}
		
		private:
			query_id_t query_id_;
			Operators operators_;
			QueryProcessor* processor_;
			size_type expected_operators_;
			bool expected_operators_set_;
			
		
	}; // INQPQuery
}

#endif // QUERY_H

