// build.cxx -- routines to build polygon model of an airport from the runway
//              definition
//
// Written by Curtis Olson, started September 1999.
//
// Copyright (C) 1999  Curtis L. Olson  - curt@flightgear.org
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// $Id$
//


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <simgear/compiler.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/misc/exception.hxx>

#include <stdio.h>
#include <stdlib.h>		// for atoi() atof()
#include <time.h>

#include <list>
#include <map>
#include STL_STRING

#include <plib/sg.h>			// plib include

#include <simgear/constants.h>
#include <simgear/bucket/newbucket.hxx>
#include <simgear/io/sg_binobj.hxx>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/misc/texcoord.hxx>

#include <Geometry/poly_support.hxx>
#include <Geometry/trinodes.hxx>
#include <Output/output.hxx>
#include <Polygon/index.hxx>
#include <Polygon/polygon.hxx>
#include <Polygon/split.hxx>
#include <Polygon/superpoly.hxx>
#include <Triangulate/trieles.hxx>

#include "apt_surface.hxx"
#include "convex_hull.hxx"
#include "lights.hxx"
#include "point2d.hxx"
#include "poly_extra.hxx"
#include "runway.hxx"
#include "rwy_common.hxx"
#include "rwy_nonprec.hxx"
#include "rwy_prec.hxx"
#include "rwy_simple.hxx"
#include "rwy_visual.hxx"
#include "taxiway.hxx"
#include "texparams.hxx"

#include "build.hxx"

SG_USING_STD(map);
SG_USING_STD(less);
SG_USING_STD(string);


// calculate texture coordinates for runway section using the provided
// texturing parameters.  Returns a mirror polygon to the runway,
// except each point is the texture coordinate of the corresponding
// point in the original polygon.
static TGPolygon rwy_section_tex_coords( const TGPolygon& in_poly,
					 const TGTexParams& tp,
                                         const bool clip_result )
{
    int i, j;
    TGPolygon result;
    result.erase();
    // double length = rwy.length * SG_FEET_TO_METER;
    // double width = rwy.width * SG_FEET_TO_METER;

    Point3D ref = tp.get_ref();
    double width = tp.get_width();
    double length = tp.get_length();
    double heading = tp.get_heading();
    SG_LOG( SG_GENERAL, SG_INFO, "section ref = " << ref );
    SG_LOG( SG_GENERAL, SG_INFO, "  width = " << width );
    SG_LOG( SG_GENERAL, SG_INFO, "  length = " << length );
    SG_LOG( SG_GENERAL, SG_INFO, "  heading = " << heading );
    Point3D p, t;
    double x, y, tx, ty;

    for ( i = 0; i < in_poly.contours(); ++i ) {
	for ( j = 0; j < in_poly.contour_size( i ); ++j ) {
	    p = in_poly.get_pt( i, j );
	    SG_LOG(SG_GENERAL, SG_DEBUG, "point = " << p);

	    //
	    // 1. Calculate distance and bearing from the center of
	    // the runway
	    //

	    // given alt, lat1, lon1, lat2, lon2, calculate starting
	    // and ending az1, az2 and distance (s).  Lat, lon, and
	    // azimuth are in degrees.  distance in meters
	    double az1, az2, dist;
	    geo_inverse_wgs_84( 0, ref.y(), ref.x(), p.y(), p.x(),
				&az1, &az2, &dist );
	    SG_LOG(SG_GENERAL, SG_INFO, "basic course = " << az2);

	    //
	    // 2. Rotate this back into a coordinate system where Y
	    // runs the length of the runway and X runs crossways.
	    //

	    double course = az2 - heading;
	    while ( course < -360 ) { course += 360; }
	    while ( course > 360 ) { course -= 360; }
	    SG_LOG( SG_GENERAL, SG_INFO,
                    "  course = " << course << "  dist = " << dist );

	    //
	    // 3. Convert from polar to cartesian coordinates
	    //

	    x = sin( course * SGD_DEGREES_TO_RADIANS ) * dist;
	    y = cos( course * SGD_DEGREES_TO_RADIANS ) * dist;
	    SG_LOG(SG_GENERAL, SG_INFO, "  x = " << x << " y = " << y);

	    //
	    // 4. Map x, y point into texture coordinates
	    //
	    
	    tx = x / width;
	    // tx = ((int)(tx * 100)) / 100.0;
	    SG_LOG(SG_GENERAL, SG_INFO, "  (" << tx << ")");

            if ( clip_result ) {
                if ( tx < 0.0 ) { tx = 0.0; }
                if ( tx > 1.0 ) { tx = 1.0; }
            }

	    // ty = (y - min.y()) / (max.y() - min.y());
	    ty = y / length;
	    // ty = ((int)(ty * 100)) / 100.0;
	    SG_LOG(SG_GENERAL, SG_INFO, "  (" << ty << ")");

            if ( clip_result ) {
                if ( ty < 0.0 ) { ty = 0.0; }
                if ( ty > 1.0 ) { ty = 1.0; }
            }

	    t = Point3D( tx, ty, 0 );
	    SG_LOG(SG_GENERAL, SG_DEBUG, "  (" << tx << ", " << ty << ")");

	    result.add_node( i, t );
	}
    }

    return result;
}


// Determine node elevations of a point_list based on the provided
// TGAptSurface.  Offset is added to the final elevation
static point_list calc_elevations( TGAptSurface &surf,
                                   const point_list& geod_nodes,
                                   double offset )
{
    point_list result = geod_nodes;
    for ( unsigned int i = 0; i < result.size(); ++i ) {
        double elev = surf.query( result[i].lon(), result[i].lat() );
        result[i].setelev( elev + offset );
    }

    return result;
}


// Determine node elevations of each node of a TGPolygon based on the
// provided TGAptSurface.  Offset is added to the final elevation
static TGPolygon calc_elevations( TGAptSurface &surf,
                                  const TGPolygon& poly,
                                  double offset )
{
    TGPolygon result;
    for ( int i = 0; i < poly.contours(); ++i ) {
        point_list contour = poly.get_contour( i );
        point_list elevated = calc_elevations( surf, contour, offset );

        result.add_contour( elevated, poly.get_hole_flag(i) );
    }

    return result;
}


// strip trailing spaces
static void my_chomp( string& str ) {
    SG_LOG(SG_GENERAL, SG_DEBUG, "my_chomp()");
    SG_LOG(SG_GENERAL, SG_DEBUG, "'" << str.substr( str.length() - 1, 1 ) << "'");
    while ( str.substr( str.length() - 1, 1 ) == " " ) {
	str = str.substr( 0, str.length() - 1 );
	SG_LOG(SG_GENERAL, SG_DEBUG, "'" << str.substr( str.length() - 1, 1 ) << "'");
    }
}


// build a runway
static void build_runway( const TGRunway& rwy_info,
                          double alt_m,
                          superpoly_list *rwy_polys,
                          texparams_list *texparams,
                          TGPolygon *accum,
                          TGPolygon *apt_base,
                          TGPolygon *apt_clearing )
{
    SG_LOG(SG_GENERAL, SG_DEBUG, "surface flags = " << rwy_info.surface_flags);
    string surface_flag = rwy_info.surface_flags.substr(1, 1);
    SG_LOG(SG_GENERAL, SG_DEBUG, "surface flag = " << surface_flag);

    string material;
    if ( surface_flag == "A" ) {
        if ( !rwy_info.really_taxiway ) {
            material = "pa_";	// asphalt
        } else {
            material = "pa_taxiway";
        }
    } else if ( surface_flag == "C" ) {
        if ( !rwy_info.really_taxiway ) {
            material = "pc_";	// concrete
        } else {
            if ( rwy_info.width <= 150 ) {
                material = "pc_taxiway";
            } else {
                material = "pc_tiedown";
            }
        }
    } else if ( surface_flag == "D" ) {
        material = "dirt_rwy";
    } else if ( surface_flag == "G" ) {
        material = "grass_rwy";
    } else if ( surface_flag == "L" ) {
        if ( rwy_info.really_taxiway ) {
            material = "lakebed_taxiway";
        } else {
            material = "dirt_rwy";
        }
    } else if ( surface_flag == "T" ) {
        material = "grass_rwy";
    } else if ( surface_flag == "W" ) {
        // water ???
    } else {
	throw sg_exception("unknown runway type!");
    }


    string type_flag = rwy_info.surface_flags.substr(2, 1);
    SG_LOG(SG_GENERAL, SG_DEBUG, "type flag = " << type_flag);

    if ( rwy_info.really_taxiway ) {
	gen_taxiway( rwy_info, alt_m, material,
                     rwy_polys, texparams, accum );
    } else if ( surface_flag == "D" || surface_flag == "G" ||
	 surface_flag == "T" )
    {
	gen_simple_rwy( rwy_info, alt_m, material,
			rwy_polys, texparams, accum );
    } else if ( type_flag == "P" ) {
	// precision runway markings
	gen_precision_rwy( rwy_info, alt_m, material,
			   rwy_polys, texparams, accum );
    } else if ( type_flag == "R" ) {
	// non-precision runway markings
	gen_non_precision_rwy( rwy_info, alt_m, material,
			       rwy_polys, texparams, accum );
    } else if ( type_flag == "V" ) {
	// visual runway markings
	gen_visual_rwy( rwy_info, alt_m, material,
			rwy_polys, texparams, accum );
    } else if ( type_flag == "B" ) {
	// bouys (sea plane base)
	// do nothing for now.
    } else if ( type_flag == "H" ) {
	// helipad
	// do nothing for now.
    } else {
	// unknown runway code ... hehe, I know, let's just die
	// right here so the programmer has to fix his code if a
	// new code ever gets introduced. :-)
	throw sg_exception("Unknown runway code in build.cxx:build_airport()");
    }

    TGPolygon base, safe_base;
    if ( rwy_info.really_taxiway ) {
	base = gen_runway_area_w_extend( rwy_info, 0.0, 10.0, 10.0 );
        // also clear a safe area around the taxiway
        safe_base = gen_runway_area_w_extend( rwy_info, 0.0, 40.0, 40.0 );
    } else {
	base = gen_runway_area_w_extend( rwy_info, 0.0, 20.0, 20.0 );
        // also clear a safe area around the runway
        safe_base = gen_runway_area_w_extend( rwy_info, 0.0, 180.0, 50.0 );
    }
    *apt_clearing = polygon_union(safe_base, *apt_clearing);

    // add base to apt_base
    *apt_base = polygon_union( base, *apt_base );
}


// build 3d airport
void build_airport( string airport_id, float alt_m,
                    string_list& runways_raw,
                    string_list& taxiways_raw,
                    const string& root,
                    const string_list& elev_src )
{
    int i, j, k;

    superpoly_list rwy_polys;
    texparams_list texparams;

    // poly_list rwy_tris, rwy_txs;
    TGPolygon runway, runway_a, runway_b, clipped_a, clipped_b;
    TGPolygon split_a, split_b;
    TGPolygon apt_base;
    TGPolygon apt_clearing;
    Point3D p;

    TGPolygon accum;
    accum.erase();

    // parse main airport information
    double apt_lon = 0.0, apt_lat = 0.0;
    int rwy_count = 0;

    SG_LOG( SG_GENERAL, SG_INFO, "Building " << airport_id );

    // parse runways and generate the vertex list
    runway_list runways;
    runways.clear();
    string rwy_str;

    for ( i = 0; i < (int)runways_raw.size(); ++i ) {
        ++rwy_count;

	rwy_str = runways_raw[i];

	TGRunway rwy;

        rwy.really_taxiway = false;

	SG_LOG(SG_GENERAL, SG_DEBUG, rwy_str);
	rwy.rwy_no = rwy_str.substr(7, 4);

	string rwy_lat = rwy_str.substr(11, 10);
	rwy.lat = atof( rwy_lat.c_str() );
        apt_lat += rwy.lat;

	string rwy_lon = rwy_str.substr(22, 11);
	rwy.lon = atof( rwy_lon.c_str() );
        apt_lon += rwy.lon;

	string rwy_hdg = rwy_str.substr(34, 6);
	rwy.heading = atof( rwy_hdg.c_str() );

	string rwy_len = rwy_str.substr(41, 5);
	rwy.length = atoi( rwy_len.c_str() );

	string rwy_width = rwy_str.substr(47, 5);
	rwy.width = atoi( rwy_width.c_str() );

	rwy.surface_flags = rwy_str.substr(53, 5);

	rwy.end1_flags = rwy_str.substr(59, 4);

        string rwy_disp_threshold1 = rwy_str.substr(64, 4);
	rwy.disp_thresh1 = atoi( rwy_disp_threshold1.c_str() );

        string rwy_stopway1 = rwy_str.substr(69, 4);
	rwy.stopway1 = atoi( rwy_stopway1.c_str() );

	rwy.end2_flags = rwy_str.substr(74, 4);

        string rwy_disp_threshold2 = rwy_str.substr(79, 4);
	rwy.disp_thresh2 = atoi( rwy_disp_threshold2.c_str() );

        string rwy_stopway2 = rwy_str.substr(83, 4);
	rwy.stopway2 = atoi( rwy_stopway2.c_str() );

	SG_LOG( SG_GENERAL, SG_INFO, "  no    = " << rwy.rwy_no);
	SG_LOG( SG_GENERAL, SG_INFO, "  lat   = " << rwy_lat << " " << rwy.lat);
	SG_LOG( SG_GENERAL, SG_INFO, "  lon   = " << rwy_lon << " " << rwy.lon);
	SG_LOG( SG_GENERAL, SG_INFO, "  hdg   = " << rwy_hdg << " "
                << rwy.heading);
	SG_LOG( SG_GENERAL, SG_INFO, "  len   = " << rwy_len << " "
                << rwy.length);
	SG_LOG( SG_GENERAL, SG_INFO, "  width = " << rwy_width << " "
                << rwy.width);
	SG_LOG( SG_GENERAL, SG_INFO, "  sfc   = " << rwy.surface_flags);
	SG_LOG( SG_GENERAL, SG_INFO, "  end1  = " << rwy.end1_flags);
        SG_LOG( SG_GENERAL, SG_INFO, "  dspth1= " << rwy_disp_threshold1
                << " " << rwy.disp_thresh1);
        SG_LOG( SG_GENERAL, SG_INFO, "  stop1 = " << rwy_stopway1 << " "
                << rwy.stopway1);
	SG_LOG( SG_GENERAL, SG_INFO, "  end2  = " << rwy.end2_flags);
        SG_LOG( SG_GENERAL, SG_INFO, "  dspth2= " << rwy_disp_threshold2
                << " " << rwy.disp_thresh2);
        SG_LOG( SG_GENERAL, SG_INFO, "  stop2 = " << rwy_stopway2 << " "
                << rwy.stopway2);

	runways.push_back( rwy );
    }

    SGBucket b( apt_lon / (double)rwy_count, apt_lat / (double)rwy_count );
    Point3D center_geod( b.get_center_lon() * SGD_DEGREES_TO_RADIANS,
			 b.get_center_lat() * SGD_DEGREES_TO_RADIANS, 0 );
    Point3D gbs_center = sgGeodToCart( center_geod );
    SG_LOG(SG_GENERAL, SG_INFO, b.gen_base_path() << "/" << b.gen_index_str());

    // parse taxiways and generate the vertex list
    runway_list taxiways;
    taxiways.clear();

    for ( i = 0; i < (int)taxiways_raw.size(); ++i ) {
	string taxi_str = taxiways_raw[i];

	TGRunway taxi;

        taxi.really_taxiway = true;
        taxi.generated = false;

	SG_LOG(SG_GENERAL, SG_INFO, taxi_str);

	string taxi_lat = taxi_str.substr(11, 10);
	taxi.lat = atof( taxi_lat.c_str() );

	string taxi_lon = taxi_str.substr(22, 11);
	taxi.lon = atof( taxi_lon.c_str() );

	string taxi_hdg = taxi_str.substr(34, 6);
	taxi.heading = atof( taxi_hdg.c_str() );

	string taxi_len = taxi_str.substr(41, 5);
	taxi.length = atoi( taxi_len.c_str() );

	string taxi_width = taxi_str.substr(47, 5);
	taxi.width = atoi( taxi_width.c_str() );

	taxi.surface_flags = taxi_str.substr(53, 5);

	SG_LOG( SG_GENERAL, SG_INFO, "  lat   = " << taxi_lat << " "
                << taxi.lat);
	SG_LOG( SG_GENERAL, SG_INFO, "  lon   = " << taxi_lon << " "
                << taxi.lon);
	SG_LOG( SG_GENERAL, SG_INFO, "  hdg   = " << taxi_hdg << " "
                << taxi.heading);
	SG_LOG( SG_GENERAL, SG_INFO, "  len   = " << taxi_len << " "
                << taxi.length);
	SG_LOG( SG_GENERAL, SG_INFO, "  width = " << taxi_width << " "
                << taxi.width);
	SG_LOG( SG_GENERAL, SG_INFO, "  sfc   = " << taxi.surface_flags);

	taxiways.push_back( taxi );
    }

    TGSuperPoly sp;
    TGTexParams tp;

    // First pass: generate the precision runways since these have
    // precidence
    for ( i = 0; i < (int)runways.size(); ++i ) {
	string type_flag = runways[i].surface_flags.substr(2, 1);
	if ( type_flag == "P" ) {
	    build_runway( runways[i], alt_m,
			  &rwy_polys, &texparams, &accum,
                          &apt_base, &apt_clearing );
	}
    }

    // 2nd pass: generate the non-precision and visual runways
    for ( i = 0; i < (int)runways.size(); ++i ) {
	string type_flag = runways[i].surface_flags.substr(2, 1);
	if ( type_flag == "R" || type_flag == "V" ) {
	    build_runway( runways[i], alt_m,
			  &rwy_polys, &texparams, &accum,
                          &apt_base, &apt_clearing );
	}
    }

    // 3rd pass: generate all remaining runways not covered in the first pass
    for ( i = 0; i < (int)runways.size(); ++i ) {
	string type_flag = runways[i].surface_flags.substr(2, 1);
	if ( type_flag != string("P") && type_flag != string("R")
             && type_flag != string("V") ) {
	    build_runway( runways[i], alt_m,
			  &rwy_polys, &texparams, &accum,
                          &apt_base, &apt_clearing );
	}
    }

    // 4th pass: generate all taxiways

    // we want to generate in order of largest size first so this will
    // look a little weird, but that's all I'm doing, otherwise a
    // simple list traversal would work fine.
    bool done = false;
    while ( !done ) {
        // find the largest taxiway
        int largest_idx = -1;
        double max_size = 0;
        for ( i = 0; i < (int)taxiways.size(); ++i ) {
            double size = taxiways[i].length * taxiways[i].width;
            if ( size > max_size && !taxiways[i].generated ) {
                max_size = size;
                largest_idx = i;
            }
        }

        if ( largest_idx >= 0 ) {
            SG_LOG( SG_GENERAL, SG_INFO, "generating " << largest_idx );
            build_runway( taxiways[largest_idx], alt_m,
                          &rwy_polys, &texparams, &accum,
                          &apt_base, &apt_clearing );
            taxiways[largest_idx].generated = true;
        } else {
            done = true;
        }
    }

    // write_polygon( accum, "accum" );
    if ( apt_base.total_size() == 0 ) {
        SG_LOG(SG_GENERAL, SG_ALERT, "no airport points generated");
	return;
    }

    // 5th pass: generate runway lights
    superpoly_list rwy_lights; rwy_lights.clear();
    for ( i = 0; i < (int)runways.size(); ++i ) {
	gen_runway_lights( runways[i], alt_m, rwy_lights );
    }

    // 6th pass: generate all taxiway lights
    for ( i = 0; i < (int)taxiways.size(); ++i ) {
        gen_taxiway_lights( taxiways[i], alt_m, rwy_lights );
    }

    // generate convex hull (no longer)
    // TGPolygon hull = convex_hull(apt_pts);

    TGPolygon filled_base = strip_out_holes( apt_base );
    // write_polygon( filled_base, "filled-base" );
    TGPolygon divided_base = split_long_edges( filled_base, 200.0 );
    // write_polygon( divided_base, "divided-base" );
    TGPolygon base_poly = polygon_diff( divided_base, accum );
    // write_polygon( base_poly, "base-raw" );

    // Try to remove duplicated nodes and other degeneracies
    for ( k = 0; k < (int)rwy_polys.size(); ++k ) {
	SG_LOG(SG_GENERAL, SG_DEBUG, "add nodes/remove dups section = " << k
	       << " " << rwy_polys[k].get_material());
	TGPolygon poly = rwy_polys[k].get_poly();
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size before = " << poly.total_size());
	for ( i = 0; i < poly.contours(); ++i ) {
	    for ( j = 0; j < poly.contour_size(i); ++j ) {
		Point3D tmp = poly.get_pt(i, j);
		printf("  %.7f %.7f %.7f\n", tmp.x(), tmp.y(), tmp.z() );
	    }
	}

	poly = remove_dups( poly );
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size after remove_dups() = "
	       << poly.total_size());

	for ( i = 0; i < poly.contours(); ++i ) {
	    for ( j = 0; j < poly.contour_size(i); ++j ) {
		Point3D tmp = poly.get_pt(i, j);
		printf("    %.7f %.7f %.7f\n", tmp.x(), tmp.y(), tmp.z() );
	    }
	}

	poly = reduce_degeneracy( poly );
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size after reduce_degeneracy() = "
	       << poly.total_size());

	for ( i = 0; i < poly.contours(); ++i ) {
	    for ( j = 0; j < poly.contour_size(i); ++j ) {
		Point3D tmp = poly.get_pt(i, j);
		printf("    %.7f %.7f %.7f\n", tmp.x(), tmp.y(), tmp.z() );
	    }
	}

	rwy_polys[k].set_poly( poly );
    }

    // add segments to polygons to remove any possible "T"
    // intersections
    TGTriNodes tmp_nodes;

    // build temporary node list
    for ( k = 0; k < (int)rwy_polys.size(); ++k ) {
	TGPolygon poly = rwy_polys[k].get_poly();
	for ( i = 0; i < poly.contours(); ++i ) {
	    for ( j = 0; j < poly.contour_size( i ); ++j ) {
		tmp_nodes.unique_add( poly.get_pt(i, j) );
	    }
	}
    }
    for ( i = 0; i < base_poly.contours(); ++i ) {
	for ( j = 0; j < base_poly.contour_size( i ); ++j ) {
	    tmp_nodes.unique_add( base_poly.get_pt(i, j) );
	}
    }
    // the divided base could contain points not found in base_poly,
    // so we should add them because the skirt needs them.
    for ( i = 0; i < divided_base.contours(); ++i ) {
	for ( j = 0; j < divided_base.contour_size( i ); ++j ) {
	    tmp_nodes.unique_add( divided_base.get_pt(i, j) );
	}
    }

#if 0
    // dump info for debugging purposes
    point_list ttt = tmp_nodes.get_node_list();
    FILE *fp = fopen( "tmp_nodes", "w" );
    for ( i = 0; i < (int)ttt.size(); ++i ) {
	fprintf(fp, "%.8f %.8f\n", ttt[i].x(), ttt[i].y());
    }
    fclose(fp);

    for ( i = 0; i < base_poly.contours(); ++i ) {
	char name[256];
	sprintf(name, "l%d", i );
	FILE *fp = fopen( name, "w" );

	for ( j = 0; j < base_poly.contour_size( i ) - 1; ++j ) {
	    Point3D p0 = base_poly.get_pt(i, j);
	    Point3D p1 = base_poly.get_pt(i, j + 1);
	    fprintf(fp, "%.8f %.8f\n", p0.x(), p0.y());
	    fprintf(fp, "%.8f %.8f\n", p1.x(), p1.y());
	}
	Point3D p0 = base_poly.get_pt(i, base_poly.contour_size( i ) - 1);
	Point3D p1 = base_poly.get_pt(i, 0);
	fprintf(fp, "%.8f %.8f\n", p0.x(), p0.y());
	fprintf(fp, "%.8f %.8f\n", p1.x(), p1.y());
	fclose(fp);
    }
#endif

    for ( k = 0; k < (int)rwy_polys.size(); ++k ) {
	TGPolygon poly = rwy_polys[k].get_poly();
	poly = add_nodes_to_poly( poly, tmp_nodes );
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size after add nodes = " << poly.total_size());

#if 0
	char tmp[256];
	sprintf( tmp, "r%d", k );
	write_polygon( poly, tmp );
#endif

	rwy_polys[k].set_poly( poly );
    }

    // One more pass to try to get rid of other yukky stuff
    for ( k = 0; k < (int)rwy_polys.size(); ++k ) {
	TGPolygon poly = rwy_polys[k].get_poly();

        poly = remove_dups( poly );
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size after remove_dups() = " << poly.total_size());
        poly = remove_bad_contours( poly );
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size after remove_bad() = " << poly.total_size());

	rwy_polys[k].set_poly( poly );
    }

    SG_LOG(SG_GENERAL, SG_INFO, "add nodes base ");
    cout << " before: " << base_poly << endl;
    cout << " tmp_nodes size = " << tmp_nodes.get_node_list().size() << endl;

    base_poly = add_nodes_to_poly( base_poly, tmp_nodes );
    cout << " after adding tmp_nodes: " << base_poly << endl;

    // write_polygon( base_poly, "base-add" );
    SG_LOG(SG_GENERAL, SG_DEBUG, "remove dups base ");
    base_poly = remove_dups( base_poly );
    SG_LOG(SG_GENERAL, SG_DEBUG, "remove bad contours base");
    base_poly = remove_bad_contours( base_poly );
    // write_polygon( base_poly, "base-fin" );
    cout << " after clean up: " << base_poly << endl;

    // tesselate the polygons and prepair them for final output

    for ( i = 0; i < (int)rwy_polys.size(); ++i ) {
        SG_LOG(SG_GENERAL, SG_INFO, "Tesselating section = " << i);

	TGPolygon poly = rwy_polys[i].get_poly();
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size before = " << poly.total_size());
	TGPolygon tri = polygon_tesselate_alt( poly );
	SG_LOG(SG_GENERAL, SG_DEBUG, "total size after = " << tri.total_size());

        TGPolygon tc;
        if ( rwy_polys[i].get_flag() == "taxi" ) {
            SG_LOG(SG_GENERAL, SG_DEBUG, "taxiway, no clip");
            tc = rwy_section_tex_coords( tri, texparams[i], false );
        } else {
            tc = rwy_section_tex_coords( tri, texparams[i], true );
        }

	rwy_polys[i].set_tris( tri );
	rwy_polys[i].set_texcoords( tc );
	rwy_polys[i].set_tri_mode( GL_TRIANGLES );
    }

    SG_LOG(SG_GENERAL, SG_DEBUG, "Tesselating base");
    TGPolygon base_tris = polygon_tesselate_alt( base_poly );

#if 0
    // dump more debugging output
    for ( i = 0; i < base_strips.contours(); ++i ) {
	char name[256];
	sprintf(name, "s%d", i );
	FILE *fp = fopen( name, "w" );

	for ( j = 0; j < base_strips.contour_size( i ) - 1; ++j ) {
	    Point3D p0 = base_strips.get_pt(i, j);
	    Point3D p1 = base_strips.get_pt(i, j + 1);
	    fprintf(fp, "%.8f %.8f\n", p0.x(), p0.y());
	    fprintf(fp, "%.8f %.8f\n", p1.x(), p1.y());
	}
	Point3D p0 = base_strips.get_pt(i, base_strips.contour_size( i ) - 1);
	Point3D p1 = base_strips.get_pt(i, 0);
	fprintf(fp, "%.8f %.8f\n", p0.x(), p0.y());
	fprintf(fp, "%.8f %.8f\n", p1.x(), p1.y());
	fclose(fp);
    }
#endif

    //
    // We should now have the runway polygons all generated with their
    // corresponding triangles and texture coordinates, and the
    // surrounding base area.
    //
    // Next we need to create the output lists ... vertices, normals,
    // texture coordinates, and tri-strips
    //

    // traverse the tri list and create ordered node and texture
    // coordinate lists

    TGTriNodes nodes, normals, texcoords;
    nodes.clear();
    normals.clear();
    texcoords.clear();

    group_list pts_v; pts_v.clear();
    group_list pts_n; pts_n.clear();
    string_list pt_materials; pt_materials.clear();

    group_list tris_v; tris_v.clear();
    group_list tris_n; tris_n.clear();
    group_list tris_tc; tris_tc.clear();
    string_list tri_materials; tri_materials.clear();

    group_list strips_v; strips_v.clear();
    group_list strips_n; strips_n.clear();
    group_list strips_tc; strips_tc.clear();
    string_list strip_materials; strip_materials.clear();

    Point3D tc;
    int index;
    int_list pt_v, tri_v, strip_v;
    int_list pt_n, tri_n, strip_n;
    int_list tri_tc, strip_tc;

    // calculate "the" normal for this airport
    p.setx( base_tris.get_pt(0, 0).x() * SGD_DEGREES_TO_RADIANS );
    p.sety( base_tris.get_pt(0, 0).y() * SGD_DEGREES_TO_RADIANS );
    p.setz( 0 );
    Point3D vnt = sgGeodToCart( p );
    // SG_LOG(SG_GENERAL, SG_DEBUG, "geod = " << p);
    // SG_LOG(SG_GENERAL, SG_DEBUG, "cart = " << tmp);

    sgdVec3 tmp;
    sgdSetVec3( tmp, vnt.x(), vnt.y(), vnt.z() );
    sgdNormalizeVec3( tmp );

    Point3D vn( tmp[0], tmp[1], tmp[2] );
    SG_LOG(SG_GENERAL, SG_DEBUG, "found normal for this airport = " << tmp);

    for ( k = 0; k < (int)rwy_polys.size(); ++k ) {
	SG_LOG(SG_GENERAL, SG_DEBUG, "tri " << k);
	// TGPolygon tri_poly = rwy_tris[k];
	TGPolygon tri_poly = rwy_polys[k].get_tris();
	TGPolygon tri_txs = rwy_polys[k].get_texcoords();
	string material = rwy_polys[k].get_material();
	SG_LOG(SG_GENERAL, SG_DEBUG, "material = " << material);
	SG_LOG(SG_GENERAL, SG_DEBUG, "poly size = " << tri_poly.contours());
	SG_LOG(SG_GENERAL, SG_DEBUG, "texs size = " << tri_txs.contours());
	for ( i = 0; i < tri_poly.contours(); ++i ) {
	    tri_v.clear();
	    tri_n.clear();
	    tri_tc.clear();
	    for ( j = 0; j < tri_poly.contour_size(i); ++j ) {
		p = tri_poly.get_pt( i, j );
		index = nodes.unique_add( p );
		tri_v.push_back( index );

		// use 'the' normal
		index = normals.unique_add( vn );
		tri_n.push_back( index );

		tc = tri_txs.get_pt( i, j );
		index = texcoords.unique_add( tc );
		tri_tc.push_back( index );
	    }
	    tris_v.push_back( tri_v );
	    tris_n.push_back( tri_n );
	    tris_tc.push_back( tri_tc );
	    tri_materials.push_back( material );
	}
    }

    // add base points
    point_list base_txs; 
    int_list base_tc;
    for ( i = 0; i < base_tris.contours(); ++i ) {
	tri_v.clear();
	tri_n.clear();
	tri_tc.clear();
	for ( j = 0; j < base_tris.contour_size(i); ++j ) {
	    p = base_tris.get_pt( i, j );
	    index = nodes.unique_add( p );
	    tri_v.push_back( index );

	    index = normals.unique_add( vn );
	    tri_n.push_back( index);
	}
	tris_v.push_back( tri_v );
	tris_n.push_back( tri_n );
	tri_materials.push_back( "Grass" );

	base_txs.clear();
	base_txs = sgCalcTexCoords( b, nodes.get_node_list(), tri_v );

	base_tc.clear();
	for ( j = 0; j < (int)base_txs.size(); ++j ) {
	    tc = base_txs[j];
	    // SG_LOG(SG_GENERAL, SG_DEBUG, "base_tc = " << tc);
	    index = texcoords.simple_add( tc );
	    base_tc.push_back( index );
	}
	tris_tc.push_back( base_tc );
    }

    // calculation min/max coordinates of airport area
    Point3D min_deg(9999.0, 9999.0, 0), max_deg(-9999.0, -9999.0, 0);
    for ( j = 0; j < (int)nodes.get_node_list().size(); ++j ) {
        Point3D p = nodes.get_node_list()[j];
        if ( p.lon() < min_deg.lon() ) {
            min_deg.setlon( p.lon() );
        }
        if ( p.lon() > max_deg.lon() ) {
            max_deg.setlon( p.lon() );
        }
        if ( p.lat() < min_deg.lat() ) {
            min_deg.setlat( p.lat() );
        }
        if ( p.lat() > max_deg.lat() ) {
            max_deg.setlat( p.lat() );
        }
    }

    // extend the min/max coordinates of airport area to cover all
    // lights as well
    for ( i = 0; i < (int)rwy_lights.size(); ++i ) {
        for ( j = 0;
              j < (int)rwy_lights[i].get_poly().get_contour(0).size();
              ++j )
        {
            Point3D p = rwy_lights[i].get_poly().get_contour(0)[j];
            if ( p.lon() < min_deg.lon() ) {
                min_deg.setlon( p.lon() );
            }
            if ( p.lon() > max_deg.lon() ) {
                max_deg.setlon( p.lon() );
            }
            if ( p.lat() < min_deg.lat() ) {
                min_deg.setlat( p.lat() );
            }
            if ( p.lat() > max_deg.lat() ) {
                max_deg.setlat( p.lat() );
            }
        }
    }

    // Extend the area a bit so we don't have wierd things on the edges
    double dlon = max_deg.lon() - min_deg.lon();
    double dlat = max_deg.lat() - min_deg.lat();
    min_deg.setlon( min_deg.lon() - 0.1 * dlon );
    max_deg.setlon( max_deg.lon() + 0.1 * dlon );
    min_deg.setlat( min_deg.lat() - 0.1 * dlat );
    max_deg.setlat( max_deg.lat() + 0.1 * dlat );

    TGAptSurface apt_surf( root, elev_src, min_deg, max_deg );
    cout << "Surface created" << endl;

    // calculate node elevations
    point_list geod_nodes = calc_elevations( apt_surf, nodes.get_node_list(),
                                             0.0 );
    divided_base = calc_elevations( apt_surf, divided_base, 0.0 );
    cout << "DIVIDED" << endl;
    cout << divided_base << endl;

    SG_LOG(SG_GENERAL, SG_DEBUG, "Done with base calc_elevations()");

    // add base skirt (to hide potential cracks)
    //
    // this has to happen after we've calculated the node elevations
    // but before we convert to wgs84 coordinates
    int uindex, lindex;

    for ( i = 0; i < divided_base.contours(); ++i ) {
	strip_v.clear();
	strip_n.clear();
	strip_tc.clear();

	// prime the pump ...
	p = divided_base.get_pt( i, 0 );
	uindex = nodes.find( p );
	if ( uindex >= 0 ) {
	    Point3D lower = geod_nodes[uindex] - Point3D(0, 0, 20);
	    SG_LOG(SG_GENERAL, SG_DEBUG, geod_nodes[uindex] << " <-> " << lower);
	    lindex = nodes.simple_add( lower );
	    geod_nodes.push_back( lower );
	    strip_v.push_back( uindex );
	    strip_v.push_back( lindex );

	    // use 'the' normal.  We are pushing on two nodes so we
	    // need to push on two normals.
	    index = normals.unique_add( vn );
	    strip_n.push_back( index );
	    strip_n.push_back( index );
	} else {
            string message = "Ooops missing node when building skirt (in init)";
            SG_LOG( SG_GENERAL, SG_INFO, message << " " << p );
	    throw sg_exception( message );
	}

	// loop through the list
	for ( j = 1; j < divided_base.contour_size(i); ++j ) {
	    p = divided_base.get_pt( i, j );
	    uindex = nodes.find( p );
	    if ( uindex >= 0 ) {
		Point3D lower = geod_nodes[uindex] - Point3D(0, 0, 20);
		SG_LOG(SG_GENERAL, SG_DEBUG, geod_nodes[uindex] << " <-> " << lower);
		lindex = nodes.simple_add( lower );
		geod_nodes.push_back( lower );
		strip_v.push_back( lindex );
		strip_v.push_back( uindex );

		index = normals.unique_add( vn );
		strip_n.push_back( index );
		strip_n.push_back( index );
	    } else {
                string message
                    = "Ooops missing node when building skirt (in loop)";
                SG_LOG( SG_GENERAL, SG_INFO, message << " " << p );
                throw sg_exception( message );
	    }
	}

	// close off the loop
	p = divided_base.get_pt( i, 0 );
	uindex = nodes.find( p );
	if ( uindex >= 0 ) {
	    Point3D lower = geod_nodes[uindex] - Point3D(0, 0, 20);
	    SG_LOG(SG_GENERAL, SG_DEBUG, geod_nodes[uindex] << " <-> " << lower);
	    lindex = nodes.simple_add( lower );
	    geod_nodes.push_back( lower );
	    strip_v.push_back( lindex );
	    strip_v.push_back( uindex );

	    index = normals.unique_add( vn );
	    strip_n.push_back( index );
	    strip_n.push_back( index );
	} else {
            string message = "Ooops missing node when building skirt (at end)";
            SG_LOG( SG_GENERAL, SG_INFO, message << " " << p );
            throw sg_exception( message );
	}

	strips_v.push_back( strip_v );
	strips_n.push_back( strip_n );
	strip_materials.push_back( "Grass" );

	base_txs.clear();
	base_txs = sgCalcTexCoords( b, nodes.get_node_list(), strip_v );

	base_tc.clear();
	for ( j = 0; j < (int)base_txs.size(); ++j ) {
	    tc = base_txs[j];
	    // SG_LOG(SG_GENERAL, SG_DEBUG, "base_tc = " << tc);
	    index = texcoords.simple_add( tc );
	    base_tc.push_back( index );
	}
	strips_tc.push_back( base_tc );
    }

    // add light points

    superpoly_list tmp_light_list; tmp_light_list.clear();
    typedef map < string, double, less<string> > elev_map_type;
    typedef elev_map_type::const_iterator const_elev_map_iterator;
    elev_map_type elevation_map;

    // pass one, calculate raw elevations from Array

    for ( i = 0; i < (int)rwy_lights.size(); ++i ) {
        TGTriNodes light_nodes;
        light_nodes.clear();
        point_list lights_v = rwy_lights[i].get_poly().get_contour(0);
        for ( j = 0; j < (int)lights_v.size(); ++j ) {
            p = lights_v[j];
            index = light_nodes.simple_add( p );
        }

        // calculate light node elevations

        point_list geod_light_nodes
            = calc_elevations( apt_surf, light_nodes.get_node_list(), 0.5 );
        TGPolygon p;
        p.add_contour( geod_light_nodes, 0 );
        TGSuperPoly s;
        s.set_poly( p );
        tmp_light_list.push_back( s );

        string flag = rwy_lights[i].get_flag();
        if ( flag != (string)"" ) {
            double max = -9999;
            const_elev_map_iterator it = elevation_map.find( flag );
            if ( it != elevation_map.end() ) {
                max = elevation_map[flag];
            }
            for ( j = 0; j < (int)geod_light_nodes.size(); ++j ) {
                if ( geod_light_nodes[j].z() > max ) {
                    max = geod_light_nodes[j].z();
                }
            }
            elevation_map[flag] = max;
            SG_LOG( SG_GENERAL, SG_INFO, flag << " max = " << max );
        }
    }

    SG_LOG(SG_GENERAL, SG_DEBUG, "Done with lighting calc_elevations()");

    // pass two, for each light group check if we need to lift (based
    // on flag) and do so, then output next structures.
    for ( i = 0; i < (int)rwy_lights.size(); ++i ) {
        // tmp_light_list is a parallel structure to rwy_lights
        point_list geod_light_nodes
            = tmp_light_list[i].get_poly().get_contour(0);

#if 0
        // This code forces the elevation of all the approach lighting
        // components for a particular runway end up to the highest
        // max elevation for any of the points.  That can cause other
        // problem so let's nuke code this for the moment.
        string flag = rwy_lights[i].get_flag();
        if ( flag != (string)"" ) {
            const_elev_map_iterator it = elevation_map.find( flag );
            if ( it != elevation_map.end() ) {
                double force_elev = elevation_map[flag];
                for ( j = 0; j < (int)geod_light_nodes.size(); ++j ) {
                    geod_light_nodes[j].setz( force_elev );
                }
            }
        }
#endif
        
        // this is a little round about, but what we want to calculate the
        // light node elevations as ground + an offset so we do them
        // seperately, then we add them back into nodes to get the index
        // out, but also add them to geod_nodes to maintain consistancy
        // between these two lists.
        point_list light_normals = rwy_lights[i].get_normals().get_contour(0);
        pt_v.clear();
        pt_n.clear();
        for ( j = 0; j < (int)geod_light_nodes.size(); ++j ) {
            p = geod_light_nodes[j];
            index = nodes.simple_add( p );
            pt_v.push_back( index );
            geod_nodes.push_back( p );

            index = normals.unique_add( light_normals[j] );
            pt_n.push_back( index );
        }
        pts_v.push_back( pt_v );
        pts_n.push_back( pt_n );
        pt_materials.push_back( rwy_lights[i].get_material() );
    }

    // calculate wgs84 mapping of nodes
    point_list wgs84_nodes;
    for ( i = 0; i < (int)geod_nodes.size(); ++i ) {
	p.setx( geod_nodes[i].x() * SGD_DEGREES_TO_RADIANS );
	p.sety( geod_nodes[i].y() * SGD_DEGREES_TO_RADIANS );
	p.setz( geod_nodes[i].z() );
	wgs84_nodes.push_back( sgGeodToCart( p ) );
    }
    float gbs_radius = sgCalcBoundingRadius( gbs_center, wgs84_nodes );
    SG_LOG(SG_GENERAL, SG_DEBUG, "Done with wgs84 node mapping");

    // null structures
    group_list fans_v; fans_v.clear();
    group_list fans_n; fans_n.clear();
    group_list fans_tc; fans_tc.clear();
    string_list fan_materials; fan_materials.clear();

    string objpath = root + "/AirportObj";
    string name = airport_id + ".btg";

    SGBinObject obj;

    obj.set_gbs_center( gbs_center );
    obj.set_gbs_radius( gbs_radius );
    obj.set_wgs84_nodes( wgs84_nodes );
    obj.set_normals( normals.get_node_list() );
    obj.set_texcoords( texcoords.get_node_list() );
    obj.set_pts_v( pts_v );
    obj.set_pts_n( pts_n );
    obj.set_pt_materials( pt_materials );
    obj.set_tris_v( tris_v );
    obj.set_tris_n( tris_n );
    obj.set_tris_tc( tris_tc ); 
    obj.set_tri_materials( tri_materials );
    obj.set_strips_v( strips_v );
    obj.set_strips_n( strips_n );
    obj.set_strips_tc( strips_tc ); 
    obj.set_strip_materials( strip_materials );
    obj.set_fans_v( fans_v );
    obj.set_fans_n( fans_n );
    obj.set_fans_tc( fans_tc );
    obj.set_fan_materials( fan_materials );

    bool result;
    result = obj.write_bin( objpath, name, b );
    if ( !result ) {
        throw sg_exception("error writing file. :-(");
    }

#if 0
    // checking result of write, remove the read_bin() before this
    // goes into production
    string file = objpath + "/" + b.gen_base_path() + "/" + name;
    point_list tmp_texcoords = texcoords.get_node_list();
    sgReadBinObj( file, gbs_center, &gbs_radius, 
		  wgs84_nodes, normals,
		  tmp_texcoords, 
		  tris_v, tris_tc, tri_materials,
		  strips_v, strips_tc, strip_materials, 
		  fans_v, fans_tc, fan_materials );
#endif

    write_index( objpath, b, name );

    string holepath = root + "/AirportArea";
    // long int poly_index = poly_index_next();
    // write_boundary( holepath, b, hull, poly_index );
    tgSplitPolygon( holepath, HoleArea, divided_base, true );
    tgSplitPolygon( holepath, AirportArea, apt_clearing, false );
}
