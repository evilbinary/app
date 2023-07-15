// TKeyMapMgt.h

#if !defined(__TKEYMAPMGT_H__)
#define __TKEYMAPMGT_H__

#if !defined(BOOL)
#define BOOL int
#endif // !defined(BOOL)

typedef struct _KEYMAP
{
	char psKeyName[32];
	unsigned int nKeyValue;
}KEYMAP;

#define KEY_MAX 64

class CTKeyMapMgt
{
private:
	int m_nCount;
	struct _KEYMAP m_pstKeyMap[KEY_MAX];

public:
	CTKeyMapMgt();
	virtual ~CTKeyMapMgt();

public:
	BOOL Load( char* psFileName );
	int  Find( char* psKeyName  );
	int  GetCount();
	BOOL GetName( int nIndex, char* psKeyName );
	int  GetValue( int nIndex );
};

#endif // !defined(__TKEYMAPMGT_H__)
