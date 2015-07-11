#include "lib/universal_include.h"

#include <math.h>

#include "lib/debug_utils.h"
#include "lib/math/math_utils.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"

#include "network/ClientToServer.h"

#include "world/world.h"
#include "world/whiteboard.h"

//#include <NURBS-20040223/include/nurbs.h>


WhiteBoard::WhiteBoard() :
	m_hasChanged(true),
	m_nextPointId(-1),
	m_eraseSize(ERASE_SIZE)
{
}

WhiteBoard::~WhiteBoard()
{
	int sizePoints = m_points.Size();
	for ( int i = 0; i < sizePoints; i++ )
	{
		delete m_points.GetData( i );
	}
	m_points.Empty();
}

void WhiteBoard::RequestStartLine( float longitude, float latitude )
{
	//AppDebugOut ( "Request start line: %f, %f\n", longitude, latitude );

	if ( m_nextPointId < 0 )
		m_nextPointId = 0;

	g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointStart, m_nextPointId,
												   Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );

	m_nextPointId += 4096;
}

void WhiteBoard::RequestAddLinePoint( float lastLongitude, float lastLatitude, float longitude, float latitude )
{
	//AppDebugOut ( "Request line point: %f, %f\n", longitude, latitude );

	if( m_nextPointId < 0 )
		m_nextPointId = 0;

	// Removed, directly done in the caller (map_renderer)
	//
	//float deltax = longitude - lastLongitude;
	//float deltay = latitude - lastLatitude;
	//float dist = sqrt( deltax * deltax + deltay * deltay );
	//int nbPoint = ( (int) ( dist / LINE_LENGTH_MAX ) ) + 1;
	//for ( int i = 1; i < nbPoint; i++ )
	//{
	//	float newlongitude = lastLongitude + deltax * ( i / (float) nbPoint );
	//	float newlatitude = lastLatitude + deltay * ( i / (float) nbPoint );
	//	g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointMiddle, m_nextPointId, newlongitude, newlatitude );
	//	m_nextPointId += 4096;
	//}
	g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointMiddle, m_nextPointId,
												   Fixed::FromDouble(longitude), Fixed::FromDouble(latitude));
	m_nextPointId += 4096;
}

void WhiteBoard::RequestErase( float longitude, float latitude, float longitude2, float latitude2 )
{
	//AppDebugOut ( "Request erase: %f, %f to %f, %f\n", longitude, latitude, longitude2, latitude2 );

	g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionErase, -1,
												   Fixed::FromDouble(longitude), Fixed::FromDouble(latitude),
												   Fixed::FromDouble(longitude2), Fixed::FromDouble(latitude2));
}

void WhiteBoard::RequestEraseAllLines()
{
	g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionEraseAll );
}

static float Dist2Points2( float x1, float y1, float x2, float y2 )
{
	float dx = x2 - x1;
	float dy = y2 - y1;
	return dx * dx + dy * dy;
}

static void GetMinMax( float l1x1, float l1y1, float l1x2, float l1y2, float eraseSize,
                       float &minx, float &miny, float &maxx, float &maxy )
{
	if ( l1x1 > l1x2 )
	{
		minx = l1x2 - eraseSize;
		maxx = l1x1 + eraseSize;
	}
	else
	{
		minx = l1x1 - eraseSize;
		maxx = l1x2 + eraseSize;
	}
	if ( l1y1 > l1y2 )
	{
		miny = l1y2 - eraseSize;
		maxy = l1y1 + eraseSize;
	}
	else
	{
		miny = l1y1 - eraseSize;
		maxy = l1y2 + eraseSize;
	}
}

//
// 1001 | 1011 | 1010
// -----|------|-----
// 1101 | 1111 | 1110
// -----|------|-----
// 0101 | 0111 | 0110
//
static unsigned char GetPlacement( float minx, float miny, float maxx, float maxy, float l1x1, float l1y1)
{
	unsigned char placement = 0;
	if ( l1x1 >= minx )
	{
		placement |= 2;
	}
	if ( l1x1 <= maxx )
	{
		placement |= 1;
	}
	if ( l1y1 >= miny )
	{
		placement |= 8;
	}
	if ( l1y1 <= maxy )
	{
		placement |= 4;
	}
	return placement;
}

static bool CanIntersect ( unsigned char placement1, unsigned char placement2 )
{
	return ( ( placement1 | placement2 ) == 15 );
}

static void GetEraseBlock ( float lx1, float ly1, float lx2, float ly2, float eraseSize,
                            float &p1x, float &p1y, float &p2x, float &p2y, float &p3x, float &p3y, float &p4x, float &p4y )
{
	float deltax = lx2 - lx1;
	float deltay = ly2 - ly1;

	if ( NearlyEquals( deltax, 0.0f ) && NearlyEquals( deltay, 0.0f ) )
	{
		p1x = lx1 + eraseSize;
		p1y = ly1 + eraseSize;
		p2x = lx1 - eraseSize;
		p2y = ly1 + eraseSize;
		p3x = lx1 - eraseSize;
		p3y = ly1 - eraseSize;
		p4x = lx1 + eraseSize;
		p4y = ly1 - eraseSize;
	}
	else
	{
		float dist = sqrt( deltax * deltax + deltay * deltay );
		float deltaxu = deltax / dist;
		float deltayu = deltay / dist;

		p1x = lx2 + ( deltaxu * eraseSize ) + ( -deltayu * eraseSize );
		p1y = ly2 + ( deltayu * eraseSize ) + ( deltaxu * eraseSize );

		p2x = lx1 + ( -deltaxu * eraseSize ) + ( -deltayu * eraseSize );
		p2y = ly1 + ( -deltayu * eraseSize ) + ( deltaxu * eraseSize );

		p3x = lx1 + ( -deltaxu * eraseSize ) + ( deltayu * eraseSize );
		p3y = ly1 + ( -deltayu * eraseSize ) + ( -deltaxu * eraseSize );

		p4x = lx2 + ( deltaxu * eraseSize ) + ( deltayu * eraseSize );
		p4y = ly2 + ( deltayu * eraseSize ) + ( -deltaxu * eraseSize );
	}
}

static bool Intersect2LineSegments( float l1x1, float l1y1, float l1x2, float l1y2, float l2x1, float l2y1, float l2x2, float l2y2,
                                    float &ix, float &iy, float &r, float &s )
{
	r = ( ( l1y1 - l2y1 ) * ( l2x2 - l2x1 ) - ( l1x1 - l2x1 ) * ( l2y2 - l2y1 ) ) / 
	    ( ( l1x2 - l1x1 ) * ( l2y2 - l2y1 ) - ( l1y2 - l1y1 ) * ( l2x2 - l2x1 ) );

	if ( 0.0f <= r && r <= 1.0f )
	{
		s = ( ( l1y1 - l2y1 ) * ( l1x2 - l1x1 ) - ( l1x1 - l2x1 ) * ( l1y2 - l1y1 ) ) / 
			( ( l1x2 - l1x1 ) * ( l2y2 - l2y1 ) - ( l1y2 - l1y1 ) * ( l2x2 - l2x1 ) );
		
		if ( 0.0f <= s && s <= 1.0f )
		{
			ix = l1x1 + ( l1x2 - l1x1 ) * r;
			iy = l1y1 + ( l1y2 - l1y1 ) * r;
			return true;
		}
	}
	return false;
}

//static float DistanceLinePoint( float p1x, float p1y, float p2x, float p2y, float px, float py )
//{
//	return ( ( p1y - p2y ) * px + ( p2x - p1x ) * py + ( p1x * p2y - p2x * p1y ) ) /
//	       sqrt( Dist2Points2 ( p1x, p1y, p2x, p2y ) );
//}

static float SideLinePoint( float p1x, float p1y, float p2x, float p2y, float px, float py )
{
	return ( p1y - p2y ) * px + ( p2x - p1x ) * py + ( p1x * p2y - p2x * p1y );
}

static bool InsideEraseBlock( float p1x, float p1y, float p2x, float p2y, float p3x, float p3y, float p4x, float p4y,
                              float px, float py )
{
	float d12 = SideLinePoint( p1x, p1y, p2x, p2y, px, py );
	float d23 = SideLinePoint( p2x, p2y, p3x, p3y, px, py );
	float d34 = SideLinePoint( p3x, p3y, p4x, p4y, px, py );
	float d41 = SideLinePoint( p4x, p4y, p1x, p1y, px, py );

	return ( d12 >= 0.0f && d23 >= 0.0f && d34 >= 0.0f && d41 >= 0.0f );
}

static bool IntersectEraseBlock( float lx1, float ly1, float lx2, float ly2,
                                 float p1x, float p1y, float p2x, float p2y, float p3x, float p3y, float p4x, float p4y,
                                 float &i1x, float &i1y, float &i2x, float &i2y )
{

	bool l1Inside = InsideEraseBlock( p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y, lx1, ly1 );
	bool l2Inside = InsideEraseBlock( p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y, lx2, ly2 );

	float ix12 = 0.0f, iy12 = 0.0f, ix23 = 0.0f, iy23 = 0.0f, ix34 = 0.0f, iy34 = 0.0f, ix41 = 0.0f, iy41 = 0.0f;
	float r12 = 0.0f, s12 = 0.0f, r23 = 0.0f, s23 = 0.0f, r34 = 0.0f, s34 = 0.0f, r41 = 0.0f, s41 = 0.0f;
	bool i12 = Intersect2LineSegments( lx1, ly1, lx2, ly2, p1x, p1y, p2x, p2y, ix12, iy12, r12, s12 );
	bool i23 = Intersect2LineSegments( lx1, ly1, lx2, ly2, p2x, p2y, p3x, p3y, ix23, iy23, r23, s23 );
	bool i34 = Intersect2LineSegments( lx1, ly1, lx2, ly2, p3x, p3y, p4x, p4y, ix34, iy34, r34, s34 );
	bool i41 = Intersect2LineSegments( lx1, ly1, lx2, ly2, p4x, p4y, p1x, p1y, ix41, iy41, r41, s41 );

	int nbIntersect = 0;
	if ( i12 ) nbIntersect++;
	if ( i23 ) nbIntersect++;
	if ( i34 ) nbIntersect++;
	if ( i41 ) nbIntersect++;

	if ( l1Inside && l2Inside )
	{
		i1x = lx1;
		i1y = ly1;
		i2x = lx2;
		i2y = ly2;
		return true;
	}
	else if ( l1Inside )
	{
		if ( nbIntersect == 1 )
		{
			i1x = lx1;
			i1y = ly1;
			if( i41 )
			{
				i2x = ix41;
				i2y = iy41;
				return true;
			}
			else if ( i34 )
			{
				i2x = ix34;
				i2y = iy34;
				return true;
			}
			else if ( i23 )
			{
				i2x = ix23;
				i2y = iy23;
				return true;
			}
			else if ( i12 )
			{
				i2x = ix12;
				i2y = iy12;
				return true;
			}
		}
		else if ( nbIntersect > 1 )
		{
			i1x = lx1;
			i1y = ly1;
			if( i41 && ( !i34 || r41 >= r34 ) && ( !i23 || r41 >= r23 ) && ( !i12 || r41 >= r12 ) )
			{
				i2x = ix41;
				i2y = iy41;
				return true;
			}
			else if ( i34 && ( !i23 || r34 >= r23 ) && ( !i12 || r34 >= r12 ) )
			{
				i2x = ix34;
				i2y = iy34;
				return true;
			}
			else if ( i23 && ( !i12 || r23 >= r12 ) )
			{
				i2x = ix23;
				i2y = iy23;
				return true;
			}
			else if ( i12 )
			{
				i2x = ix12;
				i2y = iy12;
				return true;
			}
		}
	}
	else if ( l2Inside )
	{
		if ( nbIntersect == 1 )
		{
			i2x = lx2;
			i2y = ly2;
			if( i41 )
			{
				i1x = ix41;
				i1y = iy41;
				return true;
			}
			else if ( i34 )
			{
				i1x = ix34;
				i1y = iy34;
				return true;
			}
			else if ( i23 )
			{
				i1x = ix23;
				i1y = iy23;
				return true;
			}
			else if ( i12 )
			{
				i1x = ix12;
				i1y = iy12;
				return true;
			}
		}
		else if ( nbIntersect > 1 )
		{
			i2x = lx2;
			i2y = ly2;
			if( i41 && ( !i34 || r41 <= r34 ) && ( !i23 || r41 <= r23 ) && ( !i12 || r41 <= r12 ) )
			{
				i1x = ix41;
				i1y = iy41;
				return true;
			}
			else if ( i34 && ( !i23 || r34 <= r23 ) && ( !i12 || r34 <= r12 ) )
			{
				i1x = ix34;
				i1y = iy34;
				return true;
			}
			else if ( i23 && ( !i12 || r23 <= r12 ) )
			{
				i1x = ix23;
				i1y = iy23;
				return true;
			}
			else if ( i12 )
			{
				i1x = ix12;
				i1y = iy12;
				return true;
			}
		}
	}
	else if( nbIntersect >= 2 )
	{
		if( i41 && ( !i34 || r41 <= r34 ) && ( !i23 || r41 <= r23 ) && ( !i12 || r41 <= r12 ) )
		{
			i1x = ix41;
			i1y = iy41;
		}
		else if ( i34 && ( !i23 || r34 <= r23 ) && ( !i12 || r34 <= r12 ) )
		{
			i1x = ix34;
			i1y = iy34;
		}
		else if ( i23 && ( !i12 || r23 <= r12 ) )
		{
			i1x = ix23;
			i1y = iy23;
		}
		else if ( i12 )
		{
			i1x = ix12;
			i1y = iy12;
		}

		if( i41 && ( !i34 || r41 >= r34 ) && ( !i23 || r41 >= r23 ) && ( !i12 || r41 >= r12 ) )
		{
			i2x = ix41;
			i2y = iy41;
		}
		else if ( i34 && ( !i23 || r34 >= r23 ) && ( !i12 || r34 >= r12 ) )
		{
			i2x = ix34;
			i2y = iy34;
		}
		else if ( i23 && ( !i12 || r23 >= r12 ) )
		{
			i2x = ix23;
			i2y = iy23;
		}
		else if ( i12 )
		{
			i2x = ix12;
			i2y = iy12;
		}

		return true;
	}
	return false;
}

bool WhiteBoard::AddPoint( enum Action action, int pointId, float longitude, float latitude )
{
	WhiteBoardPoint *curPoint = new WhiteBoardPoint( pointId, longitude, latitude, action == ActionPointStart );

	m_hasChanged = true;
	if ( pointId >= m_nextPointId )
	{
		m_nextPointId = pointId + 4096;
	}

	int sizePoints = m_points.Size();
	if ( sizePoints > 0 && m_points.GetData( sizePoints - 1 )->m_id >= pointId )
	{
		for ( int i = 0; i < sizePoints; i++ )
		{
			WhiteBoardPoint *pt = m_points.GetData( i );
			if ( pt->m_id == pointId )
			{
				delete pt;
				m_points.RemoveData( i );
				m_points.PutDataAtIndex( curPoint, i );
				return false;
			}
			else if ( pt->m_id > pointId )
			{
				m_points.PutDataAtIndex( curPoint, i );
				return true;
			}
		}
	}

	m_points.PutData( curPoint );
	return true;
}

bool WhiteBoard::ErasePoint( int pointId )
{
	int sizePoints = m_points.Size();
	for ( int i = 0; i < sizePoints; i++ )
	{
		WhiteBoardPoint *pt = m_points.GetData( i );
		if ( pt->m_id == pointId )
		{
			delete pt;
			m_points.RemoveData( i );
			m_hasChanged = true;
			return true;
		}
	}
	return false;
}

void WhiteBoard::EraseBlock( float longitude1, float latitude1, float longitude2, float latitude2 )
{
	if ( m_points.Size() > 1 )
	{
		float p1x = 0.0f, p1y = 0.0f, p2x = 0.0f, p2y = 0.0f, p3x = 0.0f, p3y = 0.0f, p4x = 0.0f, p4y = 0.0f;
		GetEraseBlock ( longitude1, latitude1, longitude2, latitude2, m_eraseSize,
		                p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y );

		float minx = 0.0f, miny = 0.0f, maxx = 0.0f, maxy = 0.0f;
		GetMinMax ( longitude1, latitude1, longitude2, latitude2, m_eraseSize, minx, miny, maxx, maxy );

		WhiteBoardPoint *lastPoint = m_points.GetData( 0 );
		unsigned char lastPlacement = GetPlacement( minx, miny, maxx, maxy, lastPoint->m_longitude, lastPoint->m_latitude );
		bool lastDeleted = false;
		bool deleteNext = false;
		for ( int i = 1; i < m_points.Size(); i++ )
		{
			WhiteBoardPoint *pt = m_points.GetData( i );
			unsigned char placement = GetPlacement( minx, miny, maxx, maxy, pt->m_longitude, pt->m_latitude );

			float i1x = 0.0f, i1y = 0.0f, i2x = 0.0f, i2y = 0.0f;
			if ( !pt->m_startPoint && CanIntersect ( lastPlacement, placement ) && 
			     IntersectEraseBlock ( lastPoint->m_longitude, lastPoint->m_latitude, pt->m_longitude, pt->m_latitude, 
			                           p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y, 
			                           i1x, i1y, i2x, i2y ) )
			{
				int nextId = ( pt->m_id + lastPoint->m_id ) / 2;
				if ( !deleteNext && ( i1x != lastPoint->m_longitude || i1y != lastPoint->m_latitude ) )
				{
					if ( lastDeleted || ( nextId == lastPoint->m_id && lastPoint->m_startPoint ) )
					{
						//g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointStart, nextId, i1x, i1y );
						if ( AddPoint( ActionPointStart, nextId, i1x, i1y ) )
						{
							i++;
						}
					}
					else
					{
						//g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointMiddle, nextId, i1x, i1y );
						if ( AddPoint( ActionPointMiddle, nextId, i1x, i1y ) )
						{
							i++;
						}
					}
					lastDeleted = false;
					nextId = ( pt->m_id + nextId ) / 2;
				}
				else
				{
					//g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointErase, lastPoint->m_id );
					if ( ErasePoint( lastPoint->m_id ) )
					{
						i--;
					}
					lastDeleted = true;
					deleteNext = false;
				}

				if ( i2x != pt->m_longitude || i2y != pt->m_latitude )
				{
					//g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointStart, nextId, i2x, i2y );
					if ( AddPoint( ActionPointStart, nextId, i2x, i2y ) )
					{
						i++;
					}
					lastDeleted = false;
				}
				else
				{
					deleteNext = true;
				}
			}
			else if ( deleteNext || ( lastDeleted && !lastPoint->m_startPoint ) )
			{
				//g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointStart, lastPoint->m_id, lastPoint->m_longitude, lastPoint->m_latitude );
				if ( AddPoint( ActionPointStart, lastPoint->m_id, lastPoint->m_longitude, lastPoint->m_latitude ) )
				{
					i++;
				}
				lastDeleted = false;
				deleteNext = false;
			}

			lastPoint = pt;
			lastPlacement = placement;
		}
		if ( deleteNext || lastDeleted )
		{
			//g_app->GetClientToServer()->RequestWhiteBoard( g_app->GetWorld()->m_myTeamId, ActionPointErase, lastPoint->m_id );
			ErasePoint( lastPoint->m_id );
		}
	}
}

void WhiteBoard::ReceivedAction( enum Action action, int pointId, float longitude, float latitude, float longitude2, float latitude2 )
{
	if ( action == ActionPointStart || action == ActionPointMiddle )
	{
		AddPoint( action, pointId, longitude, latitude );
	}
	else if ( action == ActionErase )
	{
		EraseBlock( longitude, latitude, longitude2, latitude2 );
	}
	else if ( action == ActionEraseAll )
	{
		int sizePoints = m_points.Size();
		if ( sizePoints > 0 )
		{
			for ( int i = 0; i < sizePoints; i++ )
			{
				delete m_points.GetData( i );
			}
			m_points.Empty();
			m_hasChanged = true;
		}
	}
	else
	{
		AppDebugOut ( "Invalid action %d", action );
	}
}

const LList<WhiteBoardPoint *> *WhiteBoard::GetListPoints() const
{
	return &m_points;
}

bool WhiteBoard::HasChanges() const
{
	return m_hasChanged;
}

void WhiteBoard::ClearChanges()
{
	m_hasChanged = false;
}


WhiteBoardPoint::WhiteBoardPoint( int id, float longitude, float latitude, bool startPoint ) :
	m_id(id),
	m_longitude(longitude),
	m_latitude(latitude),
	m_startPoint(startPoint)
{
}
