#include <simgear/debug/logstream.hxx>

#include "tg_polygon.hxx"

#define DEBUG_POLY_CLEAN    SG_INFO

#if 0
tgPolygon tgPolygon::Snap( const tgPolygon& subject, double snap )
{
    tgPolygon result;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for (unsigned int c = 0; c < subject.Contours(); c++) {
        result.AddContour( tgContour::Snap( subject.GetContour( c ), snap ) );
    }

    return result;
}
#endif

void tgPolygon::Snap( double snap )
{
    for (unsigned int c = 0; c < contours.size(); c++) {
        contours[c].Snap( snap );
    }
}

#if 0
tgPolygon tgPolygon::RemoveDups( const tgPolygon& subject )
{
    tgPolygon result;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for ( unsigned int c = 0; c < subject.Contours(); c++ ) {
        result.AddContour( tgContour::RemoveDups( subject.GetContour( c ) ) );
    }

    return result;
}
#endif

unsigned int tgPolygon::RemoveDups( void )
{
    unsigned int total_removed = 0;
    
    for ( unsigned int c = 0; c < Contours(); c++ ) {
        total_removed += contours[c].RemoveDups();
    }
    
    return total_removed;
}

#if 0
tgPolygon tgPolygon::RemoveBadContours( const tgPolygon& subject )
{
    tgPolygon result;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for ( unsigned int c = 0; c < subject.Contours(); c++ ) {
        tgContour contour = subject.GetContour(c);
        if ( contour.GetSize() >= 3 ) {
            /* keeping the contour */
            result.AddContour( contour );
        }
    }

    return result;
}
#endif

unsigned int tgPolygon::RemoveBadContours( void )
{
    tgcontour_list_iterator it = contours.begin();
    unsigned int before = contours.size();
    
    while( it != contours.end() ) {
        if ( (*it).GetSize() < 3 ) {
            it = contours.erase( it );
        } else {
            it++;
        }
    }
    
    if ( before != contours.size() ) {
        SG_LOG(SG_GENERAL, DEBUG_POLY_CLEAN, "RemoveBadContours before: " << before << " after: " << contours.size() );
    }
    
    return before - contours.size();
}

tgPolygon tgPolygon::RemoveCycles( const tgPolygon& subject )
{
    tgPolygon result;
    tgcontour_list contours;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for ( unsigned int c = 0; c < subject.Contours(); c++ ) {
        contours.clear();

        // sglog().setLogLevels( SG_ALL, SG_DEBUG );
        SG_LOG(SG_GENERAL, SG_DEBUG, " REMOVE DUPES FOR CONTOUR : " << c << " with " << subject.ContourSize(c) << " nodes ");
        tgContour::RemoveCycles( subject.GetContour( c ), contours );
        SG_LOG(SG_GENERAL, SG_DEBUG, " REMOVE DUPES FOR CONTOUR : " << c << " COMPLETE");
        //sglog().setLogLevels( SG_ALL, SG_INFO );

        for ( unsigned int i=0; i< contours.size(); i++ ) {
            result.AddContour( contours[i] );
        }
    }

    return result;
}

tgPolygon tgPolygon::SplitLongEdges( const tgPolygon& subject, double dist )
{
    tgPolygon result;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for ( unsigned c = 0; c < subject.Contours(); c++ )
    {
        result.AddContour( tgContour::SplitLongEdges( subject.GetContour(c), dist ) );
    }

    return result;
}

tgPolygon tgPolygon::StripHoles( const tgPolygon& subject )
{
    tgPolygon result;
    UniqueSGGeodSet all_nodes;

    /* before diff - gather all nodes */
    for ( unsigned int i = 0; i < subject.Contours(); ++i ) {
        for ( unsigned int j = 0; j < subject.ContourSize( i ); ++j ) {
            all_nodes.add( subject.GetNode(i, j) );
        }
    }

    ClipperLib::Paths clipper_result;
    ClipperLib::Clipper c;
    c.Clear();

    for ( unsigned int i = 0; i < subject.Contours(); i++ ) {
        tgContour contour = subject.GetContour( i );
        if ( !contour.GetHole() ) {
            c.AddPath( tgContour::ToClipper( contour ), ClipperLib::ptClip, true );
        }
    }
    c.Execute(ClipperLib::ctUnion, clipper_result, ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd);

    result = tgPolygon::FromClipper( clipper_result );
    result = tgPolygon::AddColinearNodes( result, all_nodes );

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    return result;
}

tgPolygon tgPolygon::Simplify( const tgPolygon& subject )
{
    tgPolygon result;
    UniqueSGGeodSet all_nodes;

    /* before diff - gather all nodes */
    for ( unsigned int i = 0; i < subject.Contours(); ++i ) {
        for ( unsigned int j = 0; j < subject.ContourSize( i ); ++j ) {
            all_nodes.add( subject.GetNode(i, j) );
        }
    }

    ClipperLib::Paths clipper_poly = tgPolygon::ToClipper( subject );
    SimplifyPolygons( clipper_poly );

    result = tgPolygon::FromClipper( clipper_poly );
    result = tgPolygon::AddColinearNodes( result, all_nodes );

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    return result;
}

tgPolygon tgPolygon::RemoveColinearNodes( const tgPolygon& subject )
{
    tgPolygon result;

    ClipperLib::Paths clipper_poly = tgPolygon::ToClipper( subject );
    SimplifyPolygons( clipper_poly );

    result = tgPolygon::FromClipper( clipper_poly );

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    return result;
}

tgPolygon tgPolygon::RemoveTinyContours( const tgPolygon& subject )
{
    double min_area = SG_EPSILON*SG_EPSILON;
    tgPolygon result;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for ( unsigned int c = 0; c < subject.Contours(); c++ ) {
        tgContour contour = subject.GetContour( c );
        double area = contour.GetArea();

        if ( area >= min_area) {
            SG_LOG(SG_GENERAL, SG_DEBUG, "remove_tiny_contours NO - " << c << " area is " << area << " requirement is " << min_area);
            result.AddContour( contour );
        } else {
            SG_LOG(SG_GENERAL, SG_DEBUG, "remove_tiny_contours " << c << " area is " << area << ": removing");
        }
    }

    return result;
}

tgPolygon tgPolygon::RemoveSpikes( const tgPolygon& subject )
{
    tgPolygon result;

    result.SetMaterial( subject.GetMaterial() );
    result.SetTexParams( subject.GetTexParams() );
    result.SetId( subject.GetId() );
    result.SetPreserve3D( subject.GetPreserve3D() );
    result.va_int_mask = subject.va_int_mask;
    result.va_flt_mask = subject.va_flt_mask;
    result.int_vas = subject.int_vas;
    result.flt_vas = subject.flt_vas;
    
    for ( unsigned int c = 0; c < subject.Contours(); c++ ) {
        result.AddContour( tgContour::RemoveSpikes( subject.GetContour(c) ) );
    }

    return result;
}

// Move slivers from in polygon to out polygon.
void tgPolygon::RemoveSlivers( tgPolygon& subject, tgcontour_list& slivers )
{
    // traverse each contour of the polygon and attempt to identify
    // likely slivers
    SG_LOG(SG_GENERAL, SG_DEBUG, "tgPolygon::RemoveSlivers()");

    tgPolygon result;
    tgContour contour;
    int       i;

    double angle_cutoff = 10.0 * SGD_DEGREES_TO_RADIANS;
    double area_cutoff = 0.000000001;
    double min_angle;
    double area;

    // process contours in reverse order so deleting a contour doesn't
    // foul up our sequence
    for ( i = subject.Contours() - 1; i >= 0; --i ) {
        SG_LOG(SG_GENERAL, SG_DEBUG, "contour " << i );

        contour   = subject.GetContour(i);

        SG_LOG(SG_GENERAL, SG_DEBUG, "  calc min angle for contour " << i);
        min_angle = contour.GetMinimumAngle();
        SG_LOG(SG_GENERAL, SG_DEBUG, "  min_angle (rad) = " << min_angle );

        area      = contour.GetArea();
        SG_LOG(SG_GENERAL, SG_DEBUG, "  area = " << area );

        if ( ((min_angle < angle_cutoff) && (area < area_cutoff)) ||
           ( area < area_cutoff / 10.0) )
        {
            if ((min_angle < angle_cutoff) && (area < area_cutoff))
            {
                SG_LOG(SG_GENERAL, SG_DEBUG, "      WE THINK IT'S A SLIVER! - min angle < 10 deg, and area < 10 sq meters");
            }
            else
            {
                SG_LOG(SG_GENERAL, SG_DEBUG, "      WE THINK IT'S A SLIVER! - min angle > 10 deg, but area < 1 sq meters");
            }

            // Remove the sliver from source
            subject.DeleteContourAt( i );

            // And add it to the slive list if it isn't a hole
            if ( !contour.GetHole() ) {
                // move sliver contour to sliver list
                SG_LOG(SG_GENERAL, SG_DEBUG, "      Found SLIVER!");

                slivers.push_back( contour );
            }
        }
    }
}

tgcontour_list tgPolygon::MergeSlivers( tgpolygon_list& polys, tgcontour_list& sliver_list ) {
    tgPolygon poly, result;
    tgContour sliver;
    tgContour contour;
    tgcontour_list unmerged;
    unsigned int original_contours, result_contours;
    bool done;

    for ( unsigned int i = 0; i < sliver_list.size(); i++ ) {
        sliver = sliver_list[i];
        SG_LOG(SG_GENERAL, SG_DEBUG, "Merging sliver = " << i );

        sliver.SetHole( false );

        done = false;

        // try to merge the slivers with the list of clipped polys
        for ( unsigned int j = 0; j < polys.size() && !done; j++ ) {
            poly = polys[j];
            original_contours = poly.Contours();
            result = tgContour::Union( sliver, poly );
            result_contours = result.Contours();

            if ( original_contours == result_contours ) {
                SG_LOG(SG_GENERAL, SG_DEBUG, "    FOUND a poly to merge the sliver with");
                result.SetMaterial( polys[j].GetMaterial() );
                result.SetTexParams( polys[j].GetTexParams() );
                result.SetId( polys[j].GetId() );
	        result.SetPreserve3D( polys[j].GetPreserve3D() );
                result.va_int_mask = polys[j].va_int_mask;
                result.va_flt_mask = polys[j].va_flt_mask;
                result.int_vas = polys[j].int_vas;
                result.flt_vas = polys[j].flt_vas;
                polys[j] = result;
                done = true;
            }
        }

        if ( !done ) {
            SG_LOG(SG_GENERAL, SG_DEBUG, "couldn't merge sliver " << i );
            unmerged.push_back( sliver );
        }
    }

    return unmerged;
}