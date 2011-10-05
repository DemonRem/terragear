/* OpenSceneGraph example, osgshaderterrain.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*  THE SOFTWARE.
*/

#include <osg/AlphaFunc>
#include <osg/Billboard>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/GL2Extensions>
#include <osg/Material>
#include <osg/Math>
#include <osg/MatrixTransform>
#include <osg/PolygonMode>
#include <osg/PolygonOffset>
#include <osg/Program>
#include <osg/Projection>
#include <osg/Shader>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/Switch>
#include <osg/Texture2D>
#include <osg/Uniform>

#include <osgDB/ReadFile>
#include <osgDB/FileUtils>

#include <osgUtil/Tessellator>
#include <osgUtil/SmoothingVisitor>
#include <osgText/Text>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgUtil/Optimizer>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/StateSetManipulator>

#include <string.h>
#include <iostream>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>

#include <simgear/misc/sgstream.hxx>
//#include <simgear/misc/strutils.hxx>

#include <Polygon/index.hxx>

#include "beznode.hxx"
#include "closedpoly.hxx"
#include "linearfeature.hxx"
#include "parser.hxx"

// TODO : Modularize this function
// IDEAS 
// 1) Start / Stop MarkStyle
// 2) Start / Stop LightStyle
// 3) holes
// 4) CreatePavementSS(pavement type)
// 5) CreateMarkingSS(marking type)
// 6) CalcStripeOffsets should be AddMarkingVerticies, and take the 2 distances from pavement edge...
// SCRAP ALL IDEAS - Markings are encapsulated in LinearFeature class. 
// Start creates a new object
// End closes the object (and should add it to a list - either in the parser, or ClosedPoly)

// Display usage
static void usage( int argc, char **argv ) {
    SG_LOG(SG_GENERAL, SG_ALERT, 
	   "Usage " << argv[0] << " --input=<apt_file> "
	   << "--work=<work_dir> [ --start-id=abcd ] [ --nudge=n ] "
	   << "[--min-lon=<deg>] [--max-lon=<deg>] [--min-lat=<deg>] [--max-lat=<deg>] "
	   << "[--clear-dem-path] [--dem-path=<path>] [--max-slope=<decimal>] "
           << "[ --airport=abcd ]  [--tile=<tile>] [--chunk=<chunk>] [--verbose] [--help]");
}


void setup_default_elevation_sources(string_list& elev_src) {
    elev_src.push_back( "SRTM2-Africa-3" );
    elev_src.push_back( "SRTM2-Australia-3" );
    elev_src.push_back( "SRTM2-Eurasia-3" );
    elev_src.push_back( "SRTM2-Islands-3" );
    elev_src.push_back( "SRTM2-North_America-3" );
    elev_src.push_back( "SRTM2-South_America-3" );
    elev_src.push_back( "DEM-USGS-3" );
    elev_src.push_back( "SRTM-1" );
    elev_src.push_back( "SRTM-3" );
    elev_src.push_back( "SRTM-30" );
}


// TODO: where do these belong
int nudge = 10;
double slope_max = 0.2;

int main(int argc, char **argv)
{
    float min_lon = -180;
    float max_lon = 180;
    float min_lat = -90;
    float max_lat = 90;
    bool  ready_to_go = true;
    bool  view_osg = false;

    string_list elev_src;
    elev_src.clear();
    setup_default_elevation_sources(elev_src);

    // Set verbose
//    sglog().setLogLevels( SG_GENERAL, SG_BULK );
    sglog().setLogLevels( SG_GENERAL, SG_INFO );

    SG_LOG(SG_GENERAL, SG_INFO, "Run genapt");

    // parse arguments
    string work_dir = "";
    string input_file = "";
    string start_id = "";
    string airport_id = "";
    int arg_pos;

    for (arg_pos = 1; arg_pos < argc; arg_pos++) 
    {
        string arg = argv[arg_pos];
        if ( arg.find("--work=") == 0 ) 
        {
            work_dir = arg.substr(7);
    	} 
        else if ( arg.find("--input=") == 0 ) 
        {
	        input_file = arg.substr(8);
        } 
        else if ( arg.find("--terrain=") == 0 ) 
        {
            elev_src.push_back( arg.substr(10) );
     	} 
        else if ( arg.find("--start-id=") == 0 ) 
        {
    	    start_id = arg.substr(11);
    	    ready_to_go = false;
     	} 
        else if ( arg.find("--nudge=") == 0 ) 
        {
    	    nudge = atoi( arg.substr(8).c_str() );
    	} 
        else if ( arg.find("--min-lon=") == 0 ) 
        {
    	    min_lon = atof( arg.substr(10).c_str() );
    	} 
        else if ( arg.find("--max-lon=") == 0 ) 
        {
    	    max_lon = atof( arg.substr(10).c_str() );
    	} 
        else if ( arg.find("--min-lat=") == 0 ) 
        {
    	    min_lat = atof( arg.substr(10).c_str() );
    	} 
        else if ( arg.find("--max-lat=") == 0 ) 
        {
    	    max_lat = atof( arg.substr(10).c_str() );
        } 
#if 0
        else if ( arg.find("--chunk=") == 0 ) 
        {
            tg::Rectangle rectangle = tg::parseChunk(arg.substr(8).c_str(), 10.0);
            min_lon = rectangle.getMin().x();
            min_lat = rectangle.getMin().y();
            max_lon = rectangle.getMax().x();
            max_lat = rectangle.getMax().y();
        } 
        else if ( arg.find("--tile=") == 0 ) 
        {
            tg::Rectangle rectangle = tg::parseTile(arg.substr(7).c_str());
            min_lon = rectangle.getMin().x();
            min_lat = rectangle.getMin().y();
            max_lon = rectangle.getMax().x();
            max_lat = rectangle.getMax().y();
    	} 
#endif
        else if ( arg.find("--airport=") == 0 ) 
        {
    	    airport_id = arg.substr(10).c_str();
    	    ready_to_go = false;
    	} 
        else if ( arg == "--clear-dem-path" ) 
        {
    	    elev_src.clear();
    	} 
        else if ( arg.find("--dem-path=") == 0 ) 
        {
    	    elev_src.push_back( arg.substr(11) );
    	} 
        else if ( (arg.find("--verbose") == 0) || (arg.find("-v") == 0) ) 
        {
    	    sglog().setLogLevels( SG_GENERAL, SG_BULK );
    	} 
        else if ( arg.find("--view") == 0 )
        {
            SG_LOG(SG_GENERAL, SG_INFO, "Found --view : view OSG model" );
            view_osg = true;
        }
        else if ( (arg.find("--max-slope=") == 0) ) 
        {
    	    slope_max = atof( arg.substr(12).c_str() );
    	} 
#if 0
        else if ( (arg.find("--help") == 0) || (arg.find("-h") == 0) ) 
        {
    	    help( argc, argv, elev_src );
    	    exit(-1);
    	} 
#endif
        else 
        {
    	    usage( argc, argv );
    	    exit(-1);
    	}
    }

    SG_LOG(SG_GENERAL, SG_INFO, "Input file = " << input_file);
    SG_LOG(SG_GENERAL, SG_INFO, "Terrain sources = ");
    for ( unsigned int i = 0; i < elev_src.size(); ++i ) 
    {
        SG_LOG(SG_GENERAL, SG_INFO, "  " << work_dir << "/" << elev_src[i] );
    }
    SG_LOG(SG_GENERAL, SG_INFO, "Work directory = " << work_dir);
    SG_LOG(SG_GENERAL, SG_INFO, "Nudge = " << nudge);
    SG_LOG(SG_GENERAL, SG_INFO, "Longitude = " << min_lon << ':' << max_lon);
    SG_LOG(SG_GENERAL, SG_INFO, "Latitude = " << min_lat << ':' << max_lat);

    if (max_lon < min_lon || max_lat < min_lat ||
	    min_lat < -90 || max_lat > 90 ||
	    min_lon < -180 || max_lon > 180) 
    {
        SG_LOG(SG_GENERAL, SG_ALERT, "Bad longitude or latitude");
    	exit(1);
    }

    if ( work_dir == "" ) 
    {
    	SG_LOG( SG_GENERAL, SG_ALERT, "Error: no work directory specified." );
    	usage( argc, argv );
	    exit(-1);
    }

    if ( input_file == "" ) 
    {
    	SG_LOG( SG_GENERAL, SG_ALERT,  "Error: no input file." );
    	exit(-1);
    }

    // make work directory
    SG_LOG(SG_GENERAL, SG_INFO, "Creating airportarea directory");

    string airportareadir=work_dir+"/AirportArea";
    SGPath sgp( airportareadir );
    sgp.append( "dummy" );
    sgp.create_dir( 0755 );
    
    string lastaptfile = work_dir+"/last_apt";

    // initialize persistant polygon counter
    string counter_file = airportareadir+"/poly_counter";
    poly_index_init( counter_file );

    sg_gzifstream in( input_file );
    if ( !in.is_open() ) 
    {
        SG_LOG( SG_GENERAL, SG_ALERT, "Cannot open file: " << input_file );
        exit(-1);
    }

    SG_LOG(SG_GENERAL, SG_INFO, "Creating parser");

    // Create the parser...
    Parser* parser = new Parser(input_file);

    SG_LOG(SG_GENERAL, SG_INFO, "Parse katl");
    parser->Parse((char*)"edfe");

    if (view_osg)
    {
        // just view in OSG
        osg::Group* airportNode;

        SG_LOG(SG_GENERAL, SG_INFO, "View OSG");
        airportNode = parser->CreateOsgGroup();

        // construct the viewer.
        osgViewer::Viewer viewer;

        // add the thread model handler
        viewer.addEventHandler(new osgViewer::ThreadingHandler);

        // add the window size toggle handler
        viewer.addEventHandler(new osgViewer::WindowSizeHandler);

        // add model to viewer.
        viewer.setSceneData( airportNode );
        viewer.setUpViewAcrossAllScreens();

        // create the windows and run the threads.
        viewer.realize();
    
        viewer.setCameraManipulator(new osgGA::TrackballManipulator());

        osgUtil::Optimizer optimzer;
        optimzer.optimize(airportNode);

        viewer.run();    
    }
    else
    {
        // write a .btg file....
        SG_LOG(SG_GENERAL, SG_INFO, "Write BTG");
        parser->WriteBtg(work_dir, elev_src);
    }

    SG_LOG(SG_GENERAL, SG_INFO, "Done");

    return 0;
}
