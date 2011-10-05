
#include <simgear/compiler.h>
#include <simgear/structure/exception.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/bucket/newbucket.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>

#include <Geometry/poly_support.hxx>
#include <Polygon/polygon.hxx>

#include "beznode.hxx"
#include "runway.hxx"

extern int nudge;

Runway::Runway(char* definition)
{
    double az2;

    // format:
    // runway   width   surface  shoulder  smoothness centerline lights  edge lighting distance remaining signs
    // 100      46.02   2        1         0.00       1                  2             1 
    //     
    //          runway number  runway end lat runway end long   threshold  overrun  markings  approach lighting
    //          09L            33.63470475     -084.44798671    0.00       120.09   3         7 
    // 
    //          touchdown zone lighting  runway end identifier lights
    //          0                        1                              
    //
    //          runway number  runway end lat runway end long   threshold  overrun  markings  approach lighting
    //          27R             33.63469907   -084.40893004     0.00       120.09   3         6 
    //
    //          touchdown zone lighting  runway end identifier lights
    //          0                        1

    // Parse the line
    // 46.02   2   1 0.00 1 2 1 09L  33.63470475 -084.44798671    0.00  120.09 3  7 0 1 27R  33.63469907 -084.40893004    0.00  120.09 3  6 0 1

    // int fscanf(FILE *stream, const char *format, ...);
    sscanf(definition, "%lf %d %d %lf %d %d %d %s %lf %lf %lf %lf %d %d %d %d %s %lf %lf %lf %lf %d %d %d %d", 
        &rwy.width,  &rwy.surface, &rwy.shoulder, &rwy.smoothness, &rwy.centerline_lights, &rwy.edge_lights, &rwy.dist_remain_signs,
        &rwy.rwnum[0], &rwy.lat[0], &rwy.lon[0], &rwy.threshold[0], &rwy.overrun[0], &rwy.marking[0], &rwy.approach_lights[0], &rwy.tz_lights[0], &rwy.reil[0],
        &rwy.rwnum[1], &rwy.lat[1], &rwy.lon[1], &rwy.threshold[1], &rwy.overrun[1], &rwy.marking[1], &rwy.approach_lights[1], &rwy.tz_lights[1], &rwy.reil[1]
    );

    // calculate runway heading and length (used a lot)
    geo_inverse_wgs_84( rwy.lat[0], rwy.lon[0], rwy.lat[1], rwy.lon[1], &rwy.heading, &az2, &rwy.length );

    SG_LOG(SG_GENERAL, SG_DEBUG, "Read runway: (" << rwy.lon[0] << "," << rwy.lat[0] << ") to (" << rwy.lon[1] << "," << rwy.lat[1] << ") heading: " << rwy.heading << " length: " << rwy.length << " width: " << rwy.width );
} 

int Runway::BuildOsg ( osg::Group* airport )
{
    // calculated distance, and azimuths
    double az1, az2, dist;

    // rectangle verticies
    double v0_lat, v0_lon, v1_lat, v1_lon, v2_lat, v2_lon, v3_lat, v3_lon;

    // Create a node for the runway
    osg::Geode* geode = new osg::Geode();

    osg::Geometry* geometry = new osg::Geometry;
    osg::Vec3dArray& v = *(new osg::Vec3dArray(4));
    
    // first, find the runway direction vector
    // static int geo_inverse_wgs_84( double lat1, double lon1, double lat2, double lon2, double *az1, double *az2, double *s )
    geo_inverse_wgs_84( rwy.lat[0], rwy.lon[0], rwy.lat[1], rwy.lon[1], &az1, &az2, &dist);

    // now, need to caculate the 4 verticies
    // static int geo_direct_wgs_84( double lat1, double lon1, double az1, double s, double *lat2, double *lon2, double *az2 )
    geo_direct_wgs_84( rwy.lat[0], rwy.lon[0], az1+90, rwy.width/2, &v0_lat, &v0_lon, &az2 );
    geo_direct_wgs_84( v0_lat, v0_lon, az1, dist, &v1_lat, &v1_lon, &az2 );
    geo_direct_wgs_84( v1_lat, v1_lon, az1-90, rwy.width, &v2_lat, &v2_lon, &az2 );
    geo_direct_wgs_84( v2_lat, v2_lon, az1+180, dist, &v3_lat, &v3_lon, &az2 );

    // convert from lat/lon to geodisc
    // (maybe later) - check some simgear objects...
    v[0].set(v0_lat, v0_lon, 0.0f);
    v[1].set(v1_lat, v1_lon, 0.0f);
    v[2].set(v2_lat, v2_lon, 0.0f);
    v[3].set(v3_lat, v3_lon, 0.0f);

    geometry->setVertexArray(&v);

    osg::DrawElementsUShort& drawElements = *(new osg::DrawElementsUShort(GL_TRIANGLE_STRIP,4));
    geometry->addPrimitiveSet(&drawElements);
    
    drawElements[0] = 3;
    drawElements[1] = 0;
    drawElements[2] = 2;
    drawElements[3] = 1;

    geode->addDrawable(geometry);

    airport->addChild( geode );

    return 0;
}

TGPolygon Runway::gen_wgs84_area(   Point3D origin,
                                    double length_m,
                                    double displ1, double displ2,
                                    double width_m,
                                    double heading_deg,
                                    double alt_m,
                                    bool   add_mid )
{
    TGPolygon result_list;
    double length_hdg = heading_deg;
    double left_hdg = length_hdg - 90.0;
    if ( left_hdg < 0 ) { left_hdg += 360.0; }

    // move to the +l end/center of the runway
    Point3D ref = origin;
    double lon, lat, r;
    geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), length_hdg, 
                        length_m / 2.0 - displ2, &lat, &lon, &r );
    ref = Point3D( lon, lat, 0.0 );

    // move to the l,-w corner (then we add points in a clockwise direction)
    geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), left_hdg, 
                        -width_m / 2.0, &lat, &lon, &r );
    Point3D p = Point3D( lon, lat, 0.0 );
    result_list.add_node( 0, p );

    // move to the l,w corner
    geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), left_hdg, 
                        width_m / 2.0, &lat, &lon, &r );
    p = Point3D( lon, lat, 0.0 );
    result_list.add_node( 0, p );

    if ( add_mid ) {
        // move to the 0,w point (then we add points in a clockwise direction)

        ref = origin;
        geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), left_hdg, 
                            width_m / 2.0, &lat, &lon, &r );
        p = Point3D( lon, lat, 0.0 );
        result_list.add_node( 0, p );
    }

    // move to the -l end/center of the runway
    ref = origin;
    geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), length_hdg, 
                        displ1 - length_m/2.0, &lat, &lon, &r );
    ref = Point3D( lon, lat, 0.0 );

    // move to the -l,w corner (then we add points in a clockwise direction)
    geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), left_hdg, 
                        width_m / 2.0, &lat, &lon, &r );
    p = Point3D( lon, lat, 0.0 );
    result_list.add_node( 0, p );

    // move to the -l,-w corner
    geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), left_hdg, 
                        -width_m / 2.0, &lat, &lon, &r );
    p = Point3D( lon, lat, 0.0 );
    result_list.add_node( 0, p );

    if ( add_mid ) {
        // move to the 0,-w point (then we add points in a clockwise direction)

        ref = origin;
        geo_direct_wgs_84 ( alt_m, ref.lat(), ref.lon(), left_hdg, 
                            -width_m / 2.0, &lat, &lon, &r );
        p = Point3D( lon, lat, 0.0 );
        result_list.add_node( 0, p );
    }

    return result_list;
}


// generate an area for a runway with expansion specified in meters
// (return result points in degrees)
TGPolygon Runway::gen_runway_area_w_extend( double alt_m, double length_extend, double displ1, double displ2, double width_extend ) 
{
    TGPolygon result_list;
    Point3D origin = GetMidpoint();

    result_list = gen_wgs84_area( origin, rwy.length + 2.0*length_extend, displ1, displ2, rwy.width + 2.0*width_extend, rwy.heading, alt_m, false );

    // display points
    SG_LOG(SG_GENERAL, SG_DEBUG, "Results w/ extend (new way)");
    for ( int i = 0; i < result_list.contour_size( 0 ); ++i ) 
    {
        SG_LOG(SG_GENERAL, SG_DEBUG, "  " << result_list.get_pt(0, i));
    }

    return result_list;
}


// generate an area for a runway and include midpoints
TGPolygon Runway::gen_runway_w_mid( double alt_m, double length_extend_m, double width_extend_m ) 
{
    TGPolygon result_list;
    Point3D origin = GetMidpoint();

    result_list = gen_wgs84_area( origin, rwy.length + 2.0*length_extend_m, 0.0, 0.0, rwy.width + 2.0 * width_extend_m, rwy.heading, alt_m, true );

    // display points
    SG_LOG(SG_GENERAL, SG_DEBUG, "Results w/ mid (new way)");
    for ( int i = 0; i < result_list.contour_size( 0 ); ++i ) 
    {
        SG_LOG(SG_GENERAL, SG_DEBUG, "  " << result_list.get_pt(0, i));
    }

    return result_list;
}


int Runway::BuildBtg( float alt_m, superpoly_list* rwy_polys, texparams_list* texparams, superpoly_list* rwy_lights, TGPolygon* accum, TGPolygon* apt_base, TGPolygon* apt_clearing )
{
    TGPolygon base, safe_base;
    string material;

    if ( rwy.surface == 1 /* Asphalt */ )
    {
        material = "pa_";
    } 
    else if ( rwy.surface == 2 /* Concrete */ )
    {
        material = "pc_";
    } 
    else if ( rwy.surface == 3 /* Turf/Grass */ )
    {
        material = "grass_rwy";
    } 
    else if ( rwy.surface == 4 /* Dirt */ || rwy.surface == 5 /* Gravel */ )
    {
        material = "dirt_rwy";
    } 
    else if ( rwy.surface == 12 /* Dry Lakebed */ )
    {
        material = "dirt_rwy";
    } 
    else if ( rwy.surface == 13 /* Water runway (buoy's?) */ )
    {
        // water
    } 
    else 
    {
        SG_LOG(SG_GENERAL, SG_WARN, "surface_code = " << rwy.surface);
    	throw sg_exception("unknown runway type!");
    }

    // first, check the surface type - anything but concrete and asphalt are easy
    switch( rwy.surface )
    {
        case 1: // asphalt:
        case 2: // concrete
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: asphalt or concrete" << rwy.surface);
            gen_rwy( alt_m, material, rwy_polys, texparams, accum );
            gen_runway_lights( alt_m, rwy_lights, apt_base );
            break;
    
        case 3: // Grass
        case 4: // Dirt
        case 5: // Gravel
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: Turf, Dirt or Gravel" << rwy.surface );
	        gen_simple_rwy( alt_m, material, rwy_polys, texparams, accum );
                gen_runway_lights( alt_m, rwy_lights, apt_base );
            break;

        case 12: // dry lakebed
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: Dry Lakebed");
            break;

        case 13: // water
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: Water");
            break;

        case 14: // snow
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: Snow");
            break;

        case 15: // transparent
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: transparent");
            break;

        default: // unknown
            SG_LOG( SG_GENERAL, SG_ALERT, "Build Runway: unknown" << rwy.surface);
            break;
    }    

    // generate area around runways
    base      = gen_runway_area_w_extend( 0.0, 20.0, -rwy.overrun[0], -rwy.overrun[1], 20.0 );

    // also clear a safe area around the runway
    safe_base = gen_runway_area_w_extend( 0.0, 180.0, -rwy.overrun[0], -rwy.overrun[1], 50.0 );

    // add this to the airport clearing
    *apt_clearing = tgPolygonUnion(safe_base, *apt_clearing);

    // and add the clearing to the base
    *apt_base = tgPolygonUnion( base, *apt_base );
}