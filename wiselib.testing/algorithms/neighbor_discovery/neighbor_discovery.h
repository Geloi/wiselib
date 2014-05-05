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

#ifndef __NEIGHBOR_DISCOVERY_H__
#define	__NEIGHBOR_DISCOVERY_H__

#include "util/pstl/vector_static.h"
#include "util/delegates/delegate.hpp"
#include "../../internal_interface/message/message.h"
#include "neighbor_discovery_source_config.h"
#include "neighbor_discovery_default_values_config.h"
#include "neighbor.h"
#include "protocol_settings.h"
#include "protocol_payload.h"
#include "protocol.h"
#include "beacon.h"
#include "util/serialization/simple_types.h"
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
#ifdef UNIGE_TESTBED
#include "../../../apps/generic_apps/topologies/UNIGE_ISENSE_topology.h"
#endif
#ifdef CTI_TESTBED
#include "../../../apps/generic_apps/topologies/CTI_ISENSE_topology.h"
#endif
#endif

namespace wiselib
{
	template<	typename Os_P,
				typename Radio_P,
				typename Clock_P,
				typename Timer_P,
				typename Rand_P,
				typename Debug_P
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
				,typename iSenseBatterySensor_P
#endif
				>
	class NeighborDiscovery_Type
	{
	public:
		typedef Os_P Os;
		typedef Radio_P Radio;
		typedef Timer_P Timer;
		typedef Debug_P Debug;
		typedef Clock_P Clock;
		typedef Rand_P Rand;
		typedef typename Radio::node_id_t node_id_t;
		typedef typename Radio::size_t size_t;
		typedef typename Radio::block_data_t block_data_t;
		typedef typename Radio::message_id_t message_id_t;
		typedef typename Clock::time_t time_t;
		typedef typename Radio::ExtendedData ExData;
		typedef typename Radio::TxPower TxPower;
		typedef typename Timer::millis_t millis_t;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
		typedef iSenseBatterySensor_P iSenseBatterySensor;
		typedef NeighborDiscovery_Type	<Os, Radio,	Clock, Timer, Rand, Debug, iSenseBatterySensor> self_t;
#else
		typedef NeighborDiscovery_Type	<Os, Radio,	Clock, Timer, Rand, Debug> self_t;
#endif
		typedef Message_Type<Os, Radio, Debug> Message;
		typedef Neighbor_Type<Os, Radio, Clock, Timer, Debug> Neighbor;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
		typedef typename Neighbor::Position Position;
#endif
		typedef ProtocolPayload_Type< Os, Radio, Debug> ProtocolPayload;
		typedef ProtocolSettings_Type<Os, Radio, Timer, Debug> ProtocolSettings;
		typedef Protocol_Type<Os, Radio, Clock, Timer, Debug> Protocol;
		typedef Beacon_Type<Os, Radio, Clock, Timer, Debug> Beacon;
		typedef vector_static<Os, Neighbor, ND_MAX_NEIGHBORS> Neighbor_vector;
		typedef typename Neighbor_vector::iterator Neighbor_vector_iterator;
		typedef vector_static<Os, ProtocolPayload, ND_MAX_REGISTERED_PROTOCOLS> ProtocolPayload_vector;
		typedef typename ProtocolPayload_vector::iterator ProtocolPayload_vector_iterator;
		typedef vector_static<Os, Protocol, ND_MAX_REGISTERED_PROTOCOLS> Protocol_vector;
		typedef typename Protocol_vector::iterator Protocol_vector_iterator;
		typedef delegate4<void, uint8_t, node_id_t, size_t, uint8_t*> event_notifier_delegate_t;

		// --------------------------------------------------------------------
		NeighborDiscovery_Type()	:
			status								( WAITING_STATUS ),
			beacon_period						( ND_BEACON_PERIOD ),
			transmission_power_dB				( ND_TRANSMISSION_POWER_DB ),
			protocol_max_payload_size			( ND_MAX_PROTOCOL_PAYLOAD_SIZE ),
			protocol_max_payload_size_strategy	( FIXED_PAYLOAD_SIZE ),
			nd_daemon_period					( ND_DAEMON_PERIOD )
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
			,messages_received					( 0 ),
			bytes_received						( 0 ),
			bytes_received_0					( 0 ),
			bytes_received_6					( 0 ),
			bytes_received_12					( 0 ),
			bytes_received_18					( 0 ),
			bytes_received_24					( 0 ),
			bytes_received_30					( 0 ),
			avg_bytes_size_received				( 0 ),
			messages_send						( 0 ),
			bytes_send							( 0 ),
			bytes_send_0						( 0 ),
			bytes_send_6						( 0 ),
			bytes_send_12						( 0 ),
			bytes_send_18						( 0 ),
			bytes_send_24						( 0 ),
			bytes_send_30						( 0 ),
			avg_bytes_size_send					( 0 ),
			corrupted_messages_received			( 0 ),
			corrupted_bytes_received			( 0 ),
			avg_corrupted_byte_size_received	( 0 ),
			clock_paradox_message_drops			( 0 ),
			bytes_received_periodic				( 0 ),
			metrics_counter						( 0 )
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
			,energy_counter1						( 0 ),
			energy_counter2							( 0 )
#endif
		{};
		// --------------------------------------------------------------------
		~NeighborDiscovery_Type()
		{};
		void enable()
		{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS_DAEMON
			timer().template set_timer<self_t, &self_t::nd_metrics_daemon> ( ND_STATS_DURATION, this, 0 );
#endif
			Protocol p;
			Neighbor n;
			n.set_id( radio().id() );
			n.set_active();
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT_ISENSE
			n.set_position( get_node_info<Position, Radio>( &radio() ) );
			set_position( get_node_info<Position, Radio>( &radio() ).get_x(), get_node_info<Position, Radio>( &radio() ).get_y(), get_node_info<Position, Radio>( &radio() ).get_z() );
#endif
			Neighbor_vector neighbors;
			neighbors.push_back( n );
			ProtocolPayload pp;
			pp.set_protocol_id( ND_PROTOCOL_ID );
			pp.set_payload_size( 1 );
			uint8_t events_flag = 	ProtocolSettings::NEW_NB|
									ProtocolSettings::UPDATE_NB|
									ProtocolSettings::NEW_PAYLOAD|
									ProtocolSettings::LOST_NB|
									ProtocolSettings::TRANS_DB_UPDATE|
									ProtocolSettings::BEACON_PERIOD_UPDATE|
									ProtocolSettings::NB_REMOVED;
			ProtocolSettings ps(
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
									ND_MAX_AVG_LQI_THRESHOLD,
									ND_MIN_AVG_LQI_THRESHOLD,
									ND_MAX_AVG_LQI_INVERSE_THRESHOLD,
									ND_MIN_AVG_LQI_INVERSE_THRESHOLD,
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
									ND_MAX_AVG_RSSI_THRESHOLD,
									ND_MIN_AVG_RSSI_THRESHOLD,
									ND_MAX_AVG_RSSI_INVERSE_THRESHOLD,
									ND_MIN_AVG_RSSI_INVERSE_THRESHOLD,
#endif
									ND_MAX_LINK_STAB_RATIO_THRESHOLD,
									ND_MIN_LINK_STABILITY_RATIO_THRESHOLD,
									ND_MAX_LINK_STAB_RATIO_INVERSE_THRESHOLD,
									ND_MIN_LINK_STAB_RATIO_INVERSE_THRESHOLD,
									events_flag,
									ProtocolSettings::RATIO_DIVIDER,
									ND_RATIO_DIVIDER,
									ProtocolSettings::NEW_DEAD_TIME_PERIOD,
									ND_OLD_DEAD_TIME_PERIOD_WEIGHT,
									ND_NEW_DEAD_TIME_PERIOD_WEIGHT,
									ProtocolSettings::R_NR_WEIGHTED,
									ND_BEACON_WEIGHT,
									ND_LOST_BEACON_WEIGHT,
									pp
								);
			p.set_protocol_id( ND_PROTOCOL_ID );
			p.set_neighborhood( neighbors );
			p.set_protocol_settings( ps );
			p.set_event_notifier_callback( event_notifier_delegate_t::template from_method<NeighborDiscovery_Type, &NeighborDiscovery_Type::events_callback > ( this ) );
			protocols.push_back( p );
			set_status( ACTIVE_STATUS );
			radio().enable_radio();
			recv_callback_id_ = radio().template reg_recv_callback<self_t, &self_t::receive>( this );
#ifdef CONFIG_NEIGHBOR_DISCOVER_H_RAND_BACKOFF_BEACONS
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RAND_STARTUP
			timer().template set_timer<self_t, &self_t::beacon_scheduler> ( rand()() % get_beacon_period(), this, 0 );
#else
			beacon_scheduler();
#endif
#else
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RAND_STARTUP
			debug().debug("%x:%i:%d:%d:%d:R\n", radio().id(), transmission_power_dB, beacon_period, nd_daemon_period, ND_STATS_DURATION );
			timer().template set_timer<self_t, &self_t::beacons> ( rand()() % get_beacon_period(), this, 0 );
#else
			debug().debug("%x:%i:%d:%d:%d:N\n", radio().id(), transmission_power_dB, beacon_period, nd_daemon_period,  ND_STATS_DURATION );
			beacons();
#endif
#endif
			nd_daemon();
		};
		// --------------------------------------------------------------------
		void disable()
		{
			set_status( WAITING_STATUS );
			radio().unreg_recv_callback( recv_callback_id_ );
		};
		// --------------------------------------------------------------------
		void events_callback( uint8_t _event, node_id_t _node_id, size_t _len, uint8_t* _data )
		{
			if ( _event & ProtocolSettings::TRANS_DB_UPDATE )
			{
#ifdef DEBUG_ATP_H_EVENTS_CALLBACK
				debug().debug( "NB - events_callback %x - TRANS_DB_UPDATE %x.\n", radio().id(), _node_id );
#endif
			}
			else if ( _event & ProtocolSettings::BEACON_PERIOD_UPDATE )
			{
#ifdef DEBUG_ATP_H_EVENTS_CALLBACK
				debug().debug( "NB - events_callback %x - BEACON_PERIOD_UPDATE %x.\n", radio().id(), _node_id );
#endif
			}
		}
		// --------------------------------------------------------------------
		void send( node_id_t _dest, size_t _len, block_data_t* _data, message_id_t _msg_id )
		{
			Message message;
			message.set_message_id( _msg_id );
			message.set_payload( _len, _data );
			TxPower power;
			power.set_dB( get_transmission_power_dB() );
			radio().set_power( power );
			radio().send( _dest, message.serial_size(), message.serialize() );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_SEND
			debug().debug( "NeighborDiscovery - send - Message has payload len: %d, serial_size: %d", _len, message.serial_size() );
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
			messages_send = messages_send + 1;
			bytes_send = bytes_send + message.serial_size() + sizeof( size_t ) + sizeof( node_id_t );
			avg_bytes_size_send = bytes_send / messages_send;
			if ( get_transmission_power_dB() == 0 )
			{
				bytes_send_0++;
			}
			else if ( get_transmission_power_dB() == -6 )
			{
				bytes_send_6++;
			}
			else if ( get_transmission_power_dB() == -12 )
			{
				bytes_send_12++;
			}
			else if ( get_transmission_power_dB() == -18 )
			{
				bytes_send_18++;
			}
			else if ( get_transmission_power_dB() == -24 )
			{
				bytes_send_24++;
			}
			else if ( get_transmission_power_dB() == -30 )
			{
				bytes_send_30++;
			}
#endif
		}
		// --------------------------------------------------------------------
		void beacon_scheduler( void* _data = NULL )
		{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACON_SCHEDULER
			debug().debug("NeighborDiscovery - beacon_scheduler - Entering scheduler\n", radio().id() );
#endif
			millis_t bp = get_beacon_period();
			uint32_t backoff_beacon_period = rand()() % ( bp - 50 );
			time_t current_time = clock().time();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
			//debug().debug("BEAC_SCH:%x:%d:%d:%d:%d\n", radio().id(), clock().seconds( current_time ), clock().milliseconds( current_time ), backoff_beacon_period, bp );
#endif
			timer().template set_timer<self_t, &self_t::beacons>( backoff_beacon_period, this, 0 );
			timer().template set_timer<self_t, &self_t::beacon_scheduler> ( bp, this, 0 );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACON_SCHEDULER
			debug().debug("NeighborDiscovery - beacon_scheduler - Exiting scheduler\n", radio().id() );
#endif
		}
		// --------------------------------------------------------------------
		void beacons( void* _data = NULL )
		{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
			debug().debug( "NeighborDiscovery - beacons - Entering.\n" );
#endif
			if ( get_status() == ACTIVE_STATUS )
			{
				Protocol* p_ptr = get_protocol_ref( ND_PROTOCOL_ID );;
				if ( p_ptr != NULL )
				{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
					debug().debug( "NeighborDiscovery - beacons - Protocol exists.\n" );
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
					time_t current_time = clock().time();
					debug().debug( "Time:%d:%d", clock().seconds( current_time ), clock().milliseconds( current_time )  );
#endif
					millis_t bp = get_beacon_period();
					Neighbor* n = p_ptr->get_neighbor_ref( radio().id() );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
					debug().debug( "beacon_period:%d", bp );
#endif
					if ( n != NULL )
					{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
						debug().debug( "NeighborDiscovery - beacons - Neighbor exists.\n" );
#endif
						n->inc_beacon_period_update_counter();
						n->set_beacon_period( bp );
						n->inc_total_beacons();
						Beacon beacon;
						beacon.set_protocol_payloads( protocols );
						beacon.set_beacon_period( bp );
						beacon.set_beacon_period_update_counter( n->get_beacon_period_update_counter() );
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRANSMISSION_POWER_PIGGY
						beacon.set_TP( get_transmission_power_dB() );
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_SMALL_PAYLOAD
						Neighbor_vector nv_SCL;
						Neighbor_vector nv_non_SCL;
						Neighbor_vector nv;
						uint8_t SCLD=0;
						for ( Neighbor_vector_iterator i = p_ptr->get_neighborhood_ref()->begin(); i != p_ptr->get_neighborhood_ref()->end(); ++i )
						{
							if (		( 1 )
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_SMALL_PAYLOAD_TRUST
									&&	( i->get_trust_counter() >= ND_PAYLOAD_TRUST_COUNTER_THRESHOLD )
#endif
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_SMALL_PAYLOAD_LINK_STAB
									&&	( i->get_link_stab_ratio() >= ND_PAYLOAD_MIN_LINK_STAB_RATIO_THRESHOLD )
									&&	( i->get_link_stab_ratio() <= ND_PAYLOAD_MAX_LINK_STAB_RATIO_THRESHOLD )
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_SMALL_PAYLOAD_RSSI
									&&	( i->get_avg_RSSI() >= ND_PAYLOAD_MIN_RSSI_THRESHOLD )
									&&	( i->get_avg_RSSI() <= ND_PAYLOAD_MAX_RSSI_THRESHOLD )
#endif
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_SMALL_PAYLOAD_LQI
									&&	( i->get_avg_LQI() >= ND_PAYLOAD_MIN_LQI_THRESHOLD )
									&&	( i->get_avg_LQI() <= ND_PAYLOAD_MAX_LQI_THRESHOLD )
#endif
#endif
								)
							{
								if ( i->get_trust_counter_inverse() >= ND_TRUST_COUNTER_THRESHOLD_INVERSE )
								{
									SCLD++;
									nv_SCL.push_back( *i );
								}
								else
								{
									nv_non_SCL.push_back( *i );
								}
							}
						}
						if (nv_SCL.size() > 0 )
						{
							q_sort_neigh_active_con( 0, nv_SCL.size(), nv_SCL );
						}
						if ( nv_non_SCL.size() > 0 )
						{
							q_sort_neigh_active_con( 0, nv_non_SCL.size(), nv_non_SCL );
						}
						for ( Neighbor_vector_iterator i = nv_SCL.begin(); i != nv_SCL.end(); ++i )
						{
							nv.push_back( *i );
						}
						for ( Neighbor_vector_iterator i = nv_non_SCL.begin(); i != nv_non_SCL.end(); ++i )
						{
							nv.push_back( *i );
						}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
							for ( Neighbor_vector_iterator i = nv.begin(); i != nv.end(); ++i )
							{
								i->print( debug(), radio() );
							}
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ACTIVE_SCLD
						beacon.set_SCLD( SCLD );
#endif
#else
						Neighbor_vector nv = p_ptr->get_neighborhood();
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
						beacon.set_position( position );
#endif
						beacon.set_neighborhood( nv, radio().id() );
						block_data_t buff[Radio::MAX_MESSAGE_LENGTH];
						Message empty_message;
						size_t available_bytes = Radio::MAX_MESSAGE_LENGTH - empty_message.serial_size();
						if ( beacon.serial_size() > available_bytes )
						{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
							debug().debug("SERIAL_BEAC_PRE:%x:%d:%d", radio().id(), available_bytes, beacon.serial_size() );
#endif
							size_t items_to_pop = beacon.get_neighborhood_ref()->size() - ( ( available_bytes - ( beacon.serial_size() - ( n->serial_size() * beacon.get_neighborhood_ref()->size() ) ) ) / n->serial_size() );
							for ( size_t i = 0; i < items_to_pop; i++ )
							{
								beacon.get_neighborhood_ref()->pop_back();
							}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
							debug().debug("SERIAL_BEAC_POST:%x:%d:%d", radio().id(), available_bytes, beacon.serial_size() );
#endif
						}
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ACTIVE_CONNECTIVITY_FILTERING
						if ( beacon.get_neighborhood_ref()->size() > 0 )
						{
							q_sort_neigh_active_con( 0, beacon.get_neighborhood_ref()->size() - 1, *( beacon.get_neighborhood_ref() ) );
						}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
						Protocol* p_ptr_atp = get_protocol_ref( ATP_PROTOCOL_ID );
						time_t current_time = clock().time();
						debug().debug("SCLD:%x:[%d:%d] - %d:%d:%d:%d:%d:%d",radio().id(), Radio::MAX_MESSAGE_LENGTH, beacon.serial_size(), nv.size(), SCLD, p_ptr_atp->get_neighborhood_active_size(), p_ptr->get_neighborhood_active_size(),
																			clock().seconds( current_time ),
																			clock().milliseconds( current_time ) );
#endif
#endif

#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
						if ( (  radio().id() == 0x1c74 ) )
						{
							block_data_t bufff[20];
							block_data_t* buffff = bufff;
							size_t VOLTAGE_POS = 0;
							size_t TEMP_POS = VOLTAGE_POS + sizeof(uint16_t);
							size_t CURRENT_POS = TEMP_POS + sizeof(int16_t);
							size_t CHARGE_POS = CURRENT_POS + sizeof(int32_t);
							size_t COUNTER_POS = CHARGE_POS + sizeof(uint32_t);
							size_t MILLIS_POS = COUNTER_POS + sizeof(uint32_t);
							size_t payload_size = MILLIS_POS + sizeof(millis_t);
							uint16_t voltage = iSBS->BS().voltage;
							int16_t temp = iSBS->BS().temp;
							int32_t current = iSBS->BS().current;
							uint32_t charge = iSBS->BS().charge;
							time_t current_time = clock().time();
							millis_t mils = clock().milliseconds( current_time ) + clock().seconds( current_time ) * 1000;
							//debug().debug("ENERGY_SCL:%x:%d:%i:%i:%d", radio().id(), voltage, temp, current, charge);//, payload_size );
							write<Os, block_data_t, uint16_t>( buffff + VOLTAGE_POS, voltage );
							write<Os, block_data_t, int16_t>( buffff + TEMP_POS, temp );
							write<Os, block_data_t, int32_t>( buffff + CURRENT_POS, current );
							write<Os, block_data_t, uint32_t>( buffff + CHARGE_POS, charge );
							write<Os, block_data_t, uint32_t>( buffff + COUNTER_POS, energy_counter2 );
							energy_counter2++;
							write<Os, block_data_t, millis_t>( buffff + MILLIS_POS, mils );
							for ( ProtocolPayload_vector_iterator ppit = beacon.get_protocol_payloads_ref()->begin(); ppit != beacon.get_protocol_payloads_ref()->end(); ++ppit )
							{
								if ( ppit->get_protocol_id() == ND_PROTOCOL_ID )
								{
									//debug().debug("payload_found for ND_PROTO_ID");
									ppit->set_payload( buffff, payload_size );
								}
							}
						}
						if ( beacon.get_beacon_period_update_counter() < 28800 )
						{
#endif
							send( Radio::BROADCAST_ADDRESS, beacon.serial_size(), beacon.serialize( buff ), ND_MESSAGE );
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
						}
						else
						{
							Neighbor* n = p_ptr->get_neighbor_ref( radio().id() );
							n->set_beacon_period_update_counter( 0 );
							transmission_power_dB = transmission_power_dB + 6;
						}
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
						debug().debug( "NeighborDiscovery - beacons - Sending beacon %d.\n", beacon.get_beacon_period_update_counter() );
#endif
#ifndef CONFIG_NEIGHBOR_DISCOVER_H_RAND_BACKOFF_BEACONS
						timer().template set_timer<self_t, &self_t::beacons> ( bp, this, 0 );
#endif
					}
					else
					{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
						debug().debug( "NeighborDiscovery - beacons - Neighbor does not exist.\n" );
#endif
					}
				}
				else
				{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
					debug().debug( "NeighborDiscovery - beacons - Protocol does not exist.\n" );
#endif
				}
			}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_BEACONS
			debug().debug( "NeighborDiscovery-beacons - Exiting.\n" );
#endif
		}
		// --------------------------------------------------------------------
		void receive( node_id_t _from, size_t _len, block_data_t * _msg, ExData const &_ex )
		{
			if ( _from != radio().id() )
			{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
			debug().debug( "NeighborDiscovery - receive - From %x Entering.\n", _from );
#endif
				message_id_t msg_id = *_msg;
				Message *message = (Message*) _msg;
				if ( !message->compare_checksum() )
				{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
					debug().debug( "NeighborDiscovery - receive - Problem with csum from %x.\n", _from );
					debug().debug( "csum : %d vs comp : %d.\n", message->csum(), message->fletcher16_checksum( message->get_payload(), message->get_payload_size() ) );
					debug().debug( "message serial size: %d received len : %d.\n", message->serial_size(), _len );
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
					corrupted_messages_received = corrupted_messages_received + 1;
					corrupted_bytes_received = corrupted_bytes_received + _len;
					avg_corrupted_byte_size_received = corrupted_bytes_received / corrupted_messages_received;
#endif
					return;
				}
				if ( msg_id == ND_MESSAGE )
				{
					Beacon beacon;
					beacon.de_serialize( message->get_payload() );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
						debug().debug( "NeighborDiscovery - receive - Received beacon message from %x with message serial_size : %d, beacon serial_size : %d and neigh size : %d.\n", _from, message->serial_size(), beacon.serial_size(), beacon.get_neighborhood_ref()->size() );
#endif
					time_t current_time = clock().time();
					uint32_t dead_time = 0;
					int32_t dead_time_res = 0;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
					uint16_t signal_quality = _ex.get_lqi();
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
					uint16_t signal_strength = _ex.get_rssi();
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
					debug().debug( "NeighborDiscovery - receive - [signal_quality : %d], [signal_strength : %d]\n", signal_quality, signal_strength );
#endif
#endif
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
					if (radio().id() == 0x1b4c )
					{
						uint8_t found_flag = 0;
						ProtocolPayload pp;
						for ( ProtocolPayload_vector_iterator ppit = beacon.get_protocol_payloads_ref()->begin(); ppit != beacon.get_protocol_payloads_ref()->end(); ++ppit )
						{
							if ( ppit->get_protocol_id() == ND_PROTOCOL_ID )
							{
								pp = *ppit;
								found_flag = 1;
							}
						}
						if ( found_flag )
						{
							size_t VOLTAGE_POS = 0;
							size_t TEMP_POS = VOLTAGE_POS + sizeof(uint16_t);
							size_t CURRENT_POS = TEMP_POS + sizeof(int16_t);
							size_t CHARGE_POS = CURRENT_POS + sizeof(int32_t);
							size_t COUNTER_POS = CHARGE_POS + sizeof(uint32_t);
							size_t MILLIS_POS = COUNTER_POS + sizeof(uint32_t);
							block_data_t* buff = pp.get_payload_data();
							uint16_t voltage = read<Os, block_data_t, uint16_t>( buff + VOLTAGE_POS );
							int16_t temp = read<Os, block_data_t, int16_t>( buff + TEMP_POS );
							int32_t current = read<Os, block_data_t, int32_t>( buff + CURRENT_POS );
							uint32_t charge = read<Os, block_data_t, uint32_t>( buff + CHARGE_POS );
							uint32_t energy_counter3 = read<Os, block_data_t, uint32_t>( buff + COUNTER_POS );
							millis_t mils = read<Os, block_data_t, millis_t>( buff + MILLIS_POS );
							debug().debug("ENERGY:%d:%d:%d:%x:%i:%d:%i:%i:%d:%d", energy_counter1, energy_counter3, mils, _from, transmission_power_dB, voltage, temp, current, charge, pp.get_payload_size() );
							energy_counter1++;
						}
					}
#endif
					for ( Protocol_vector_iterator pit = protocols.begin(); pit != protocols.end(); ++pit )
					{
						uint8_t found_flag = 0;
						Neighbor new_neighbor;
						Neighbor_vector_iterator update_neighbor_it = pit->get_neighborhood_ref()->begin();
						for ( Neighbor_vector_iterator nit = pit->get_neighborhood_ref()->begin(); nit != pit->get_neighborhood_ref()->end(); ++nit )
						{
							if ( _from == nit->get_id() )
							{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
								if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
								{
								debug().debug( "NeighborDiscovery - receive %x - Neighbor %x is known for protocol %i.\n", radio().id(), _from, pit->get_protocol_id() );
								}
#endif
								update_neighbor_it = nit;
								found_flag = 1;
								dead_time_res = clock().seconds( current_time ) * 1000 - clock().seconds( nit->get_last_beacon() ) * 1000 + clock().milliseconds( current_time ) - clock().milliseconds( nit->get_last_beacon() );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
								if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
								{
								if ( clock().milliseconds( current_time ) == 0 )
								{
									debug().debug( "NeighborDiscovery - receive %x - Clock paradox possibility from: %x - %d:%d minus %d:%d.\n", radio().id(), _from, clock().seconds( current_time ), clock().seconds( nit->get_last_beacon() ), clock().milliseconds( current_time ), clock().milliseconds( nit->get_last_beacon() ) );
								}
								}
#endif
								if ( dead_time_res < 0 )
								{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
									if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
									{
									debug().debug( "NeighborDiscovery - receive %x - Clock paradox from: %x - %d:%d minus %d:%d.\n", radio().id(), _from, clock().seconds( current_time ), clock().seconds( nit->get_last_beacon() ), clock().milliseconds( current_time ), clock().milliseconds( nit->get_last_beacon() ) );
									}
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
									clock_paradox_message_drops++;
#endif
									return;
								}
								else
								{
									dead_time = clock().seconds( current_time ) * 1000 - clock().seconds( nit->get_last_beacon() ) * 1000 + clock().milliseconds( current_time ) - clock().milliseconds( nit->get_last_beacon() );
								}
								if ( beacon.get_beacon_period() == nit->get_beacon_period() )
								{
									if ( dead_time < beacon.get_beacon_period() + beacon.get_beacon_period()/2 )
									{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
										{
										debug().debug( "NeighborDiscovery - receive %x - Neighbor %x is on time same as advertised for protocol %i with dead_time : %d.\n", radio().id(), _from, pit->get_protocol_id(), dead_time );
										}
#endif
										new_neighbor = *nit;
										new_neighbor.set_transmission_power_dB( beacon.get_TP() );
										new_neighbor.inc_total_beacons( 1 * pit->resolve_beacon_weight( _from ) );
										new_neighbor.inc_total_beacons_expected( 1 * pit->resolve_beacon_weight( _from ) );
										new_neighbor.update_link_stab_ratio();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
										{
										debug().debug( "LSR:%x:%x:%d", radio().id(), new_neighbor.get_id(), new_neighbor.get_total_beacons_expected() );
										}
#endif
#ifndef CONFIG_NEIGHBOR_DISCOVERY_H_DTPC
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
										new_neighbor.update_avg_LQI( signal_quality, 1 );
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
										new_neighbor.update_avg_RSSI( signal_strength, 1 );
#endif
#else
										new_neighbor.set_avg_LQI( signal_quality );
										new_neighbor.set_avg_RSSI( signal_strength );
#endif
										new_neighbor.set_beacon_period( beacon.get_beacon_period() );
										new_neighbor.set_beacon_period_update_counter( beacon.get_beacon_period_update_counter() );
										new_neighbor.set_last_beacon( current_time );
									}
									else
									{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
										{
										debug().debug( "NeighborDiscovery - receive %x - Neighbor %x was late same as advertised for protocol %i with dead_time : %d.\n", radio().id(), _from, pit->get_protocol_id(), dead_time );
										}
#endif
										new_neighbor = *nit;
										new_neighbor.set_transmission_power_dB( beacon.get_TP() );
										new_neighbor.inc_total_beacons( 1 * pit->resolve_beacon_weight( _from ) );
										new_neighbor.inc_total_beacons_expected( ( dead_time / nit->get_beacon_period() ) * ( pit->resolve_lost_beacon_weight( _from ) ) );
										new_neighbor.update_link_stab_ratio();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
										{
										debug().debug( "LSR:%x:%x:%d", radio().id(), new_neighbor.get_id(), new_neighbor.get_total_beacons_expected() );
										}
#endif
#ifndef CONFIG_NEIGHBOR_DISCOVERY_H_DTPC
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
										new_neighbor.update_avg_LQI( signal_quality, 1 );
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
										new_neighbor.update_avg_RSSI( signal_strength, 1 );
#endif
#else
										new_neighbor.set_avg_LQI( signal_quality );
										new_neighbor.set_avg_RSSI( signal_strength );
#endif
										new_neighbor.set_beacon_period( beacon.get_beacon_period() );
										new_neighbor.set_beacon_period_update_counter( beacon.get_beacon_period_update_counter() );
										new_neighbor.set_last_beacon( current_time );
									}
								}
								else
								{
									if ( dead_time < beacon.get_beacon_period() + beacon.get_beacon_period()/2 )
									{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
										{
										debug().debug( "NeighborDiscovery - receive %x - Neighbor %x is on time same as advertised for protocol %i with dead_time : %d.\n", radio().id(), _from, pit->get_protocol_id(), dead_time );
										}
#endif
										new_neighbor = *nit;
										new_neighbor.set_transmission_power_dB( beacon.get_TP() );
										new_neighbor.inc_total_beacons( 1 * pit->resolve_beacon_weight( _from ) );
										new_neighbor.inc_total_beacons_expected( 1 * pit->resolve_beacon_weight( _from ) );
										new_neighbor.update_link_stab_ratio();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
										{
										debug().debug( "LSR:%x:%x:%d", radio().id(), new_neighbor.get_id(), new_neighbor.get_total_beacons_expected() );
										}
#endif
#ifndef CONFIG_NEIGHBOR_DISCOVERY_H_DTPC
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
										new_neighbor.update_avg_LQI( signal_quality, 1 );
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
										new_neighbor.update_avg_RSSI( signal_strength, 1 );
#endif
#else
										new_neighbor.set_avg_LQI( signal_quality );
										new_neighbor.set_avg_RSSI( signal_strength );
#endif
										new_neighbor.set_beacon_period( beacon.get_beacon_period() );
										new_neighbor.set_beacon_period_update_counter( beacon.get_beacon_period_update_counter() );
										new_neighbor.set_last_beacon( current_time );
									}
									else
									{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID )/* && ( radio().id() == 0x96f4 )*/ )
										{
										debug().debug( "NeighborDiscovery - receive %x - Neighbor %x is late and not as advertised for protocol %id with dead_time : %d.\n", radio().id(), _from, pit->get_protocol_id(), dead_time );
										}
#endif
										//TODO overflow here.
										//uint32_t last_beacon_period_update = beacon.get_beacon_period_update_counter() * beacon.get_beacon_period();
										millis_t approximate_beacon_period = 0;
										if ( pit->get_protocol_settings_ref()->get_dead_time_strategy() == ProtocolSettings::NEW_DEAD_TIME_PERIOD )
										{
											approximate_beacon_period = beacon.get_beacon_period();
										}
										else if ( pit->get_protocol_settings_ref()->get_dead_time_strategy() == ProtocolSettings::OLD_DEAD_TIME_PERIOD )
										{
											approximate_beacon_period = nit->get_beacon_period();
										}
										else if ( pit->get_protocol_settings_ref()->get_dead_time_strategy() == ProtocolSettings::MEAN_DEAD_TIME_PERIOD )
										{
											approximate_beacon_period = ( beacon.get_beacon_period() + nit->get_beacon_period() ) / 2;
										}
										else if ( pit->get_protocol_settings_ref()->get_dead_time_strategy() == ProtocolSettings::WEIGHTED_MEAN_DEAD_TIME_PERIOD )
										{
											approximate_beacon_period = ( beacon.get_beacon_period() * pit->get_protocol_settings_ref()->get_new_dead_time_period_weight() + nit->get_beacon_period() * pit->get_protocol_settings_ref()->get_old_dead_time_period_weight() ) / ( pit->get_protocol_settings_ref()->get_old_dead_time_period_weight() + pit->get_protocol_settings_ref()->get_new_dead_time_period_weight() );
										}
										uint32_t dead_time_messages_lost = dead_time / approximate_beacon_period;
										new_neighbor = *nit;
										new_neighbor.set_transmission_power_dB( beacon.get_TP() );
										new_neighbor.inc_total_beacons( 1 * pit->resolve_beacon_weight( _from ) );
										new_neighbor.inc_total_beacons_expected( ( dead_time_messages_lost + 1 ) * ( pit->resolve_lost_beacon_weight( _from ) ) );
										new_neighbor.update_link_stab_ratio();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) /*&& ( radio().id() == 0x96f4 )*/ )
										{
										debug().debug( "LSR:%x:%x:%d:%d:%d:%d:%d\n", radio().id(), new_neighbor.get_id(), dead_time_messages_lost, beacon.get_beacon_period(), nit->get_beacon_period(), dead_time, pit->resolve_beacon_weight( _from ), new_neighbor.get_total_beacons_expected() );
										}
#endif
#ifndef CONFIG_NEIGHBOR_DISCOVERY_H_DTPC
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
										new_neighbor.update_avg_LQI( signal_quality, 1 );
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
										new_neighbor.update_avg_RSSI( signal_strength, 1 );
#endif
#else
										new_neighbor.set_avg_LQI( signal_quality );
										new_neighbor.set_avg_RSSI( signal_strength );
#endif
										new_neighbor.set_beacon_period( beacon.get_beacon_period() );
										new_neighbor.set_beacon_period_update_counter( beacon.get_beacon_period_update_counter() );
										new_neighbor.set_last_beacon( current_time );
									}
								}
							}
						}
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ACTIVE_CONNECTIVITY_FILTERING
						new_neighbor.set_active_connectivity( beacon.get_SCLD());
#endif
						if ( found_flag == 0 )
						{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
							if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
							{
							debug().debug( "NeighborDiscovery - receive %x - Neighbor %x is unknown for protocol %i.\n", radio().id(), _from, pit->get_protocol_id() );
							}
#endif
							new_neighbor.set_id( _from );
							new_neighbor.set_transmission_power_dB( beacon.get_TP() );
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
							new_neighbor.set_position( beacon.get_position() );
#endif
							new_neighbor.set_total_beacons( 1 );
							new_neighbor.set_total_beacons_expected( 1 );
							new_neighbor.set_link_stab_ratio( 0 );
							new_neighbor.set_link_stab_ratio_inverse( 0 );
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
							new_neighbor.update_avg_LQI( signal_quality, 1 );
#endif
#ifndef CONFIG_NEIGHBOR_DISCOVERY_H_DTPC
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
							new_neighbor.update_avg_RSSI( signal_strength, 1 );
#endif
#else
							new_neighbor.set_avg_LQI( signal_quality );
							new_neighbor.set_avg_RSSI( signal_strength );
#endif
							new_neighbor.set_beacon_period( beacon.get_beacon_period() );
							new_neighbor.set_beacon_period_update_counter( beacon.get_beacon_period_update_counter() );
							new_neighbor.set_last_beacon( current_time );
						}
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
						new_neighbor.inc_trust_counter();
#endif
						uint8_t inverse_found_flag = 0;
						for ( Neighbor_vector_iterator nit = beacon.get_neighborhood_ref()->begin(); nit != beacon.get_neighborhood_ref()->end(); ++nit )
						{
							if ( radio().id() == nit->get_id() )
							{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
								if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
								{
								debug().debug( "NeighborDiscovery - receive %x - 1- Neighbor %x was aware of node for protocol %i nn %d - [%d:%d] [%d:%d].\n", radio().id(), _from, pit->get_protocol_id(), new_neighbor.get_id(), new_neighbor.get_link_stab_ratio(), new_neighbor.get_link_stab_ratio_inverse(), new_neighbor.get_trust_counter(), new_neighbor.get_trust_counter_inverse() );
								}
#endif
								inverse_found_flag = 1;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
								new_neighbor.inc_trust_counter_inverse();
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
								if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
								{
								debug().debug( "NeighborDiscovery - receive %x - 2 - Neighbor %x was aware of node for protocol %i nn %d - [%d:%d] [%d:%d].\n", radio().id(), _from, pit->get_protocol_id(), new_neighbor.get_id(), new_neighbor.get_link_stab_ratio(), new_neighbor.get_link_stab_ratio_inverse(), new_neighbor.get_trust_counter(), new_neighbor.get_trust_counter_inverse() );
								}
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
								if ( new_neighbor.get_trust_counter_inverse() >= ND_TRUST_COUNTER_THRESHOLD_INVERSE )
								{
#endif
									new_neighbor.set_link_stab_ratio_inverse( nit->get_link_stab_ratio() );
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
									new_neighbor.set_avg_RSSI_inverse( nit->get_avg_RSSI() );
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
									new_neighbor.set_avg_LQI_inverse( nit->get_avg_LQI() );
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
									if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) && ( radio().id() == 0x96f4 ) )
									{
									debug().debug( "NeighborDiscovery - receive %x - Neighbor %x was aware of node for protocol %i nn %d INCREASING INVERSE- [%d:%d] [%d:%d].\n", radio().id(), _from, pit->get_protocol_id(), new_neighbor.get_id(), new_neighbor.get_link_stab_ratio(), new_neighbor.get_link_stab_ratio_inverse(), new_neighbor.get_trust_counter(), new_neighbor.get_trust_counter_inverse() );
									}
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
								}
#endif
							}
						}
						if ( inverse_found_flag == 0 )
						{
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
							new_neighbor.dec_trust_counter_inverse();
#endif
						}
						uint8_t events_flag = 0;
						if	(
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
								( new_neighbor.get_avg_LQI() <= pit->get_protocol_settings_ref()->get_max_avg_LQI_threshold() ) &&
								( new_neighbor.get_avg_LQI() >= pit->get_protocol_settings_ref()->get_min_avg_LQI_threshold() ) &&
								( new_neighbor.get_avg_LQI_inverse() <= pit->get_protocol_settings_ref()->get_max_avg_LQI_inverse_threshold() ) &&
								( new_neighbor.get_avg_LQI_inverse() >= pit->get_protocol_settings_ref()->get_min_avg_LQI_inverse_threshold() ) &&
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
								( new_neighbor.get_avg_RSSI() <= pit->get_protocol_settings_ref()->get_max_avg_RSSI_threshold() ) &&
								( new_neighbor.get_avg_RSSI() >= pit->get_protocol_settings_ref()->get_min_avg_RSSI_threshold() ) &&
								( new_neighbor.get_avg_RSSI_inverse() <= pit->get_protocol_settings_ref()->get_max_avg_RSSI_inverse_threshold() ) &&
								( new_neighbor.get_avg_RSSI_inverse() >= pit->get_protocol_settings_ref()->get_min_avg_RSSI_inverse_threshold() ) &&
#endif
								( new_neighbor.get_link_stab_ratio() <= pit->get_protocol_settings_ref()->get_max_link_stab_ratio_threshold() ) &&
								( new_neighbor.get_link_stab_ratio() >= pit->get_protocol_settings_ref()->get_min_link_stab_ratio_threshold() ) &&
								( new_neighbor.get_link_stab_ratio_inverse() <= pit->get_protocol_settings_ref()->get_max_link_stab_ratio_inverse_threshold() ) &&
								( new_neighbor.get_link_stab_ratio_inverse() >= pit->get_protocol_settings_ref()->get_min_link_stab_ratio_inverse_threshold() )
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
							&&	( new_neighbor.get_trust_counter() >= ND_TRUST_COUNTER_THRESHOLD ) &&
								( new_neighbor.get_trust_counter_inverse() >= ND_TRUST_COUNTER_THRESHOLD_INVERSE )
#endif
							)
						{
							new_neighbor.set_active();
							if ( found_flag == 1 )
							{
								events_flag = events_flag | ProtocolSettings::UPDATE_NB;
								if ( new_neighbor.get_beacon_period() != update_neighbor_it->get_beacon_period() )
								{
									events_flag = events_flag | ProtocolSettings::BEACON_PERIOD_UPDATE;
								}
								*update_neighbor_it = new_neighbor;
								pit->resolve_overflow_strategy( _from );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
								debug().debug( "NeighborDiscovery - receive - Neighbor %x was updated and active for protocol %i.\n", _from, pit->get_protocol_id() );
#endif
							}
							else
							{
								events_flag = events_flag | ProtocolSettings::NEW_NB;
								if ( pit->get_neighborhood_ref()->size() == pit->get_neighborhood_ref()->max_size() )
								{
									uint8_t rs = remove_worst_neighbor( *pit );
									if ( rs == 0 )
									{
										pit->get_neighborhood_ref()->push_back( new_neighbor );
										pit->resolve_overflow_strategy( _from );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										debug().debug("NeighborDiscovery - receive - Neighbor %x was inserted and active for protocol %i.\n", _from, pit->get_protocol_id() );
										new_neighbor.print( debug(), radio() );
#endif
									}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
									else
									{
										debug().debug("NeighborDiscovery - receive - Neighbor %x was could be inserted and would be active for protocol %i.\n", _from, pit->get_protocol_id() );
										new_neighbor.print( debug(), radio() );
									}
#endif
								}
								else
								{
									pit->get_neighborhood_ref()->push_back( new_neighbor );
									pit->resolve_overflow_strategy( _from );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										debug().debug("NeighborDiscovery - receive - Neighbor %x was inserted and active for protocol %i.\n", _from, pit->get_protocol_id() );
										new_neighbor.print( debug(), radio() );
#endif
								}
							}
							uint8_t payload_found_flag = 0;
							ProtocolPayload pp;
							for ( ProtocolPayload_vector_iterator ppit = beacon.get_protocol_payloads_ref()->begin(); ppit != beacon.get_protocol_payloads_ref()->end(); ++ppit )
							{
								if ( ( ppit->get_protocol_id() == pit->get_protocol_id() ) && ( pit->get_protocol_id() != ND_PROTOCOL_ID ) )
								{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
									debug().debug("NeighborDiscovery - receive - Beacon carried a payload for protocol %i.\n", pit->get_protocol_id() );
#endif
									events_flag = events_flag | ProtocolSettings::NEW_PAYLOAD;
									pp = *ppit;
									payload_found_flag = 1;
								}
							}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
							if ( payload_found_flag == 0 )
							{
								debug().debug("NeighborDiscovery - receive - Beacon did not carry a payload for protocol %i.\n", pit->get_protocol_id() );
							}
#endif
							events_flag = pit->get_protocol_settings_ref()->get_events_flag() & events_flag;
							if ( events_flag != 0 )
							{
								pit->get_event_notifier_callback()( events_flag, _from, pp.get_payload_size(), pp.get_payload_data() );
							}
						}
						else
						{
							new_neighbor.set_active( 0 );
							if ( found_flag == 1 )
							{
								events_flag = events_flag | ProtocolSettings::LOST_NB;
								*update_neighbor_it = new_neighbor;
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
								debug().debug("NeighborDiscovery - receive - Neighbor %x was updated but inactive for protocol %i.\n", _from, pit->get_protocol_id() );
#endif
							}
							else
							{
								if (pit->get_neighborhood_ref()->size() == pit->get_neighborhood_ref()->max_size() )
								{
									uint8_t rs = remove_worst_neighbor( *pit );
									if ( rs == 0 )
									{
										pit->get_neighborhood_ref()->push_back( new_neighbor );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
										debug().debug("NeighborDiscovery - receive - Neighbor %x was inserted but inactive for protocol %i.\n", _from, pit->get_protocol_id() );
										new_neighbor.print( debug(), radio() );
#endif
									}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
									else
									{
										debug().debug("NeighborDiscovery - receive - Neighbor %x could not be inserted but would be inactive for protocol %i.\n", _from, pit->get_protocol_id() );
										new_neighbor.print( debug(), radio() );
									}
#endif
								}
								else
								{
									pit->get_neighborhood_ref()->push_back( new_neighbor );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
									debug().debug("NeighborDiscovery - receive - Neighbor %x was inserted but inactive for protocol %i.\n", _from, pit->get_protocol_id() );
									new_neighbor.print( debug(), radio() );
#endif
								}
							}
							events_flag = pit->get_protocol_settings_ref()->get_events_flag() & events_flag;
							if ( events_flag != 0 )
							{
								pit->get_event_notifier_callback()( events_flag, _from, 0, NULL );
							}
						}
					}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
					messages_received = messages_received + 1;
					bytes_received = bytes_received + _len;
					avg_bytes_size_received = bytes_received / messages_received;
					if ( beacon.get_TP() == 0 )
					{
						bytes_received_0++;
					}
					else if ( get_transmission_power_dB() == -6 )
					{
						bytes_received_6++;
					}
					else if ( get_transmission_power_dB() == -12 )
					{
						bytes_received_12++;
					}
					else if ( get_transmission_power_dB() == -18 )
					{
						bytes_received_18++;
					}
					else if ( get_transmission_power_dB() == -24 )
					{
						bytes_received_24++;
					}
					else if ( get_transmission_power_dB() == -30 )
					{
						bytes_received_30++;
					}
#endif
				}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_RECEIVE
				debug().debug("NeighborDiscovery - receive - From %x Exiting.\n", _from );
#endif
			}
		}
		// --------------------------------------------------------------------
		template<class T, void(T::*TMethod)(uint8_t, node_id_t, size_t, uint8_t*) >
		uint8_t register_protocol( uint8_t _pid, ProtocolSettings _psett, T *_obj_pnt )
		{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_REGISTER_PROTOCOL
			debug().debug("NeighborDiscovery-register_protocols %x - Entering for protocol_id = %i.\n", radio().id(), _pid );
#endif
			if ( protocols.max_size() == protocols.size() )
			{
				return PROT_LIST_FULL;
			}
			if ( ( _psett.get_protocol_payload_ref()->get_payload_size() > protocol_max_payload_size ) && ( protocol_max_payload_size_strategy == FIXED_PAYLOAD_SIZE ) )
			{
				return PAYLOAD_SIZE_OUT_OF_BOUNDS;
			}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_REGISTER_PROTOCOL
			debug().debug("NeighborDiscovery-register_protocols %x - Before protocol loop for size calculations for protocol_id = %i.\n", radio().id(), _pid );
#endif
			size_t protocol_total_payload_size = 0;
			for ( Protocol_vector_iterator it = protocols.begin(); it != protocols.end(); ++it )
			{
				protocol_total_payload_size = it->get_protocol_settings_ref()->get_protocol_payload_ref()->serial_size() + protocol_total_payload_size;
			}
			size_t neighbors_total_payload_size = 0;
			Protocol* prot_ref = get_protocol_ref( ND_PROTOCOL_ID );
			if ( prot_ref != NULL )
			{
				Neighbor_vector* n_ref = prot_ref->get_neighborhood_ref();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_REGISTER_PROTOCOL
			debug().debug("NeighborDiscovery-register_protocols %x - Before neighbor loop for size calculations for protocol_id = %i.\n", radio().id(), _pid );
#endif
				for ( Neighbor_vector_iterator it = n_ref->begin(); it != n_ref->end(); ++it )
				{
					neighbors_total_payload_size = it->serial_size() + neighbors_total_payload_size;
				}
			}
			Beacon b;
			if ( protocol_total_payload_size + neighbors_total_payload_size + b.serial_size() + sizeof(message_id_t) + sizeof(size_t) + _psett.get_protocol_payload_ref()->serial_size() > Radio::MAX_MESSAGE_LENGTH )
			{
				return NO_PAYLOAD_SPACE;
			}
			for ( Protocol_vector_iterator it = protocols.begin(); it != protocols.end(); ++it )
			{
				if ( ( it->get_protocol_id() == _pid ) || ( it->get_protocol_settings_ref()->get_protocol_payload_ref()->get_protocol_id() == _pid ) )
				{
					return PROT_NUM_IN_USE;
				}
			}
			Protocol p;
			p.set_protocol_id( _pid );
			p.set_protocol_settings( _psett );
			p.set_event_notifier_callback( event_notifier_delegate_t::template from_method<T, TMethod > ( _obj_pnt ) );
			protocols.push_back( p );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_REGISTER_PROTOCOL
			debug().debug("NeighborDiscovery-register_protocols %x - Exiting for protocol_id = %i.\n", radio().id(), _pid );
#endif
			return SUCCESS;
		}
		// --------------------------------------------------------------------
		void nd_daemon( void* user_data = NULL )
		{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
			debug().debug("NeighborDiscovery-nd_daemon %x - Entering.\n", radio().id() );
#endif
			if ( status == ACTIVE_STATUS )
			{
				time_t current_time = clock().time();
				uint32_t dead_time = 0;
				int32_t dead_time_res = 0;
				for ( Protocol_vector_iterator pit = protocols.begin(); pit != protocols.end(); ++pit )
				{
					for ( Neighbor_vector_iterator nit = pit->get_neighborhood_ref()->begin(); nit != pit->get_neighborhood_ref()->end(); ++nit )
					{
						dead_time_res = clock().seconds( current_time ) * 1000 - clock().seconds( nit->get_last_beacon() ) * 1000 + clock().milliseconds( current_time ) - clock().milliseconds( nit->get_last_beacon() );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
						if ( clock().milliseconds( current_time ) == 0 )
						{
							debug().debug( "NeighborDiscovery - nd_daemon - Clock paradox possibility from: %x - %d:%d minus %d:%d.\n", nit->get_id(), clock().seconds( current_time ), clock().seconds( nit->get_last_beacon() ), clock().milliseconds( current_time ), clock().milliseconds( nit->get_last_beacon() ) );
						}
#endif
						if ( dead_time_res < 0 )
						{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
							debug().debug( "NeighborDiscovery - nd_daemon - Clock paradox from: %x - %d:%d minus %d:%d.\n", nit->get_id(), clock().seconds( current_time ), clock().seconds( nit->get_last_beacon() ), clock().milliseconds( current_time ), clock().milliseconds( nit->get_last_beacon() ) );
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
							clock_paradox_message_drops++;
#endif
							timer().template set_timer<self_t, &self_t::nd_daemon> ( nd_daemon_period, this, 0 );
							return;
						}
						else
						{
							dead_time = clock().seconds( current_time ) * 1000 - clock().seconds( nit->get_last_beacon() ) * 1000 + clock().milliseconds( current_time ) - clock().milliseconds( nit->get_last_beacon() );
						}
						if ( ( dead_time > nit->get_beacon_period() + nit->get_beacon_period()/2 ) && ( nit->get_id() != radio().id() ) )
						{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
							debug().debug("NeighborDiscovery-nb_daemon %x - Teasing node %x.", radio().id(), nit->get_id() );
#endif
							nit->inc_total_beacons_expected( dead_time / nit->get_beacon_period() * ( pit->resolve_lost_beacon_weight( nit->get_id() ) ) );
							nit->update_link_stab_ratio();
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
							if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) /*&& ( radio().id() == 0x96f4 )*/ )
							{
								debug().debug( "LSRD1:%x:%x:%d:%d:%d:%d:[%d:%d]", radio().id(), nit->get_id(), dead_time, nit->get_beacon_period(), nit->get_total_beacons_expected(), dead_time / nit->get_beacon_period(),nit->get_trust_counter(), nit->get_trust_counter_inverse() );
							}
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
							for ( uint16_t b = 0; b < ( dead_time / nit->get_beacon_period() ); b++ )
							{
								nit->dec_trust_counter();
								nit->dec_trust_counter_inverse();
							}
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
							if ( ( pit->get_protocol_id() == ATP_PROTOCOL_ID ) /*&& ( radio().id() == 0x96f4 )*/ )
							{
								debug().debug( "LSRD2:%x:%x:%d:%d:%d:%d:[%d:%d]", radio().id(), nit->get_id(), dead_time, nit->get_beacon_period(), nit->get_total_beacons_expected(), dead_time / nit->get_beacon_period(),nit->get_trust_counter(), nit->get_trust_counter_inverse() );
							}
#endif
							nit->set_beacon_period( nit->get_beacon_period() );
							nit->set_last_beacon( current_time );
							uint8_t events_flag = 0;
							if 	(
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
									( nit->get_avg_LQI() <= pit->get_protocol_settings_ref()->get_max_avg_LQI_threshold() ) &&
									( nit->get_avg_LQI() >= pit->get_protocol_settings_ref()->get_min_avg_LQI_threshold() ) &&
									( nit->get_avg_LQI_inverse() <= pit->get_protocol_settings_ref()->get_max_avg_LQI_inverse_threshold() ) &&
									( nit->get_avg_LQI_inverse() >= pit->get_protocol_settings_ref()->get_min_avg_LQI_inverse_threshold() ) &&
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
									( nit->get_avg_RSSI() <= pit->get_protocol_settings_ref()->get_max_avg_RSSI_threshold() ) &&
									( nit->get_avg_RSSI() >= pit->get_protocol_settings_ref()->get_min_avg_RSSI_threshold() ) &&
									( nit->get_avg_RSSI_inverse() <= pit->get_protocol_settings_ref()->get_max_avg_RSSI_inverse_threshold() ) &&
									( nit->get_avg_RSSI_inverse() >= pit->get_protocol_settings_ref()->get_min_avg_RSSI_inverse_threshold() ) &&
#endif
									( nit->get_link_stab_ratio() <= pit->get_protocol_settings_ref()->get_max_link_stab_ratio_threshold() ) &&
									( nit->get_link_stab_ratio() >= pit->get_protocol_settings_ref()->get_min_link_stab_ratio_threshold() ) &&
									( nit->get_link_stab_ratio_inverse() <= pit->get_protocol_settings_ref()->get_max_link_stab_ratio_inverse_threshold() ) &&
									( nit->get_link_stab_ratio_inverse() >= pit->get_protocol_settings_ref()->get_min_link_stab_ratio_inverse_threshold() )
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_TRUST_FILTERING
							&&		( nit->get_trust_counter() >= ND_TRUST_COUNTER_THRESHOLD ) &&
									( nit->get_trust_counter_inverse() >= ND_TRUST_COUNTER_THRESHOLD_INVERSE )
#endif
								)
							{
								nit->set_active();
								events_flag = events_flag | ProtocolSettings::UPDATE_NB;
							}
							else
							{
								nit->set_active( 0 );
								events_flag = events_flag | ProtocolSettings::LOST_NB;
							}
							events_flag = pit->get_protocol_settings_ref()->get_events_flag() & events_flag;
							if ( events_flag != 0 )
							{
								pit->get_event_notifier_callback()( events_flag, nit->get_id(), 0, NULL );
							}
						}
					}
				}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
				if ( bytes_received_periodic > 6250 )
				{
					bytes_received_periodic = bytes_received - bytes_received_periodic;
					debug().debug("BR:%x:%d:%d", radio().id(), bytes_received_periodic, bytes_received );
					bytes_received_periodic = bytes_received;
				}
#endif
				timer().template set_timer<self_t, &self_t::nd_daemon> ( nd_daemon_period, this, 0 );
			}
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_DAEMON
			debug().debug("NeighborDiscovery-nd_daemon %x - Exiting.\n", radio().id() );
#endif
		}
		// --------------------------------------------------------------------
		uint8_t remove_worst_neighbor( Protocol& p_ref )
		{
			uint8_t min_link_stab_ratio = 100;
			uint8_t min_link_stab_ratio_inverse = 100;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
			uint8_t max_avg_lqi = ND_MAX_AVG_LQI_THRESHOLD;
			uint8_t max_avg_lqi_inverse = ND_MAX_AVG_LQI_INVERSE_THRESHOLD;
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
			uint8_t min_avg_rssi = ND_MIN_AVG_RSSI_THRESHOLD;
			uint8_t min_avg_rssi_inverse = ND_MIN_AVG_RSSI_INVERSE_THRESHOLD;
#endif
			Neighbor_vector_iterator mlsr		= p_ref.get_neighborhood_ref()->begin();
			Neighbor_vector_iterator mlsr_in	= p_ref.get_neighborhood_ref()->begin();
			Neighbor_vector_iterator mal		= p_ref.get_neighborhood_ref()->begin();
			Neighbor_vector_iterator mal_in		= p_ref.get_neighborhood_ref()->begin();
			Neighbor_vector_iterator mar		= p_ref.get_neighborhood_ref()->begin();
			Neighbor_vector_iterator mar_in		= p_ref.get_neighborhood_ref()->begin();

			for ( Neighbor_vector_iterator nit = p_ref.get_neighborhood_ref()->begin(); nit != p_ref.get_neighborhood_ref()->end(); ++nit )
			{
				if ( ( min_link_stab_ratio > nit->get_link_stab_ratio() ) && ( nit->get_active() == 0 ) )
				{
					mlsr = nit;
					min_link_stab_ratio = nit->get_link_stab_ratio();
				}
				if ( ( min_link_stab_ratio > nit->get_link_stab_ratio_inverse() ) && ( nit->get_active() == 0 ) )
				{
					mlsr_in = nit;
					min_link_stab_ratio_inverse = nit->get_link_stab_ratio_inverse();
				}
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
				if ( ( max_avg_lqi < nit->get_avg_LQI() ) && ( nit->get_active() == 0 ) )
				{
					mal = nit;
					max_avg_lqi = nit->get_avg_LQI();
				}
				if ( ( max_avg_lqi_inverse < nit->get_avg_LQI_inverse() ) && ( nit->get_active() == 0 ) )
				{
					mal_in = nit;
					max_avg_lqi_inverse = nit->get_avg_LQI_inverse();
				}

#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
				if ( ( min_avg_rssi > nit->get_avg_RSSI() ) && ( nit->get_active() == 0 ) )
				{
					mar = nit;
					min_avg_rssi = nit->get_avg_RSSI();
				}
				if ( ( min_avg_rssi_inverse < nit->get_avg_RSSI_inverse() ) && ( nit->get_active() == 0 ) )
				{
					mar_in = nit;
					min_avg_rssi_inverse = nit->get_avg_RSSI_inverse();
				}
#endif
			}
			if ( min_link_stab_ratio != 0 )
			{
				p_ref.get_neighborhood_ref()->erase( mlsr );
				return ProtocolSettings::NB_REMOVED;
			}
			if ( min_link_stab_ratio_inverse != 0 )
			{
				p_ref.get_neighborhood_ref()->erase( mlsr_in );
				return ProtocolSettings::NB_REMOVED;
			}
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_LQI_FILTERING
			if ( max_avg_lqi != 0 )
			{
				p_ref.get_neighborhood_ref()->erase( mal );
				return ProtocolSettings::NB_REMOVED;
			}
			if ( max_avg_lqi_inverse != 0 )
			{
				p_ref.get_neighborhood_ref()->erase( mal_in );
				return ProtocolSettings::NB_REMOVED;
			}
#endif
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_RSSI_FILTERING
			if ( min_avg_rssi != 0 )
			{
				p_ref.get_neighborhood_ref()->erase( mar );
				return ProtocolSettings::NB_REMOVED;
			}
			if ( min_avg_rssi_inverse != 0 )
			{
				p_ref.get_neighborhood_ref()->erase( mar_in );
				return ProtocolSettings::NB_REMOVED;
			}
#endif
			return 0;
		}
		// --------------------------------------------------------------------
		uint8_t register_protocol( Protocol p )
		{
			if ( protocols.max_size() == protocols.size() )
			{
				return PROT_LIST_FULL;
			}
			if ( ( p.get_protocol_settings_ref()->get_protocol_payload_ref()->serial_size() > protocol_max_payload_size ) && ( protocol_max_payload_size_strategy == FIXED_PAYLOAD_SIZE ) )
			{
				return PAYLOAD_SIZE_OUT_OF_BOUNDS;
			}
			uint8_t protocol_total_payload_size = 0;
			for ( Protocol_vector_iterator it = protocols.begin(); it != protocols.end(); ++it )
			{
				protocol_total_payload_size = it->get_protocol_settings_ref()->get_protocol_payload_ref()->serial_size() + protocol_total_payload_size;
			}
			size_t neighbors_total_payload_size = 0;
			Neighbor_vector* n_ref = get_protocol_ref( ND_PROTOCOL_ID )->get_neighborhood_ref();
			for ( Neighbor_vector_iterator it = n_ref->begin(); it != n_ref->end(); ++it )
			{
				neighbors_total_payload_size = it->serial_size() + neighbors_total_payload_size;
			}
			Beacon b;
			if ( protocol_total_payload_size + neighbors_total_payload_size + b.serial_size() + sizeof(message_id_t) + sizeof(size_t) + p.get_protocol_settings_ref()->get_protocol_payload_ref()->serial_size() > Radio::MAX_MESSAGE_LENGTH )
			{
				return NO_PAYLOAD_SPACE;
			}
			for ( Protocol_vector_iterator it = protocols.begin(); it != protocols.end(); ++it )
			{
				if (	( it->get_protocol_id() == p.get_protocol_id() ) ||
						( it->get_protocol_id() == p.get_protocol_settings_ref()->get_protocol_payload_ref()->get_protocol_id() ) ||
						( it->get_protocol_settings_ref()->get_protocol_payload_ref()->get_protocol_id() == p.get_protocol_id() ) ||
						( it->get_protocol_settings_ref()->get_protocol_payload_ref()->get_protocol_id() == p.get_protocol_settings_ref()->get_protocol_payload_ref()->get_protocol_id() )
					)
				{
					return PROT_NUM_IN_USE;
				}
			}
			protocols.push_back( p );
			return SUCCESS;
		}
		// --------------------------------------------------------------------
		uint8_t unregister_protocol( uint8_t _pid )
		{
			for ( Protocol_vector_iterator it = protocols.begin(); it != protocols.end(); ++it )
			{
				if ( it->get_protocol_id() == _pid )
				{
					protocols.erase( it );
					return SUCCESS;
				}
			}
			return INV_PROT_ID;
		}
		// --------------------------------------------------------------------
		Protocol* get_protocol_ref( uint8_t _pid )
		{
			for ( Protocol_vector_iterator it = protocols.begin(); it != protocols.end(); ++it )
			{
				if ( it->get_protocol_id() == _pid )
				{
					return &(*it);
				}
			}
			return NULL;
		}
		// --------------------------------------------------------------------
		Protocol_vector* get_protocols_ref()
		{
			return &protocols;
		}
		// --------------------------------------------------------------------
		uint8_t get_status()
		{
			return status;
		}
		// --------------------------------------------------------------------
		void set_status( uint8_t _st )
		{
			status = _st;
		}
		// --------------------------------------------------------------------
		millis_t get_beacon_period()
		{
			return beacon_period;
		}
		// --------------------------------------------------------------------
		void set_beacon_period( millis_t _bp )
		{
			millis_t old_beacon_period = beacon_period;
			beacon_period =_bp;
			if ( old_beacon_period != beacon_period )
			{
				for ( Protocol_vector_iterator pit = protocols.begin(); pit != protocols.end(); ++pit )
				{
					uint8_t events_flag = 0;
					events_flag = events_flag | ProtocolSettings::BEACON_PERIOD_UPDATE;
					events_flag = pit->get_protocol_settings_ref()->get_events_flag() & events_flag;
					if ( events_flag != 0 )
					{
						pit->get_event_notifier_callback()( events_flag, radio().id(), 0, NULL );
					}
				}
			}
		}
		// --------------------------------------------------------------------
		int8_t get_transmission_power_dB()
		{
			return transmission_power_dB;
		}
		// --------------------------------------------------------------------
		void set_transmission_power_dB( int8_t _tp_dB )
		{
			int8_t old_transmission_power_dB = transmission_power_dB;
			transmission_power_dB = _tp_dB;
			if ( old_transmission_power_dB != transmission_power_dB )
			{
				for ( Protocol_vector_iterator pit = protocols.begin(); pit != protocols.end(); ++pit )
				{
					uint8_t events_flag = 0;
					events_flag = events_flag | ProtocolSettings::TRANS_DB_UPDATE;
					events_flag = pit->get_protocol_settings_ref()->get_events_flag() & events_flag;
					if ( events_flag != 0 )
					{
						pit->get_event_notifier_callback()(events_flag, radio().id(), 0, NULL );
					}
				}
			}
		}
		// --------------------------------------------------------------------
		uint8_t get_protocol_max_payload_size()
		{
			return protocol_max_payload_size;
		}
		// --------------------------------------------------------------------
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ACTIVE_CONNECTIVITY_FILTERING
		void q_sort_neigh_active_con( int8_t _left, int8_t _right, Neighbor_vector& _neighborhood )
		{
			int8_t i = _left;
			int8_t j = _right;
			Neighbor tmp;
			Neighbor pivot = _neighborhood[ ( _left + _right ) / 2 ];
			while (i <= j)
			{
				while ( _neighborhood[i].get_active_connectivity() < pivot.get_active_connectivity() )
				{
					i++;
				}
				while ( _neighborhood[j].get_active_connectivity() > pivot.get_active_connectivity() )
				{
					j--;
				}
				if ( i <= j )
				{
					tmp = _neighborhood[i];
					_neighborhood[i] = _neighborhood[j];
					_neighborhood[j] = tmp;
					i++;
					j--;
				}
			};
			if ( _left < j )
			{
				q_sort_neigh_active_con( _left, j, _neighborhood );
			}
			if ( i < _right )
			{
				q_sort_neigh_active_con( i, _right, _neighborhood);
			}
		}
#endif
		// --------------------------------------------------------------------
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS_DAEMON
		void nd_metrics_daemon( void* user_data = NULL )
		{
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_METRICS_DAEMON
			debug().debug("NeighborDiscovery - nd_metrics_daemon - Entering.\n" );
#endif
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_METRICS_DAEMON
			debug().debug("NeighborDiscovery - nd_metrics_daemon - Breakpoint 1.\n" );
#endif
			Protocol* p;
			p = get_protocol_ref( ND_PROTOCOL_ID );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_METRICS_DAEMON
			debug().debug("NeighborDiscovery - nd_metrics_daemon - Breakpoint 2.\n" );
#endif

#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
			debug().debug( "AGGR:%x:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",

#else
			debug().debug( "AGGR:%x:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
#endif

										radio().id(),
										metrics_counter,
										p->get_neighborhood_ref()->size() - 1,
										p->get_neighborhood_active_size() - 1,
										messages_received,
										bytes_received,
										avg_bytes_size_received,
										messages_send,
										bytes_send,
										avg_bytes_size_send,
										corrupted_messages_received,
										corrupted_bytes_received,
										avg_corrupted_byte_size_received,
										clock_paradox_message_drops
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
										,position.get_x()
										,position.get_y()
#endif
                                                                                );
#ifdef DEBUG_NEIGHBOR_DISCOVERY_H_ND_METRICS_DAEMON
			debug().debug("NeighborDiscovery - nd_metrics_daemon - Exiting.\n" );
#endif
			metrics_counter++;
			timer().template set_timer<self_t, &self_t::nd_metrics_daemon> ( ND_STATS_DURATION, this, 0 );
		}
#endif
		// --------------------------------------------------------------------
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
		void set_position( PositionNumber _x, PositionNumber _y, PositionNumber _z )
		{
			position = Position( _x, _y, _z);
		}
		// --------------------------------------------------------------------
		Position get_position()
		{
			return position;
		}
#endif
		// --------------------------------------------------------------------
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
		uint32_t get_bytes_received()
		{
			return bytes_received;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send()
		{
			return bytes_send;
		}
		// --------------------------------------------------------------------
		uint32_t get_messages_received()
		{
			return messages_received;
		}
		// --------------------------------------------------------------------
		uint32_t get_messages_send()
		{
			return messages_send;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send_0()
		{
			return bytes_send_0;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send_6()
		{
			return bytes_send_6;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send_12()
		{
			return bytes_send_12;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send_18()
		{
			return bytes_send_18;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send_24()
		{
			return bytes_send_24;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_send_30()
		{
			return bytes_send_30;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_received_0()
		{
			return bytes_received_0;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_received_6()
		{
			return bytes_received_6;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_received_12()
		{
			return bytes_received_12;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_received_18()
		{
			return bytes_received_18;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_received_24()
		{
			return bytes_received_24;
		}
		// --------------------------------------------------------------------
		uint32_t get_bytes_received_30()
		{
			return bytes_received_30;
		}
		// --------------------------------------------------------------------
		void set_bytes_send_0( uint32_t _bs0 )
		{
			bytes_send_0 = _bs0;
		}
		// --------------------------------------------------------------------
		void set_bytes_send_6( uint32_t _bs6 )
		{
			bytes_send_6 = _bs6;
		}
		// --------------------------------------------------------------------
		void set_bytes_send_12( uint32_t _bs12 )
		{
			bytes_send_12 = _bs12;
		}
		// --------------------------------------------------------------------
		void set_bytes_send_18( uint32_t _bs18 )
		{
			bytes_send_18 = _bs18;
		}
		// --------------------------------------------------------------------
		void set_bytes_send_24( uint32_t _bs24 )
		{
			bytes_send_24 = _bs24;
		}
		// --------------------------------------------------------------------
		void set_bytes_send_30( uint32_t _bs30 )
		{
			bytes_send_30 = _bs30;
		}
		// --------------------------------------------------------------------
		void set_bytes_received_0( uint32_t _br0 )
		{
			bytes_received_0 = _br0;
		}
		// --------------------------------------------------------------------
		void set_bytes_received_6( uint32_t _br6)
		{
			bytes_received_6 = _br6;
		}
		// --------------------------------------------------------------------
		void set_bytes_received_12(uint32_t _br12)
		{
			bytes_received_12 = _br12;
		}
		// --------------------------------------------------------------------
		void set_bytes_received_18(uint32_t _br18)
		{
			bytes_received_18 = _br18;
		}
		// --------------------------------------------------------------------
		void set_bytes_received_24(uint32_t _br24)
		{
			bytes_received_24 = _br24;
		}
		// --------------------------------------------------------------------
		void set_bytes_received_30(uint32_t _br30)
		{
			bytes_received_30 = _br30;
		}
		// --------------------------------------------------------------------
#endif
		// --------------------------------------------------------------------
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
		void init( Radio& _radio, Timer& _timer, Debug& _debug, Clock& _clock, Rand& _rand, iSenseBatterySensor& _iSBS )
#else
		void init( Radio& _radio, Timer& _timer, Debug& _debug, Clock& _clock, Rand& _rand )
#endif
		{
			radio_ = &_radio;
			timer_ = &_timer;
			debug_ = &_debug;
			clock_ = &_clock;
			rand_ = &_rand;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
			iSBS = &_iSBS;
#endif
		}
		// --------------------------------------------------------------------
		Radio& radio()
		{
			return *radio_;
		}
		// --------------------------------------------------------------------
		Clock& clock()
		{
			return *clock_;
		}
		// --------------------------------------------------------------------
		Timer& timer()
		{
			return *timer_;
		}
		// --------------------------------------------------------------------
		Debug& debug()
		{
			return *debug_;
		}
		// --------------------------------------------------------------------
		Rand& rand()
		{
			return *rand_;
		}
		// --------------------------------------------------------------------
		enum error_codes
		{
			SUCCESS,
			PROT_NUM_IN_USE,
			PROT_LIST_FULL,
			INV_PROT_ID,
			NO_PAYLOAD_SPACE,
			PAYLOAD_SIZE_OUT_OF_BOUNDS,
			ERROR_CODES_NUM_VALUES
		};
		enum neighbor_discovery_status
		{
			ACTIVE_STATUS,
			WAITING_STATUS,
			ND_STATUS_NUM_VALUES
		};
		enum protocol_payload_strategies
		{
			FIXED_PAYLOAD_SIZE,
			DYNAMIC_PAYLOAD_SIZE,
			PP_STRATEGY_NUM_VALUES
		};
		enum protocol_ids
		{
			ND_PROTOCOL_ID,
			TRACKING_PROTOCOL_ID,
			ATP_PROTOCOL_ID
		};
		enum message_ids
		{
			ND_MESSAGE = 12
		};
	private:
		uint32_t recv_callback_id_;
		uint8_t status;
		millis_t beacon_period;
		int8_t transmission_power_dB;
		Protocol_vector protocols;
		size_t protocol_max_payload_size;
		uint8_t protocol_max_payload_size_strategy;
		millis_t nd_daemon_period;
#ifdef DEBUG_NEIGHBOR_DISCOVERY_STATS
		uint32_t messages_received;
		uint32_t bytes_received;
		uint32_t bytes_received_0;
		uint32_t bytes_received_6;
		uint32_t bytes_received_12;
		uint32_t bytes_received_18;
		uint32_t bytes_received_24;
		uint32_t bytes_received_30;
		uint32_t avg_bytes_size_received;
		uint32_t messages_send;
		uint32_t bytes_send;
		uint32_t bytes_send_0;
		uint32_t bytes_send_6;
		uint32_t bytes_send_12;
		uint32_t bytes_send_18;
		uint32_t bytes_send_24;
		uint32_t bytes_send_30;
		uint32_t avg_bytes_size_send;
		uint32_t corrupted_messages_received;
		uint32_t corrupted_bytes_received;
		uint32_t avg_corrupted_byte_size_received;
		uint32_t clock_paradox_message_drops;
		uint32_t bytes_received_periodic;
#endif

#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_COORD_SUPPORT
		Position position;
#endif
        Radio * radio_;
        Clock * clock_;
        Timer * timer_;
        Debug * debug_;
        Rand * rand_;
        uint32_t metrics_counter;
#ifdef CONFIG_NEIGHBOR_DISCOVERY_H_ENERGY
        iSenseBatterySensor * iSBS;
        uint32_t energy_counter1;
        uint32_t energy_counter2;
#endif
    };
}
#endif
