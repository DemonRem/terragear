#ifndef _PARSER_H_
#define _PARSER_H_

#include "beznode.hxx"
#include "closedpoly.hxx"
#include "linearfeature.hxx"
#include "runway.hxx"
#include "airport.hxx"

#define STATE_NONE                  (0)
#define STATE_PARSE_SIMPLE          (1)
#define STATE_PARSE_BOUNDARY        (2)
#define STATE_PARSE_PAVEMENT        (3)
#define STATE_PARSE_FEATURE         (4)
#define STATE_DONE                  (10)

#define MAXLINE (256)

#define LAND_AIRPORT_CODE               (1)
#define SEA_AIRPORT_CODE                (16)
#define HELIPORT_CODE                   (17)

#define LAND_RUNWAY_CODE                (100)
#define WATER_RUNWAY_CODE               (101)
#define HELIPAD_CODE                    (102)

#define PAVEMENT_CODE                   (110)
#define LINEAR_FEATURE_CODE             (120)
#define BOUNDRY_CODE                    (130)

#define NODE_CODE                       (111)
#define BEZIER_NODE_CODE                (112)
#define CLOSE_NODE_CODE                 (113)
#define CLOSE_BEZIER_NODE_CODE          (114)
#define TERM_NODE_CODE                  (115)
#define TERM_BEZIER_NODE_CODE           (116)

#define AIRPORT_VIEWPOINT_CODE          (14)
#define AIRPLANE_STARTUP_LOCATION_CODE  (15)
#define LIGHT_BEACON_CODE               (18)
#define WINDSOCK_CODE                   (19)
#define TAXIWAY_SIGN                    (20)
#define LIGHTING_OBJECT                 (21)

#define COMM_FREQ1_CODE                 (50)               
#define COMM_FREQ2_CODE                 (51)               
#define COMM_FREQ3_CODE                 (52)               
#define COMM_FREQ4_CODE                 (53)               
#define COMM_FREQ5_CODE                 (54)               
#define COMM_FREQ6_CODE                 (55)               
#define COMM_FREQ7_CODE                 (56)               

#define END_OF_FILE                     (99)

class Parser
{
public:
    Parser(string& f)
    {
        filename        = f;
        cur_airport     = NULL;
        cur_pavement    = NULL;
        cur_feat        = NULL;
        prev_node       = NULL;
        cur_state       = STATE_NONE;
    }
    
    int             Parse( char* icao );
    void            WriteBtg( const string& root, const string_list& elev_src );
    osg::Group*     CreateOsgGroup( void );
    
private:
    int             SetState( int state );

    BezNode*        ParseNode( int type, char* line, BezNode* prevNode );
    LinearFeature*  ParseFeature( char* line );
    ClosedPoly*     ParsePavement( char* line );
    osg::Geode*     ParseRunway(char* line );
    int             ParseLine( char* line );

//    int ParseBoundry(char* line, Node* airport);

    string          filename;

    // a polygon conists of an array of contours 
    // (first is outside boundry, remaining are holes)
    Airport*        cur_airport;
    Runway*         cur_runway;
    ClosedPoly*     cur_pavement;
    LinearFeature*  cur_feat;
    BezNode*        prev_node;
    LightingObj*    cur_object;

    AirportList     airports;

    int cur_state;
};

#endif
