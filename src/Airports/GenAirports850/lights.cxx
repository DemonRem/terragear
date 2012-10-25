// lights.cxx -- Generate runway lighting
//
// Written by Curtis Olson, started February 2002.
//
// Copyright (C) 2002  Curtis L. Olson  - http://www.flightgear.org/~curt
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdlib>

#include <simgear/math/SGMath.hxx>
#include <simgear/debug/logstream.hxx>

#include "runway.hxx"

using std::string;

// calculate the runway light direction vector.  We take both runway
// ends to get the direction of the runway.
Point3D Runway::gen_runway_light_vector( float angle, bool recip ) {
    SGVec3f cart1, cart2;
    if ( !recip ) {
        cart1 = normalize(SGVec3f::fromGeod(GetStart()));
        cart2 = normalize(SGVec3f::fromGeod(GetEnd()));
    } else {
        cart2 = normalize(SGVec3f::fromGeod(GetStart()));
        cart1 = normalize(SGVec3f::fromGeod(GetEnd()));
    }

    SGVec3f runway_vec = normalize(cart1 - cart2);
    SGVec3f horizontal(normalize(cross(cart1, runway_vec)));
    SGQuatf rotation = SGQuatf::fromAngleAxisDeg(angle, horizontal);
    SGVec3f light_vec = rotation.transform(runway_vec);

    return Point3D::fromSGVec3(light_vec);
}

// generate runway edge lighting
// 60 meters spacing or the next number down that divides evenly.
superpoly_list Runway::gen_runway_edge_lights( bool recip )
{
    point_list r_lights; r_lights.clear();
    point_list w_lights; w_lights.clear();
    point_list y_lights; y_lights.clear();
    point_list r_normals; r_normals.clear();
    point_list w_normals; w_normals.clear();
    point_list y_normals; y_normals.clear();

    int i;
    double length_hdg;
    double dist = rwy.length - rwy.threshold[0] - rwy.threshold[1];
    int divs = (int)(dist / 60.0) + 1;
    double step = dist / divs;
    SGGeod pt1, pt2;

    Point3D normal = gen_runway_light_vector( 3.0, recip );

    if ( recip ) {
        length_hdg = rwy.heading + 180.0;
        if ( length_hdg > 360.0 ) { length_hdg -= 360.0; }
        pt1 = SGGeodesy::direct(GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)]);
        pt2 = GetStart();
    } else {
        length_hdg = rwy.heading;
        pt1 = SGGeodesy::direct(GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)]);
        pt2 = GetEnd();
    }
    double left_hdg = length_hdg - 90.0;
    if ( left_hdg < 0 ) { left_hdg += 360.0; }

    int tstep;
    double offset = 2 + rwy.width * 0.5;

    //front threshold
    if (rwy.threshold[get_thresh0(recip)] > step )
    {
        SGGeod pt0 = pt1;
        tstep = (int)(rwy.threshold[get_thresh0(recip)] / step);
        for ( i = 0; i < tstep; ++i ) {
            pt0 = SGGeodesy::direct(pt0, length_hdg, -step);
            r_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt0, left_hdg, offset)) );
            r_normals.push_back( normal );
            r_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt0, left_hdg, -offset)) );
            r_normals.push_back( normal );
        }
    }

    for ( i = 0; i < divs; ++i ) {
        pt1 = SGGeodesy::direct(pt1, SGGeodesy::courseDeg(pt1, pt2), step);
        dist -= step;
        if ( dist > 610.0 || dist > rwy.length / 2 ) {
            w_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt1, left_hdg, offset)) );
            w_normals.push_back( normal );
            w_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt1, left_hdg, -offset)) );
            w_normals.push_back( normal );
        } else if (dist > 5.0) {
            y_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt1, left_hdg, offset)) );
            y_normals.push_back( normal );
            y_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt1, left_hdg, -offset)) );
            y_normals.push_back( normal );
        }
    }

    //back threshold
    if (rwy.threshold[get_thresh1(recip)] > step )
    {
        tstep = (int)(rwy.threshold[get_thresh1(recip)] / step);
        for ( i = 0; i < tstep; ++i ) {
            y_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt1, left_hdg, offset)) );
            y_normals.push_back( normal );
            y_lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct(pt1, left_hdg, -offset)) );
            y_normals.push_back( normal );
            pt1 = SGGeodesy::direct(pt1, length_hdg, step);
        }
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( w_lights, false );
    normals_poly.add_contour( w_normals, false );

    TGSuperPoly white;
    white.set_poly( lights_poly );
    white.set_normals( normals_poly );
    //Different intensities
    if (rwy.edge_lights == 3)
        white.set_material( "RWY_WHITE_LIGHTS" );
    else if (rwy.edge_lights == 2)
        white.set_material( "RWY_WHITE_MEDIUM_LIGHTS" );
    else if (rwy.edge_lights == 1)
        white.set_material( "RWY_WHITE_LOW_LIGHTS" );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( y_lights, false );
    normals_poly.add_contour( y_normals, false );

    TGSuperPoly yellow;
    yellow.set_poly( lights_poly );
    yellow.set_normals( normals_poly );
    //Different intensities
    if (rwy.edge_lights == 3)
        yellow.set_material( "RWY_YELLOW_LIGHTS" );
    else if (rwy.edge_lights == 2)
        yellow.set_material( "RWY_YELLOW_MEDIUM_LIGHTS" );
    else if (rwy.edge_lights == 1)
        yellow.set_material( "RWY_YELLOW_LOW_LIGHTS" );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( r_lights, false );
    normals_poly.add_contour( r_normals, false );

    TGSuperPoly red;
    red.set_poly( lights_poly );
    red.set_normals( normals_poly );
    //Different intensities
    if (rwy.edge_lights == 3)
        red.set_material( "RWY_RED_LIGHTS" );
    else if (rwy.edge_lights == 2)
        red.set_material( "RWY_RED_MEDIUM_LIGHTS" );
    else if (rwy.edge_lights == 1)
        red.set_material( "RWY_RED_LOW_LIGHTS" );

    superpoly_list result; result.clear();

    result.push_back( white );
    result.push_back( yellow );
    result.push_back( red );

    return result;
}

// generate threshold lights for displaced/normal runways and with/without light bars
superpoly_list Runway::gen_runway_threshold_lights( const int kind, bool recip )
{
    point_list g_lights; g_lights.clear();
    point_list g_normals; g_normals.clear();
    point_list r_lights; r_lights.clear();
    point_list r_normals; r_normals.clear();
    int i;

    // determine the start point.
    SGGeod ref1, ref2;
    double length_hdg;

    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref1 = SGGeodesy::direct(GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)] - 1);
        ref2 = GetEnd();
    } else {
        length_hdg = rwy.heading;
        ref1 = SGGeodesy::direct(GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)] - 1);
        ref2 = GetStart();
    }

    Point3D normal1 = gen_runway_light_vector( 3.0, recip );
    Point3D normal2 = gen_runway_light_vector( 3.0, !recip );

    double left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);
    int divs = (int)(rwy.width + 4) / 3.0;
    double step = (rwy.width + 4) / divs;
    SGGeod pt1, pt2;
    double offset = 2 + rwy.width * 0.5;

    if ( GetsThreshold(recip) ) {
        SGGeod thresh1 = pt1 = SGGeodesy::direct(ref1, left_hdg, offset);
        SGGeod thresh2 = SGGeodesy::direct(ref1, left_hdg, -offset);
        // four lights for each side
        for ( i = 0; i < 4; ++i ) {
            g_lights.push_back( Point3D::fromSGGeod(thresh1) );
            g_normals.push_back( normal1 );

            g_lights.push_back( Point3D::fromSGGeod(thresh2) );
            g_normals.push_back( normal1 );

            // offset 3m towards outside
            thresh1 = SGGeodesy::direct(thresh1, left_hdg, 3);
            thresh2 = SGGeodesy::direct(thresh2, left_hdg, -3);
        }
    } else {
        pt1 = SGGeodesy::direct(ref2, left_hdg, offset);
    }

    if ( kind ) {
        // Add a green and red threshold lights bar
        SGGeod redbar = SGGeodesy::direct(ref2, left_hdg, offset);
        for ( i = 0; i < divs + 1; ++i ) {
            g_lights.push_back( Point3D::fromSGGeod(pt1) );
            g_normals.push_back( normal1 );
            r_lights.push_back( Point3D::fromSGGeod(redbar) );
            r_normals.push_back( normal2 );
            pt1 = SGGeodesy::direct(pt1, left_hdg, -step);
            redbar = SGGeodesy::direct(redbar, left_hdg, -step);
        }
    }

    // Now create the lights at the front of the runway
    pt1 = SGGeodesy::direct(ref2, left_hdg, offset);
    pt2 = SGGeodesy::direct(ref2, left_hdg, -offset);

    point_list tmp; tmp.clear();
    point_list tmp_norm; tmp_norm.clear();

    // Create groups of four lights in front of the displaced threshold
    for ( i = 0; i < 4; ++i ) {
        tmp.push_back( Point3D::fromSGGeod(pt1) );
        tmp_norm.push_back( normal1 );
        tmp.push_back( Point3D::fromSGGeod(pt2) );
        tmp_norm.push_back( normal1 );

        if (!kind) {
            r_lights.push_back( Point3D::fromSGGeod(pt1) );
            r_normals.push_back( normal2 );
            r_lights.push_back( Point3D::fromSGGeod(pt2) );
            r_normals.push_back( normal2 );
        }

        // offset 3m towards center
        pt1 = SGGeodesy::direct(pt1, left_hdg, -3);
        pt2 = SGGeodesy::direct(pt2, left_hdg, 3);
    }

    if (GetsThreshold(recip) ) {
        r_lights.insert(r_lights.end(), tmp.begin(), tmp.end());
        r_normals.insert(r_normals.end(), tmp_norm.begin(), tmp_norm.end());
    } else if (!kind) {
        g_lights.insert(g_lights.end(), tmp.begin(), tmp.end());
        g_normals.insert(g_normals.end(), tmp_norm.begin(), tmp_norm.end());
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( g_lights, false );
    normals_poly.add_contour( g_normals, false );

    TGSuperPoly green;
    green.set_poly( lights_poly );
    green.set_normals( normals_poly );
    green.set_material( "RWY_GREEN_LIGHTS" );
    /* other intensities for when the data file supports it
     *
     * green.set_material( "RWY_GREEN_MEDIUM_LIGHTS" );
     * green.set_material( "RWY_GREEN_LOW_LIGHTS" );
     */

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( r_lights, false );
    normals_poly.add_contour( r_normals, false );

    TGSuperPoly red;
    red.set_poly( lights_poly );
    red.set_normals( normals_poly );
    red.set_material( "RWY_RED_LIGHTS" );
    /* other intensities for when the data file supports it
     *
     * red.set_material( "RWY_RED_MEDIUM_LIGHTS" );
     * red.set_material( "RWY_RED_LOW_LIGHTS" );
     */

    superpoly_list result; result.clear();

    result.push_back( green );
    result.push_back( red );

    return result;
}


// generate runway center line lighting, 15m spacing.
superpoly_list Runway::gen_runway_center_line_lights( bool recip )
{
    point_list w_lights; w_lights.clear();
    point_list r_lights; r_lights.clear();
    point_list w_normals; w_normals.clear();
    point_list r_normals; r_normals.clear();

    Point3D normal = gen_runway_light_vector( 3.0, recip );

    SGGeod pt1, pt2;
    double length_hdg;

    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        pt1 = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)]);
        pt2 = GetStart();
    } else {
        length_hdg = rwy.heading;
        pt1 = SGGeodesy::direct(GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)]);
        pt2 = GetEnd();
    }

    double dist = SGGeodesy::distanceM(pt1, pt2);
    int divs = (int)(dist / 15.0) + 1;
    double step = dist / divs;
    bool use_white = true;

    while ( dist > 0.0 ) {
        if ( dist > 900.0 ) {
            w_lights.push_back( Point3D::fromSGGeod(pt1) );
            w_normals.push_back( normal );
        } else if ( dist > 300.0 ) {
            if ( use_white ) {
                w_lights.push_back( Point3D::fromSGGeod(pt1) );
                w_normals.push_back( normal );
            } else {
                r_lights.push_back( Point3D::fromSGGeod(pt1) );
                r_normals.push_back( normal );
            }
            use_white = !use_white;
        } else {
            r_lights.push_back( Point3D::fromSGGeod(pt1) );
            r_normals.push_back( normal );
        }
        length_hdg = SGGeodesy::courseDeg(pt1, pt2);
        pt1 = SGGeodesy::direct(pt1, length_hdg, step);
        dist -= step;
    }

    superpoly_list result; result.clear();

    if ( w_lights.size() ) {
        TGPolygon lights_poly; lights_poly.erase();
        TGPolygon normals_poly; normals_poly.erase();
        lights_poly.add_contour( w_lights, false );
        normals_poly.add_contour( w_normals, false );

        TGSuperPoly white;
        white.set_poly( lights_poly );
        white.set_normals( normals_poly );
        white.set_material( "RWY_WHITE_MEDIUM_LIGHTS" );

        result.push_back( white );
    }

    if ( r_lights.size() ) {
        TGPolygon lights_poly; lights_poly.erase();
        TGPolygon normals_poly; normals_poly.erase();
        lights_poly.add_contour( r_lights, false );
        normals_poly.add_contour( r_normals, false );

        TGSuperPoly red;
        red.set_poly( lights_poly );
        red.set_normals( normals_poly );
        red.set_material( "RWY_RED_MEDIUM_LIGHTS" );

        result.push_back( red );
    }

    return result;
}

/*
// generate taxiway center line lighting, 50' spacing.
static superpoly_list gen_taxiway_center_line_lights( bool recip )
{
    point_list g_lights; g_lights.clear();
    point_list g_normals; g_normals.clear();
    int i;

    double len = rwy.length;
    // this should be ??' technically but I'm trying 50' to space things out
    int divs = (int)(len / 70) + 1;

    Point3D normal = gen_runway_light_vector( 3.0, recip );

    // using TGPolygon is a bit innefficient, but that's what the
    // routine returns.
    TGPolygon poly_corners = gen_runway_area_w_extend( 2.0, 0.0, 0.0, 2.0 );

    point_list corner;
    for ( i = 0; i < poly_corners.contour_size( 0 ); ++i ) {
	corner.push_back( poly_corners.get_pt( 0, i ) );
    }

    Point3D inc;
    Point3D pt1, pt2;

    if ( recip ) {
        pt1 = (corner[0] + corner[1] ) / 2.0;
        pt2 = (corner[2] + corner[3] ) / 2.0;
    } else {
        pt1 = (corner[2] + corner[3] ) / 2.0;
        pt2 = (corner[0] + corner[1] ) / 2.0;
    }
    inc = (pt2 - pt1) / divs;

    double dist = len;
    double step = len / divs;
    pt1 += inc;                 // move 25' in
    dist -= step;

    while ( dist > 0.0 ) {
        g_lights.push_back( pt1 );
        g_normals.push_back( normal );

  	pt1 += inc;
  	pt1 += inc;
        dist -= step;
        dist -= step;
    }

    superpoly_list result; result.clear();

    if ( g_lights.size() > 0 ) {
        TGPolygon lights_poly; lights_poly.erase();
        TGPolygon normals_poly; normals_poly.erase();
        lights_poly.add_contour( g_lights, false );
        normals_poly.add_contour( g_normals, false );

        TGSuperPoly green;
        green.set_poly( lights_poly );
        green.set_normals( normals_poly );
        green.set_material( "RWY_GREEN_TAXIWAY_LIGHTS" );

        result.push_back( green );
    }

    return result;
}
*/

// generate touch down zone lights
TGSuperPoly Runway::gen_touchdown_zone_lights( bool recip )
{
    point_list lights; lights.clear();
    point_list normals; normals.clear();

    Point3D normal = gen_runway_light_vector( 3.0, recip );

    // determine the start point.
    SGGeod ref;
    double length_hdg, left_hdg;
    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)]);
    } else {
        length_hdg = rwy.heading;
        ref = SGGeodesy::direct(GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)]);
    }
    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);

    // calculate amount of touchdown light rows.
    // They should cover a distance of 900m or
    // half the runway length, whichever comes first. Spacing is 30m.
    int rows = (int)(rwy.length * 0.5) / 30;
    if (rows > 30) rows = 30;

    for ( int i = 0; i < rows; ++i ) {
        // offset 30m upwind
        ref = SGGeodesy::direct( ref, length_hdg, 30 );
        SGGeod pt1 = ref;

        // left side bar
        pt1 = SGGeodesy::direct( pt1, left_hdg, 11 );
        lights.push_back( Point3D::fromSGGeod(pt1) );
        normals.push_back( normal );
    
        pt1 = SGGeodesy::direct( pt1, left_hdg, 1.5 );
        lights.push_back( Point3D::fromSGGeod(pt1) );
        normals.push_back( normal );

        pt1 = SGGeodesy::direct( pt1, left_hdg, 1.5 );
        lights.push_back( Point3D::fromSGGeod(pt1) );
        normals.push_back( normal );

        pt1 = ref;

        // right side bar
        pt1 = SGGeodesy::direct( pt1, left_hdg, -11 );
        lights.push_back( Point3D::fromSGGeod(pt1) );
        normals.push_back( normal );

        pt1 = SGGeodesy::direct( pt1, left_hdg, -1.5 );
        lights.push_back( Point3D::fromSGGeod(pt1) );
        normals.push_back( normal );

        pt1 = SGGeodesy::direct( pt1, left_hdg, -1.5 );
        lights.push_back( Point3D::fromSGGeod(pt1) );
        normals.push_back( normal );
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( lights, false );
    normals_poly.add_contour( normals, false );

    TGSuperPoly result;
    result.set_poly( lights_poly );
    result.set_normals( normals_poly );
    result.set_material( "RWY_WHITE_LIGHTS" );

    return result;
}


// generate REIL lights
TGSuperPoly Runway::gen_reil( const int kind, bool recip )
{
    point_list lights; lights.clear();
    point_list normals; normals.clear();
    string flag = rwy.rwnum[get_thresh0(recip)];
    Point3D normal;

    if (kind == 1) {
        // omnidirectional lights
        normal = Point3D::fromSGVec3( normalize(SGVec3d::fromGeod(GetStart())) );
    } else {
        // unidirectional lights
        normal = gen_runway_light_vector( 10, recip );
    }

    // determine the start point.
    SGGeod ref;
    double length_hdg, left_hdg;
    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)] - 1);
    } else {
        length_hdg = rwy.heading;
        ref = SGGeodesy::direct(GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)] - 1);
    }

    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);
    double offset = rwy.width * 0.5 + 12;

    // left light
    lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct( ref, left_hdg, offset )) );
    normals.push_back( normal );
    
    // right light
    lights.push_back( Point3D::fromSGGeod(SGGeodesy::direct( ref, left_hdg, -offset )) );
    normals.push_back( normal );

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( lights, false );
    normals_poly.add_contour( normals, false );

    TGSuperPoly result;
    result.set_poly( lights_poly );
    result.set_normals( normals_poly );
    result.set_material( "RWY_REIL_LIGHTS" );
    result.set_flag( flag );

    return result;
}


// generate Calvert-I/II approach lighting schemes
superpoly_list Runway::gen_calvert( const string &kind, bool recip )
{
    point_list w_lights; w_lights.clear();
    point_list r_lights; r_lights.clear();
    point_list w_normals; w_normals.clear();
    point_list r_normals; r_normals.clear();
    int i, j;
    string flag;

    Point3D normal = gen_runway_light_vector( 3.0, recip );

    // Generate long center bar of lights
    // determine the start point.
    SGGeod ref_save, pt;
    double length_hdg, left_hdg;
    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref_save = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)] );
    } else {
        length_hdg = rwy.heading;
        ref_save = SGGeodesy::direct( GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)] );
    }
    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);

    SGGeod ref = ref_save;
    //
    // Centre row of lights:
    // 1 x lights out to 300m
    // 2 x lights from 300m to 600m
    // 3 x lights from 600m to 900m
    // light spacing is 30m
    //

    double vert_space = 30;
    double horiz_space = 10;
    int count=30;

    SGGeod crossbar[5];
    SGGeod pair;

    // first set of single lights
    pt = ref;
    for ( i = 0; i < count; ++i ) {


        // centre lights
        pt = SGGeodesy::direct(pt, length_hdg, -vert_space);

        if ( i >= 10 && i < 20 ) {
            pair = SGGeodesy::direct(pt, left_hdg, horiz_space/2);
            w_lights.push_back( Point3D::fromSGGeod(pair) );
            w_normals.push_back( normal );

            pair = SGGeodesy::direct(pt, left_hdg, -horiz_space/2);
            w_lights.push_back( Point3D::fromSGGeod(pair) );
            w_normals.push_back( normal );
        } else if (i >= 20) {
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );

            pair = SGGeodesy::direct(pt, left_hdg, horiz_space);
            w_lights.push_back( Point3D::fromSGGeod(pair) );
            w_normals.push_back( normal );

            pair = SGGeodesy::direct(pt, left_hdg, -horiz_space);
            w_lights.push_back( Point3D::fromSGGeod(pair) );
            w_normals.push_back( normal );
        } else if (i < 10 && kind == "1" ) {
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );
        } else {
            // cal2 has red centre lights
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );
        }

        switch ( i ) {
            case 4:
                crossbar[0] = pt;
                break;
            case 9:
                crossbar[1] = pt;
                break;
            case 14:
                crossbar[2] = pt;
                break;
            case 19:
                crossbar[3] = pt;
                break;
            case 24:
                crossbar[4] = pt;
                break;
        }

    }

    if ( kind == "2" ) {
        // add some red and white bars in the 300m area
        // in front of the threshold
        ref = ref_save;
        for ( int i = 0; i < 9; ++i ) {
            // offset upwind
            ref = SGGeodesy::direct( ref, length_hdg, -vert_space );
            pt = ref;

            // left side bar
            pt = SGGeodesy::direct( pt, left_hdg, 1.5 );
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );

            pt = SGGeodesy::direct( pt, left_hdg, 1.5 );
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );

            pt = ref;
            pt = SGGeodesy::direct( pt, left_hdg, 11 );
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );

            pt = SGGeodesy::direct( pt, left_hdg, 1.5 );
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );

            pt = SGGeodesy::direct( pt, left_hdg, 1.5 );
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );

            pt = ref;

            // right side bar
            pt = SGGeodesy::direct( pt, left_hdg, -1.5 );
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );

            pt = SGGeodesy::direct( pt, left_hdg, -1.5 );
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );

            pt = ref;
            pt = SGGeodesy::direct( pt, left_hdg, -11 );
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );

            pt = SGGeodesy::direct( pt, left_hdg, -1.5 );
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );

            pt = SGGeodesy::direct( pt, left_hdg, -1.5 );
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal );
        }
    }

    ref = ref_save;
    int num_lights = 0;

    // draw nice crossbars
    for ( i = 0; i < 5; i++ ) {
        switch ( i ) {
            case 0:
                num_lights = 4;
                break;
            case 1:
                num_lights = 5;
                break;
            case 2:
                num_lights = 6;
                break;
            case 3:
                num_lights = 7;
                break;
            case 4:
                num_lights = 8;
                break;
        }

        pt = crossbar[i];
        for ( j = 0 ; j < num_lights; j++ ) {
            // left side lights
            pt = SGGeodesy::direct(pt, left_hdg, horiz_space);
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );
        }

        pt = crossbar[i];
        for ( j = 0; j < num_lights; j++ ) {
            // right side lights
            pt = SGGeodesy::direct(pt, left_hdg, -horiz_space);
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal );
        }
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( r_lights, false );
    normals_poly.add_contour( r_normals, false );

    TGSuperPoly red;
    red.set_poly( lights_poly );
    red.set_normals( normals_poly );
    red.set_material( "RWY_RED_LIGHTS" );
    red.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( w_lights, false );
    normals_poly.add_contour( w_normals, false );

    TGSuperPoly white;
    white.set_poly( lights_poly );
    white.set_normals( normals_poly );
    white.set_material( "RWY_WHITE_LIGHTS" );
    white.set_flag( flag );

    superpoly_list result; result.clear();

    result.push_back( red );
    result.push_back( white );

    return result;
}

// generate ALSF-I/II and SALS/SALSF approach lighting schemes
superpoly_list Runway::gen_alsf( const string &kind, bool recip )
{
    point_list g_lights; g_lights.clear();
    point_list w_lights; w_lights.clear();
    point_list r_lights; r_lights.clear();
    point_list s_lights; s_lights.clear();
    point_list g_normals; g_normals.clear();
    point_list w_normals; w_normals.clear();
    point_list r_normals; r_normals.clear();
    point_list s_normals; s_normals.clear();
    int i;
    string flag;

    Point3D normal1 = gen_runway_light_vector( 3.0, recip );

    // Generate long center bar of lights
    // determine the start point.
    SGGeod ref_save, pt;
    double length_hdg, left_hdg;
    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref_save = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)]);
    } else {
        length_hdg = rwy.heading;
        ref_save = SGGeodesy::direct( GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)]);
    }
    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);

    SGGeod ref = ref_save;

    int count;
    if ( kind == "1" || kind == "2" ) {
        // ALSF-I or ALSF-II
        ref = SGGeodesy::direct(ref, length_hdg, -30);
        count = 30;
    } else {
        // SALS/SALSF
        ref = SGGeodesy::direct(ref, length_hdg, -90);
        count = 13;
    }

    for ( i = 0; i < count; ++i ) {
        pt = ref;
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        // left 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 1);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, 1);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = ref;

        // right 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -1);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, -1);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        ref = SGGeodesy::direct(ref, length_hdg, -30);
    }

    ref = ref_save;

    if ( kind == "1" || kind == "O" || kind == "P" ) {
        // Terminating bar

        // offset 60m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -60);

        pt = ref;

        // left 3 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 4.5);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, 1.5);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, 1.5);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = ref;

        // right 3 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -4.5);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, -1.5);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, -1.5);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );
    } else if ( kind == "2" ) {
        // Generate red side row lights

        for ( i = 0; i < 9; ++i ) {
            // offset 30m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -30);

            pt = ref;

            // left 3 side lights
            pt = SGGeodesy::direct(pt, left_hdg, 11);
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );

            pt = SGGeodesy::direct(pt, left_hdg, 1.5);
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );

            pt = SGGeodesy::direct(pt, left_hdg, 1.5);
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );

            pt = ref;

            // right 3 side lights
            pt = SGGeodesy::direct(pt, left_hdg, -11);
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );

            pt = SGGeodesy::direct(pt, left_hdg, -1.5);
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );

            pt = SGGeodesy::direct(pt, left_hdg, -1.5);
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );
        }
    }

    if ( kind == "1" || kind == "O" || kind == "P" ) {
        // Generate pre-threshold bar
        ref = ref_save;

        // offset 30m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -30);
        pt = ref;

        // left 5 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 22.5);
        for ( i = 0; i < 5; ++i ) {
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );
            pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        }
        pt = ref;

        // right 5 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -22.5);
        for ( i = 0; i < 5; ++i ) {
            r_lights.push_back( Point3D::fromSGGeod(pt) );
            r_normals.push_back( normal1 );
            pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        }
    } else if ( kind == "2" ) {
        // Generate -150m extra horizontal row of lights
        ref = ref_save;

        // offset 150m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -150);
        pt = ref;

        // left 4 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 4.25);

        for ( i = 0; i < 4; ++i ) {
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal1 );
            pt = SGGeodesy::direct(pt, left_hdg, 1.5);
        }
        pt = ref;

        // right 4 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -4.25);

        for ( i = 0; i < 4; ++i ) {
            w_lights.push_back( Point3D::fromSGGeod(pt) );
            w_normals.push_back( normal1 );
            pt = SGGeodesy::direct(pt, left_hdg, -1.5);
        }
    }

    ref = ref_save;

    if ( kind == "O" || kind == "P" ) {
        // generate SALS secondary threshold
        ref = SGGeodesy::direct(ref, length_hdg, -60);

        pt = ref;
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        // left 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = ref;

        // right 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        r_lights.push_back( Point3D::fromSGGeod(pt) );
        r_normals.push_back( normal1 );
    }

    // Generate -300m horizontal crossbar
    ref = ref_save;

    // offset 300m downwind
    ref = SGGeodesy::direct(ref, length_hdg, -300);
    pt = ref;

    // left 8 side lights
    pt = SGGeodesy::direct(pt, left_hdg, 4.5);
    for ( i = 0; i < 8; ++i ) {
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
        pt = SGGeodesy::direct(pt, left_hdg, 1.5);
    }

    pt = ref;

    // right 8 side lights
    pt = SGGeodesy::direct(pt, left_hdg, -4.5);
    for ( i = 0; i < 8; ++i ) {
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
        pt = SGGeodesy::direct(pt, left_hdg, -1.5);
    }

    ref = ref_save;

    if ( kind == "1" || kind == "2" ) {
        // generate rabbit lights
        // start 300m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -300);

        for ( i = 0; i < 21; ++i ) {
            s_lights.push_back( Point3D::fromSGGeod(ref) );
            s_normals.push_back( normal1 );

            // offset 30m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -30);
        }
    } else if ( kind == "P" ) {
        // generate 3 sequenced lights aligned with last 3 light bars
        // start 390m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -390);

        for ( i = 0; i < 3; ++i ) {
            s_lights.push_back( Point3D::fromSGGeod(ref) );
            s_normals.push_back( normal1 );

            // offset 30m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -30);
        }
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( g_lights, false );
    normals_poly.add_contour( g_normals, false );

    TGSuperPoly green;
    green.set_poly( lights_poly );
    green.set_normals( normals_poly );
    green.set_material( "RWY_GREEN_LIGHTS" );
    green.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( r_lights, false );
    normals_poly.add_contour( r_normals, false );

    TGSuperPoly red;
    red.set_poly( lights_poly );
    red.set_normals( normals_poly );
    red.set_material( "RWY_RED_LIGHTS" );
    red.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( w_lights, false );
    normals_poly.add_contour( w_normals, false );

    TGSuperPoly white;
    white.set_poly( lights_poly );
    white.set_normals( normals_poly );
    white.set_material( "RWY_WHITE_LIGHTS" );
    white.set_flag( flag );

    superpoly_list result; result.clear();

    result.push_back( green );
    result.push_back( red );
    result.push_back( white );

    if ( s_lights.size() ) {
        lights_poly.erase();
        normals_poly.erase();
        lights_poly.add_contour( s_lights, false );
        normals_poly.add_contour( s_normals, false );

        TGSuperPoly sequenced;
        sequenced.set_poly( lights_poly );
        sequenced.set_normals( normals_poly );
        sequenced.set_material( "RWY_SEQUENCED_LIGHTS" );
        sequenced.set_flag( flag );

        result.push_back( sequenced );
    }

    return result;
}


// generate ODALS or REIL lights. Main differende between the two:
// ODALS is omnidirectional, REIL is unidirectional
TGSuperPoly Runway::gen_odals( const int kind, bool recip )
{
    point_list lights; lights.clear();
    point_list normals; normals.clear();

    int i;
    string flag, material;
    Point3D normal;

    if (kind == 0) {
        // ODALS lighting is omni-directional, but we generate a normal as
        // a placeholder to keep everything happy.
        normal = Point3D::fromSGVec3( normalize(SGVec3d::fromGeod(GetStart())) );
        material = "RWY_ODALS_LIGHTS";
    } else {
        // RAIL lights are directional
        normal = gen_runway_light_vector( 3.0, recip );
        material = "RWY_SEQUENCED_LIGHTS";
    }

    // determine the start point.
    SGGeod ref, pt;
    double length_hdg, left_hdg;

    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)] );
        flag = rwy.rwnum[1];
    } else {
        length_hdg = rwy.heading;
        ref = SGGeodesy::direct( GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)] );
        flag = rwy.rwnum[0];
    }
    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);
    double offset = rwy.width / 2 + 14;

    if (kind == 0) {
        // offset 14m left of runway
        pt = SGGeodesy::direct(ref, left_hdg, offset);
        lights.push_back( Point3D::fromSGGeod(pt) );
        normals.push_back( normal );

        // offset 14m right of runway
        pt = SGGeodesy::direct(ref, left_hdg, -offset);
        lights.push_back( Point3D::fromSGGeod(pt) );
        normals.push_back( normal );
    }

    for ( i = 0; i < 5; ++i ) {
        // offset 90m downwind
        ref = SGGeodesy::direct( ref, length_hdg, -90 );
        lights.push_back( Point3D::fromSGGeod(ref) );
        normals.push_back( normal );
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( lights, false );
    normals_poly.add_contour( normals, false );

    TGSuperPoly result;
    result.set_poly( lights_poly );
    result.set_normals( normals_poly );
    result.set_material( material );

    result.set_flag( flag );

    return result;
}


// generate SSALS, SSALF, and SSALR approach lighting scheme (kind =
// S, F, or R)
superpoly_list Runway::gen_ssalx( const string& kind, bool recip )
{
    point_list g_lights; g_lights.clear();
    point_list w_lights; w_lights.clear();
    point_list r_lights; r_lights.clear();
    point_list s_lights; s_lights.clear();
    point_list g_normals; g_normals.clear();
    point_list w_normals; w_normals.clear();
    point_list r_normals; r_normals.clear();
    point_list s_normals; s_normals.clear();
    int i;
    string flag;

    Point3D normal1 = gen_runway_light_vector( 3.0, recip );
    SGGeod ref_save, pt;

    // Generate long center bar of lights (every 200')
    // determine the start point.
    double length_hdg, left_hdg;
    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref_save = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)] );
    } else {
        length_hdg = rwy.heading;
        ref_save = SGGeodesy::direct( GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)] );
    }
    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);

    SGGeod ref = ref_save;

    for ( i = 0; i < 7; ++i ) {
        // offset 60m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -60);

        pt = ref;
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        // left 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = ref;

        // right 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
    }

    // Generate -300m extra horizontal row of lights
    ref = SGGeodesy::direct(ref_save, length_hdg, -300);
    pt = ref;

    // left 5 side lights
    pt = SGGeodesy::direct(pt, left_hdg, 4.5);
    for ( i = 0; i < 5; ++i ) {
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
        pt = SGGeodesy::direct(pt, left_hdg, 1.5);
    }

    pt = ref;
    // right 5 side lights
    pt = SGGeodesy::direct(pt, left_hdg, -4.5);
    for ( i = 0; i < 5; ++i ) {
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
        pt = SGGeodesy::direct(pt, left_hdg, -1.5);
    }

    if ( kind == "R" ) {
        // generate 8 rabbit lights
        ref = ref_save;

        // start 480m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -480);

        for ( i = 0; i < 8; ++i ) {
            s_lights.push_back( Point3D::fromSGGeod(ref) );
            s_normals.push_back( normal1 );

            // offset 60m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -60);
        }
    } else if ( kind == "F" ) {
        // generate 3 sequenced lights aligned with last 3 light bars
        ref = ref_save;

        // start 300m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -300);

        for ( i = 0; i < 3; ++i ) {
            s_lights.push_back( Point3D::fromSGGeod(ref) );
            s_normals.push_back( normal1 );

            // offset 60m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -60);
        }
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( g_lights, false );
    normals_poly.add_contour( g_normals, false );

    TGSuperPoly green;
    green.set_poly( lights_poly );
    green.set_normals( normals_poly );
    green.set_material( "RWY_GREEN_LIGHTS" );
    green.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( r_lights, false );
    normals_poly.add_contour( r_normals, false );

    TGSuperPoly red;
    red.set_poly( lights_poly );
    red.set_normals( normals_poly );
    red.set_material( "RWY_RED_LIGHTS" );
    red.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( w_lights, false );
    normals_poly.add_contour( w_normals, false );

    TGSuperPoly white;
    white.set_poly( lights_poly );
    white.set_normals( normals_poly );
    white.set_material( "RWY_WHITE_LIGHTS" );
    white.set_flag( flag );

    superpoly_list result; result.clear();

    result.push_back( green );
    result.push_back( red );
    result.push_back( white );

    if ( s_lights.size() > 0 ) {
        lights_poly.erase();
        normals_poly.erase();
        lights_poly.add_contour( s_lights, false );
        normals_poly.add_contour( s_normals, false );

        TGSuperPoly sequenced;
        sequenced.set_poly( lights_poly );
        sequenced.set_normals( normals_poly );
        sequenced.set_material( "RWY_SEQUENCED_LIGHTS" );
        sequenced.set_flag( flag );

        result.push_back( sequenced );
    }

    return result;
}


// generate MALS, MALSF, and MALSR approach lighting scheme (kind =
// ' ', F, or R)
superpoly_list Runway::gen_malsx( const string& kind, bool recip )
{
    point_list g_lights; g_lights.clear();
    point_list w_lights; w_lights.clear();
    point_list r_lights; r_lights.clear();
    point_list s_lights; s_lights.clear();
    point_list g_normals; g_normals.clear();
    point_list w_normals; w_normals.clear();
    point_list r_normals; r_normals.clear();
    point_list s_normals; s_normals.clear();
    int i;
    string flag;

    Point3D normal1 = gen_runway_light_vector( 3.0, recip );

    // Generate long center bar of lights (every 200')
    // determine the start point.
    SGGeod ref_save, pt;
    double length_hdg, left_hdg;
    if ( recip ) {
        length_hdg = SGMiscd::normalizePeriodic(0, 360, rwy.heading + 180);
        ref_save = SGGeodesy::direct( GetEnd(), length_hdg, rwy.threshold[get_thresh0(recip)] );
    } else {
        length_hdg = rwy.heading;
        ref_save = SGGeodesy::direct( GetStart(), length_hdg, rwy.threshold[get_thresh0(recip)] );
    }
    left_hdg = SGMiscd::normalizePeriodic(0, 360, length_hdg - 90.0);
    SGGeod ref = ref_save;

    for ( i = 0; i < 7; ++i ) {
        // offset 60m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -60);

        pt = ref;
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        // left 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, 1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = ref;

        // right 2 side lights
        pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );

        pt = SGGeodesy::direct(pt, left_hdg, -1.0);
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
    }

    // Generate -300m extra horizontal row of lights
    ref = SGGeodesy::direct(ref_save, length_hdg, -300);
    pt = ref;

    // left 5 side lights
    pt = SGGeodesy::direct(pt, left_hdg, 6.5);
    for ( i = 0; i < 5; ++i ) {
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
        pt = SGGeodesy::direct(pt, left_hdg, 0.75);
    }

    pt = ref;
    // right 5 side lights
    pt = SGGeodesy::direct(pt, left_hdg, -6.5);
    for ( i = 0; i < 5; ++i ) {
        w_lights.push_back( Point3D::fromSGGeod(pt) );
        w_normals.push_back( normal1 );
        pt = SGGeodesy::direct(pt, left_hdg, -0.75);
    }

    if ( kind == "R" ) {
        // generate 5 rabbit lights
        ref = ref_save;

        // start 480m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -480);
        for ( i = 0; i < 5; ++i ) {
            s_lights.push_back( Point3D::fromSGGeod(ref) );
            s_normals.push_back( normal1 );

            // offset 60m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -60);
        }
    } else if ( kind == "F" ) {
        // generate 3 sequenced lights aligned with last 3 light bars
        ref = ref_save;

        // start 300m downwind
        ref = SGGeodesy::direct(ref, length_hdg, -300);
        for ( i = 0; i < 3; ++i ) {
            s_lights.push_back( Point3D::fromSGGeod(ref) );
            s_normals.push_back( normal1 );

            // offset 60m downwind
            ref = SGGeodesy::direct(ref, length_hdg, -60);
        }
    }

    TGPolygon lights_poly; lights_poly.erase();
    TGPolygon normals_poly; normals_poly.erase();
    lights_poly.add_contour( g_lights, false );
    normals_poly.add_contour( g_normals, false );

    TGSuperPoly green;
    green.set_poly( lights_poly );
    green.set_normals( normals_poly );
    green.set_material( "RWY_GREEN_LIGHTS" );
    green.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( r_lights, false );
    normals_poly.add_contour( r_normals, false );

    TGSuperPoly red;
    red.set_poly( lights_poly );
    red.set_normals( normals_poly );
    red.set_material( "RWY_RED_LIGHTS" );
    red.set_flag( flag );

    lights_poly.erase();
    normals_poly.erase();
    lights_poly.add_contour( w_lights, false );
    normals_poly.add_contour( w_normals, false );

    TGSuperPoly white;
    white.set_poly( lights_poly );
    white.set_normals( normals_poly );
    white.set_material( "RWY_WHITE_LIGHTS" );
    white.set_flag( flag );

    superpoly_list result; result.clear();

    result.push_back( green );
    result.push_back( red );
    result.push_back( white );

    if ( s_lights.size() > 0 ) {
        lights_poly.erase();
        normals_poly.erase();
        lights_poly.add_contour( s_lights, false );
        normals_poly.add_contour( s_normals, false );

        TGSuperPoly sequenced;
        sequenced.set_poly( lights_poly );
        sequenced.set_normals( normals_poly );
        sequenced.set_material( "RWY_SEQUENCED_LIGHTS" );
        sequenced.set_flag( flag );

        result.push_back( sequenced );
    }

    return result;
}


// top level runway light generator
void Runway::gen_runway_lights( superpoly_list *lights ) {

    unsigned int i, side;
    bool recip;

    for (side = 0; side < 2; ++side) {
        if (side == 0) {
            recip = false;
        } else {
            recip = true;
        }

        // Make edge lighting
        if ( rwy.edge_lights ) {
            superpoly_list s = gen_runway_edge_lights( recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        // Centerline lighting
        if ( rwy.centerline_lights ) {
            superpoly_list s = gen_runway_center_line_lights( recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        // Touchdown zone lighting
        if ( rwy.tz_lights[side] ) {
            TGSuperPoly s = gen_touchdown_zone_lights( recip );
            lights->push_back( s );
        }

        // REIL lighting
        if ( rwy.reil[side] ) {
            TGSuperPoly s = gen_reil( rwy.reil[side], recip );
            lights->push_back( s );
        }

        // Approach lighting
        if ( rwy.approach_lights[side] == 1 /* ALSF-I */ ) {
            superpoly_list s = gen_alsf( "1", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 2 /* ALSF-II */ ) {
            superpoly_list s = gen_alsf( "2", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 3  /* Calvert I */ ) {
            superpoly_list s = gen_calvert( "1", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 4  /* Calvert II */ ) {
            superpoly_list s = gen_calvert( "2", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 5 /* SSALR */ ) {
            superpoly_list s = gen_ssalx( "R", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 6 /* SSALF */ ) {
            superpoly_list s = gen_ssalx( "F", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        // SALS (Essentially ALSF-1 without the lead in rabbit lights, and
        // a shorter center bar)
        else if ( rwy.approach_lights[side] == 7 /* SALS */ ) {
            superpoly_list s = gen_alsf( "O", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 8 /* MALSR */ ) {
            superpoly_list s = gen_malsx( "R", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 9 /* MALSF */ ) {
            superpoly_list s = gen_malsx( "F", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 10 /* MALSX */ ) {
            superpoly_list s = gen_malsx( "x", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == 11 /* ODALS Omni-directional approach light system */ ) {
            TGSuperPoly s = gen_odals( 0, recip );
            lights->push_back( s );
        }

        // RAIL: Sequenced strobes with no other approach lights
        else if ( rwy.approach_lights[side] == 12 /* RAIL Runway alignment indicator lights */ ) {
            TGSuperPoly s = gen_odals( 1, recip );
            lights->push_back( s );
        }

#if 0
        else if ( rwy.approach_lights[side] == -1 /* SALSF not supported by database */ ) {
            superpoly_list s = gen_alsf( "P", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        else if ( rwy.approach_lights[side] == -1 /* SSALS not supported by database */ ) {
            superpoly_list s = gen_ssalx( "S", recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }
#endif

        // Approach light systems that have a threshold light bar
        // use a central routine for its creation
        if ( rwy.approach_lights[side] > 0 && rwy.approach_lights[side] < 11)
        {
            superpoly_list s = gen_runway_threshold_lights( 1, recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }

        // If the runway has edge lights but no approach lights,
        // or approach lights without a lights bar,
        // create a simple threshold lighting
        if ( rwy.edge_lights && (rwy.approach_lights[side] == 0 || rwy.approach_lights[side] > 10))
        {
            superpoly_list s = gen_runway_threshold_lights( 0, recip );
            for ( i = 0; i < s.size(); ++i ) {
                lights->push_back( s[i] );
            }
        }
    }
}
