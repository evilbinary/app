// TImageMgt.h

#if !defined(__TIMAGEMGT_H__)
#define __TIMAGEMGT_H__

 
// ��������ڵ�Ľṹ��
typedef struct _QIMAGE
{
	int              nID;
	unsigned char*   pImg;
	struct _QIMAGE*  next;
}QIMAGE;

class CTImgMgt
{
private:
	int m_nCount;
	struct _QIMAGE* m_pHead;

public:
	CTImgMgt();
	virtual ~CTImgMgt();

	// ��ʼ��
	BOOL Init( const char* pathname );

	// ��ȡָ���ĺڰ�λͼ
	unsigned char* GetImage( int nImgID );

	// ɾ��ȫ��
	BOOL RemoveAll();

private:
	// ���һ���µĺڰ�ͼ��
	BOOL Add( int id, unsigned char* pcaImage );

	// ��һ��Image�ļ�
	unsigned char* OpenImageFile( const char* filename );
};

#endif // !defined(__TIMAGEMGT_H__)
