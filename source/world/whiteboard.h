
#ifndef _included_whiteboard_h
#define _included_whiteboard_h

#include "lib/tosser/llist.h"


class WhiteBoardPoint
{
public:
	int m_id;
	float m_longitude;
	float m_latitude;
	bool m_startPoint;

	WhiteBoardPoint( int id, float longitude, float latitude, bool startPoint );
};


class WhiteBoard
{
public:
	enum Action
	{
		ActionPointStart = 0,
		ActionPointMiddle = 1,
		ActionErase = 2,
		ActionEraseAll = 3
		// begin line / end line / erase / erase all 
	};

	static const int ERASE_SIZE = 3;
	static const int LINE_LENGTH_MAX = 45;

protected:
	LList<WhiteBoardPoint *> m_points;
	int m_nextPointId;
	float m_eraseSize;
	bool m_hasChanged;

protected:
	bool AddPoint( enum Action action, int pointId, float longitude, float latitude );
	bool ErasePoint( int pointId );
	void EraseBlock( float longitude1, float latitude1, float longitude2, float latitude2 );

public:
	WhiteBoard();
	~WhiteBoard();

	void RequestStartLine( float longitude, float latitude );
	void RequestAddLinePoint( float lastLongitude, float lastLatitude, float longitude, float latitude );
	void RequestErase( float longitude, float latitude, float longitude2, float latitude2 );
	void RequestEraseAllLines();
	
	void ReceivedAction( enum Action action, int pointId, float longitude, float latitude, float longitude2, float latitude2 );

	const LList<WhiteBoardPoint *> *GetListPoints() const;

	bool HasChanges() const;
	void ClearChanges();
};

#endif
