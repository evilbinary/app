// TImageMgt.h

#if !defined(__TIMAGEMGT_H__)
#define __TIMAGEMGT_H__

 
// 定义链表节点的结构体
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

	// 初始化
	BOOL Init( const char* pathname );

	// 获取指定的黑白位图
	unsigned char* GetImage( int nImgID );

	// 删除全部
	BOOL RemoveAll();

private:
	// 添加一个新的黑白图像
	BOOL Add( int id, unsigned char* pcaImage );

	// 打开一个Image文件
	unsigned char* OpenImageFile( const char* filename );
};

#endif // !defined(__TIMAGEMGT_H__)
