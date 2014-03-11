
#include "defs.h"

////
//#define APP_DATABASE_DEBUG 1
//#define APP_DATABASE_ERASE 1
//#define MODE_ERASE 1
////

#if APP_DATABASE_DEBUG
	#define APP_HEARTBEAT 1
	#undef NDEBUG
#else
	#define NDEBUG
#endif

//#if (TS_USE_TREE_DICT || TS_USE_PRESCILLA_DICT || TS_USE_AVL_DICT)
	#define TS_USE_ALLOCATOR 1
//#endif
#undef TS_USE_BLOCK_MEMORY
#define TS_CODEC_NONE 1
#undef TS_CODEC_HUFFMAN

#include "platform.h"

extern "C" {
	#include <string.h>
}

#include "external_interface/external_interface.h"
#include "external_interface/external_interface_testing.h"
using namespace wiselib;
typedef OSMODEL Os;
typedef OSMODEL OsModel;
typedef Os::block_data_t block_data_t;
typedef Os::size_t size_type;

#if TS_USE_ALLOCATOR
	#warning "Using BITMAP allocator"

	#include <util/allocators/bitmap_allocator.h>

	#if TS_USE_PRESCILLA_DICT
		typedef wiselib::BitmapAllocator<Os, 3072, 16> Allocator;
	#elif TS_USE_AVL_DICT
		typedef wiselib::BitmapAllocator<Os, 3072, 16> Allocator;
	#elif TS_USE_TREE_DICT
		typedef wiselib::BitmapAllocator<Os, 3072, 16> Allocator;
	#else
		// actually, even for the static dict we need some
		// allocation (namely during iteration)
		typedef wiselib::BitmapAllocator<Os, 512, 16> Allocator;
	#endif


	Allocator allocator_;
	Allocator& get_allocator() { return allocator_; }
#endif

#include "setup_tuplestore.h"

#include <util/serialization/serialization.h>
#include <util/meta.h>

#if defined(CONTIKI)
extern "C" {
	#define delete why_on_earth_do_you_guys_name_a_variable_delete
	#define DOMAIN DOMAIN_WHAT_IS_THIS_I_DONT_EVEN

	#include <contiki.h>
	#include <netstack.h>
	#include <stdlib.h>
}
#endif

class App {
	public:

		#include "app_db_mixin.cpp"

		CodecTupleStoreT::Dictionary dictionary_;
		CodecTupleStoreT::TupleContainer container_;
		CodecTupleStoreT tuplestore_;

		/**
		 * Do all necessary runtime initialization for the Wiselib TupleStore.
		 */
		void initialize_db() {
			dictionary_.init(debug_);
			tuplestore_.init(&dictionary_, &container_, debug_);
			#if APP_DATABASE_DEBUG
				debug_->debug("ini");
			#endif
		}

		void insert_tuple(char* s, char* p, char* o) {
			Tuple t;
			t.set(0, (block_data_t*)s);
			t.set(1, (block_data_t*)p);
			t.set(2, (block_data_t*)o);

			#if APP_DATABASE_DEBUG
				debug_->debug("i(%s,%s,%s)", (char*)t.get(0), (char*)t.get(1), (char*)t.get(2));
			#endif
			tuplestore_.insert(t);

			#if APP_DATABASE_DEBUG
					//debug_->debug("Post-insert emergency iteration");

					//int j = 0;
					//for(CodecTupleStoreT::iterator jter = tuplestore_.begin(); jter != tuplestore_.end(); ++jter, j++) {
						//debug_->debug("ALART [%d]=(%s,%s,%s)", (int)j, (char*)jter->get(0), (char*)jter->get(1), (char*)jter->get(2));
					//}
					//debug_->debug("Emergency iteration concluded. Have a nice day.");
			#endif
		}

		size_type size() { return tuplestore_.size(); }

		inline void find(block_data_t* s, block_data_t* p, block_data_t* o, char *) {
			Tuple t;
			t.set(0, s);
			t.set(1, p);
			t.set(2, o);

			CodecTupleStoreT::column_mask_t mask =
				((s != 0) << 0) | ((p != 0) << 1) | ((o != 0) << 2);

			Tuple v;
			CodecTupleStoreT::iterator iter = tuplestore_.begin(&t, mask);
		}

		void erase(block_data_t*s, block_data_t* p, block_data_t* o) {
			if(tuplestore_.size() == 0) {
				while(1) ;
			}

			#if APP_DATABASE_DEBUG
					//debug_->debug("Pre-erase emergency iteration");

					//int j = 0;
					//for(CodecTupleStoreT::iterator jter = tuplestore_.begin(); jter != tuplestore_.end(); ++jter, j++) {
						//debug_->debug("ALART [%d]=(%s,%s,%s)", (int)j, (char*)jter->get(0), (char*)jter->get(1), (char*)jter->get(2));
					//}
					//debug_->debug("Emergency iteration concluded. Have a nice day.");
			#endif

			Tuple t;
			t.set(0, s);
			t.set(1, p);
			t.set(2, o);
			CodecTupleStoreT::column_mask_t mask =
				((*s != 0) << 0) | ((*p != 0) << 1) | ((*o != 0) << 2);

			#if APP_DATABASE_DEBUG
				debug_->debug("e(%s,%s,%s) s%d m%d", (char*)s, (char*)p, (char*)o, (int)tuplestore_.size(), (int)mask);
			#endif
			//Tuple v;
			CodecTupleStoreT::iterator iter = tuplestore_.begin(&t, mask);
			#if APP_DATABASE_DEBUG
				if(iter == tuplestore_.end()) {
					debug_->debug("ALART erase tuple not found");
					//debug_->debug("Commencing emergency iteration");

					//int j = 0;
					//for(CodecTupleStoreT::iterator jter = tuplestore_.begin(); jter != tuplestore_.end(); ++jter, j++) {
						//debug_->debug("ALART [%d]=(%s,%s,%s)", (int)j, (char*)jter->get(0), (char*)jter->get(1), (char*)jter->get(2));
					//}
					//debug_->debug("Emergency iteration concluded. Have a nice day.");
					
				}
				else {
					debug_->debug("found (%s,%s,%s)", (char*)iter->get(0), (char*)iter->get(1), (char*)iter->get(2));
				}
			#endif
			//do {
			tuplestore_.erase(iter);
			//} while(iter != tuplestore_.end());
			#if APP_DATABASE_DEBUG
				debug_->debug("pe(%s,%s,%s) s%d", (char*)s, (char*)p, (char*)o, (int)tuplestore_.size());
			#endif
		}

		void prepare_erase(block_data_t*s, block_data_t* p, block_data_t* o) {
			if(tuplestore_.size() == 0) {
				while(1) ;
			}

			RandomChoice c(tuplestore_.size());
			*s = '\0';
			*p = '\0';
			*o = '\0';
			CodecTupleStoreT::iterator iter;
			for(iter = tuplestore_.begin(); iter != tuplestore_.end(); ++iter, ++c) {
				if(c.choose()) {
					int spo = (unsigned)rand() % 3;
					//debug_->debug("spo=%d *iter=(%s,%s,%s)",
						//(int)spo,
						//(char*)iter->get(0), (char*)iter->get(1),
						//(char*)iter->get(2));
					strcpy(reinterpret_cast<char*>(s), reinterpret_cast<char*>(iter->get(0)));
					strcpy(reinterpret_cast<char*>(p), reinterpret_cast<char*>(iter->get(1)));
					strcpy(reinterpret_cast<char*>(o), reinterpret_cast<char*>(iter->get(2)));
					if(spo == 0) { *s = '\0'; }
					else if(spo == 1) { *p = '\0'; }
					else { *o = '\0'; }
					break;
				}
			}
			if(*s == '\0' && *p == '\0') {
				iter = tuplestore_.begin();
					int spo = (unsigned)rand() % 3;
					//debug_->debug("Xspo=%d *iter=(%s,%s,%s)",
						//(int)spo,
						//(char*)iter->get(0), (char*)iter->get(1),
						//(char*)iter->get(2));
					strcpy(reinterpret_cast<char*>(s), reinterpret_cast<char*>(iter->get(0)));
					strcpy(reinterpret_cast<char*>(p), reinterpret_cast<char*>(iter->get(1)));
					strcpy(reinterpret_cast<char*>(o), reinterpret_cast<char*>(iter->get(2)));
					if(spo == 0) { *s = '\0'; }
					else if(spo == 1) { *p = '\0'; }
					else { *o = '\0'; }
			}
			#if APP_DATABASE_DEBUG
				debug_->debug("wErs(%s,%s,%s) %d%d%d", (char*)s, (char*)p, (char*)o, (int)(*s == 0), (int)(*p == 0), (int)(*o == 0));
			#endif
		}

};

// <general wiselib boilerplate>
// {{{
	
	// Application Entry Point & Definiton of allocator
	wiselib::WiselibApplication<Os, App> app;
	void application_main(Os::AppMainParameter& amp) { app.init(amp); }
	
// }}}
// </general wiselib boilerplate>

// vim: set ts=4 sw=4 tw=78 noexpandtab foldmethod=marker foldenable :
