#include "catch/catch.hpp"

#include "game.h"
#include "map.h"
#include "map_iterator.h"
#include "vehicle.h"
#include "veh_type.h"
#include "itype.h"
#include "player.h"
#include "cata_utility.h"
#include "options.h"
#include "test_statistics.h"

void clear_game( const ter_id &terrain )
{
    while( g->num_zombies() > 0 ) {
        g->remove_zombie( 0 );
    }

    g->unload_npcs();

    // Move player somewhere safe
    g->u.setpos( tripoint( 0, 0, 0 ) );

    for( const tripoint &p : g->m.points_in_rectangle( tripoint( 0, 0, 0 ),
                                                       tripoint( MAPSIZE * SEEX, MAPSIZE * SEEY, 0 ) ) ) {
        g->m.furn_set( p, furn_id( "f_null" ) );
        g->m.ter_set( p, terrain );
        g->m.trap_set( p, trap_id( "tr_null" ) );
        g->m.i_clear( p );
    }

    for( wrapped_vehicle &veh : g->m.get_vehicles( tripoint( 0, 0, 0 ), tripoint( MAPSIZE * SEEX, MAPSIZE * SEEY, 0 ) ) ) {
        g->m.destroy_vehicle( veh.v );
    }

    g->m.build_map_cache( 0, true );
}

long test_efficiency( const vproto_id &veh_id, const ter_id &terrain, int reset_velocity_turn, long min_dist, long max_dist )
{
    clear_game( terrain );

    const tripoint starting_point( 60, 60, 0 );
    vehicle *veh_ptr = g->m.add_vehicle( veh_id, starting_point, -90, 5, 0 );
    REQUIRE( veh_ptr != nullptr );
    if( veh_ptr == nullptr ) {
        return 0;
    }

    vehicle &veh = *veh_ptr;
    veh.tags.insert( "IN_CONTROL_OVERRIDE" );
    veh.engine_on = true;
    REQUIRE( veh.safe_velocity() > 0 );
    veh.cruise_velocity = veh.safe_velocity();
    int reset_counter = 0;
    long tiles_travelled = 0;
    int turn_count = 0;
    while( veh.engine_on && veh.safe_velocity() > 0 ) {
        g->m.vehmove();
        veh.idle( true );
        // How much it moved
        tiles_travelled += square_dist( starting_point, veh.global_pos3() );
        // Bring it back to starting point to prevent it from leaving the map
        const tripoint displacement = starting_point - veh.global_pos3();
        tripoint veh_pos = veh.global_pos3();
        g->m.displace_vehicle( veh_pos, displacement );
        if( reset_velocity_turn < 0 ) {
            continue;
        }

        reset_counter++;
        if( reset_counter > reset_velocity_turn ) {
            veh.velocity = 0;
            veh.last_turn = 0;
            veh.of_turn_carry = 0;
            reset_counter = 0;
        }
    }

    CHECK( tiles_travelled >= min_dist );
    CHECK( tiles_travelled <= max_dist );
    return tiles_travelled;
}

void find_inner( std::string type, std::string terrain, int delay ) {
    statistics efficiency;
    for( int i = 0; i < 10; i++) {
        efficiency.add( test_efficiency( vproto_id( type ), ter_id( terrain ), delay, 0, 0 ) );
    }
    printf( "Testing %s on %s with %s: Min %d, Max %d, Midpoint %f.\n",
	    type.c_str(), terrain.c_str(), (delay < 0) ? "no resets" : "resets every 5 turns",
	    efficiency.min(), efficiency.max(), ( efficiency.min() + efficiency.max() ) / 2.0 );
}

void find_efficiency( std::string type ) {
    SECTION( "finding efficiency of " + type ) {
        find_inner( type, "t_pavement", -1 );
        find_inner( type, "t_dirt", -1 );
        find_inner( type, "t_pavement", 5 );
        find_inner( type, "t_dirt", 5 );
    }
}

void test_vehicle( std::string type, long pavement_target, long dirt_target, long pavement_target_w_stops, long dirt_target_w_stops ) {
    SECTION( type + " on pavement" ) {
      for( int i = 0; i < 10; i++)
        test_efficiency( vproto_id( type ), ter_id( "t_pavement" ), -1, pavement_target * 0.8, pavement_target * 1.2 );
    }
    SECTION( type + " on dirt" ) {
      for( int i = 0; i < 10; i++)
        test_efficiency( vproto_id( type ), ter_id( "t_dirt" ), -1, dirt_target * 0.9, dirt_target * 1.1 );
    }
    SECTION( type + " on pavement, full stop every 5 turns" ) {
      for( int i = 0; i < 10; i++)
        test_efficiency( vproto_id( type ), ter_id( "t_pavement" ), 5, pavement_target_w_stops * 0.9, pavement_target_w_stops * 1.1 );
    }
    SECTION( type + " on dirt, full stop every 5 turns" ) {
      for( int i = 0; i < 10; i++)
        test_efficiency( vproto_id( type ), ter_id( "t_dirt" ), 5, dirt_target_w_stops * 0.9, dirt_target_w_stops * 1.1 );
    }
}

TEST_CASE( "vehicle_efficiency", "[vehicle] [engine]" ) {
  //test_vehicle( "beetle", 48518, 44531, 2372, 2175 );
  //test_vehicle( "car", 48000, 31000, 2375, 1575 );
  //test_vehicle( "car_sports", 51400, 31000, 2465, 1478 );
  // Electric car seems to spawn with no charge.
  //test_vehicle( "electric_car", 300 );
  //test_vehicle( "suv", 99000, 56500, 4950, 2725 );
  //test_vehicle( "motorcycle", 7400, 4100, 575, 330 );
  //test_vehicle( "quad_bike", 6600, 4500, 500, 330 );
  //test_vehicle( "scooter", 6550, 6900, 550, 550 );
  //test_vehicle( "superbike", 8200, 4167, 600, 300 );
  //test_vehicle( "ambulance", 95000, 86000, 3650, 3200 );
  //test_vehicle( "fire_engine", 97000, 92000, 4120, 3820 );
  find_efficiency( "apc" );
  find_efficiency( "humvee" );
  test_vehicle( "fire_truck", 77077, 16972, 3201, 763 );
  test_vehicle( "truck_swat", 80451, 11170, 3470, 745 );
  test_vehicle( "tractor_plow", 55923, 56126, 2351, 2349 );
  test_vehicle( "aapc-mg", 320207, 292725, 11843, 10152 );
  //test_vehicle( "apc", 82000, 16000, 3200, 800 );
  //test_vehicle( "humvee", 82000, 16000, 3200, 800 );
}
