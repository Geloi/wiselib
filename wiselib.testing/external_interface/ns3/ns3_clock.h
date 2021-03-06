/***************************************************************************
 ** This file is part of the generic algorithm library Wiselib.           **
 ** Copyright (C) 2013 by the Wisebed (www.wisebed.eu) project.      **
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
#ifndef CONNECTOR_NS3_CLOCK_H
#define CONNECTOR_NS3_CLOCK_H

#include "external_interface/ns3/ns3_types.h"
#include <cstdlib>

namespace wiselib
{
   /** \brief Ns3 Implementation of \ref clock_concept "Clock Concept"
    *  \ingroup clock_concept
    *  \ingroup ns3_facets
    *
    * Ns3 implementation of the \ref clock_concept "Clock Concept" ...
    */
   template<typename OsModel_P>
   class Ns3ClockModel
   {
   public:
      typedef OsModel_P OsModel;

      typedef Ns3ClockModel<OsModel> self_type;
      typedef self_type* self_pointer_t;

      typedef double time_t;

      // --------------------------------------------------------------------
      enum {
         CLOCKS_PER_SECOND = 1000
      };
      // --------------------------------------------------------------------
      Ns3ClockModel( Ns3Os& os )
         : os_(os)
      {}
      // --------------------------------------------------------------------
      time_t time()
      {
         return os().proc.GetNs3Time ();
      }
      // --------------------------------------------------------------------
      uint16_t microseconds( time_t time )
      {
         return (uint16_t)((time - int(time)) * 1000);
      }
      // --------------------------------------------------------------------
      uint16_t milliseconds( time_t time )
      {
         return (uint16_t)((time - int(time)) * 1000);
      }
      // --------------------------------------------------------------------
      uint32_t seconds( time_t time )
      {
         return (uint32_t)time;
      }

   private:
      Ns3Os& os()
      { return os_; }
      // --------------------------------------------------------------------
      Ns3Os& os_;
   };
}

#endif
