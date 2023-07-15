// TDialog.cpp: implementation of the CTDialog class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTDialog::CTDialog ()
{
	m_iDoModalReturn = 0;
	m_pAccell = new CTAccell ();
}

CTDialog::~CTDialog ()
{
	if (m_pAccell != NULL)
		delete m_pAccell;
}

// �麯�������ƶԻ���
void CTDialog::Paint (CLCD* pLCD)
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	// �Ƚ�����������
	// ���ݴ��ڲ�ͬ��״̬����
	// ������ڽ���Ĵ��ڲ��ǶԻ�������Ʒ��׵���ͷ�������ƶԻ�����Ӱ
	// ���������ͨ����ͷ������ͷ�������һ����
	if (m_pActive != NULL)
	{
		if (m_pActive->m_wWndType != TWND_TYPE_DIALOG)
		{
			// ���ƽ������ͷ
			DrawDialog (pLCD, 1);
		}
		else
		{
			// ������ͨ����ͷ
			DrawDialog (pLCD, 0);
		}
	}
	else
	{
		// ���ƽ������ͷ
		DrawDialog (pLCD, 1);
	}

	// ������Ĭ�ϻ���
	CTWindow::Paint (pLCD);
}

// �麯������Ϣ����
// ��Ϣ�������ˣ�����1��δ��������0
int CTDialog::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	// �Ƚ���Ĭ�ϴ���
	int iReturn = CTWindow::Proc (pWnd, iMsg, wParam, lParam);

	// ������ؼ�û�д�������������д���
	// �Ƿ�رնԻ�������л�����ؼ��İ�����Ϣ
	if (iReturn != 1)
	{
		if (iMsg == TMSG_KEYDOWN)
		{
			if (wParam == TVK_ESCAPE)
			{
				// �رմ���
				// ���Լ�����TMSG_CLOSE��Ϣ
				_TMSG msg;
				msg.pWnd    = this;
				msg.message = TMSG_CLOSE;
				msg.wParam  = m_ID;
				msg.lParam  = 0;
				m_pApp->PostMessage (&msg);
				iReturn = 1;
			}

			// ���洦����ϵ�������л��İ���
			if (   (wParam == TVK_TAB)      ||
			       (wParam == TVK_UP)       ||
			       (wParam == TVK_DOWN)     ||
			       (wParam == TVK_LEFT)     ||
			       (wParam == TVK_RIGHT))
			{
				if( OnChangeFocus() )
				{
					int id = -1;
					int iTabOrder = -1;
					if (m_pActive != NULL)
					{
						iTabOrder = m_pActive->m_iTabOrder;
						id = m_pActive->m_ID;
					}

					// �����������Ҽ�
					if( (wParam == TVK_LEFT) || (wParam == TVK_RIGHT) ||
						(wParam == TVK_UP  ) || (wParam == TVK_DOWN ) )
					{
						CTWindow* pWnd = NULL;

						if( m_pActive != NULL )
						{
							switch( wParam )
							{
							case TVK_LEFT:
								pWnd = FindWindowLeft( m_pActive );
								break;
							case TVK_RIGHT:
								pWnd = FindWindowRight( m_pActive );
								break;
							case TVK_UP:
								pWnd = FindWindowUp( m_pActive );
								break;
							case TVK_DOWN:
								pWnd = FindWindowDown( m_pActive );
								break;
							}
						}

						if( (pWnd != NULL) && (pWnd != m_pActive) )
						{
							if( pWnd->IsWindowEnabled() )
							{
								id = pWnd->m_ID;
								iReturn = 1;
							}
						}
					}

					if( iReturn == 0 )
					{
						// ���������л���
						if( wParam == TVK_TAB || wParam == TVK_RIGHT || wParam == TVK_DOWN )
						{
							// ѭ���л�
							iTabOrder += 1;
							CTWindow* pWnd = FindChildByTab (iTabOrder);
							// ���û���ҵ�Ӧ��������Ϊ����Ŀؼ����򽫵�һ������Ϊ����
							if (pWnd != NULL)
								id = pWnd->m_ID;
							else if( m_pChildren != NULL )
								id = m_pChildren->m_ID;

							iReturn = 1;
						}
					}

					if( iReturn == 0 )
					{
						if(wParam == TVK_LEFT || wParam == TVK_UP)
						{
				  			for( int i=0; i<m_iChildCount; i++ )
				    		{
				      			if( iTabOrder == 0 )
								iTabOrder = m_iChildCount;
					  
								iTabOrder --;

								CTWindow* pWnd = FindChildByTab( iTabOrder );

								if( pWnd != NULL )
								{
					  				if (((pWnd->m_wStatus & TWND_STATUS_INVALID) == 0) &&
										((pWnd->m_wStatus & TWND_STATUS_INVISIBLE) == 0))
									{
										id = pWnd->m_ID;
										iReturn = 1;
										break;
									}
								}
							}
						}
					}

					// �л�����Ĺ�������Ϣ��������ȥ����
					_TMSG msg;	
					msg.pWnd    = this;
					msg.message = TMSG_SETCHILDACTIVE;
					msg.wParam  = id;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
				}
				iReturn = 1;
			}
		}
	}

#if defined(CHINESE_SUPPORT)
	// ������ݼ��б�(�����뷨��ʱ��Ҫ���ο�ݼ�)
	if ((iReturn != 1) && (!m_pApp->IsIMEOpen ()))
	{
#endif // defined(CHINESE_SUPPORT)

#if !defined(CHINESE_SUPPORT)
       // ������ݼ��б�
       if (iReturn != 1)
	{
#endif //!defined(CHINESE_SUPPORT)
		if (iMsg == TMSG_KEYDOWN)
		{
			int id = m_pAccell->Find (wParam);
			if (id != 0)
			{
				// ���ID��ĳ����ť��ID��ͬ���򷢳�TMSG_PUSHDOWN��Ϣʹ��ť����
				// ���ȷ�����Ϣʹ�ð�ť��Ϊ����
				CTWindow* pWnd = FindChildByID (id);
				if (pWnd != NULL)
				{
					_TMSG msg;
				//  �����ݼ���Ӧʱ��Ҫ�л����㣬��⿪�������
				/*
					msg.pWnd	= this;
					msg.message = TMSG_SETCHILDACTIVE;
					msg.wParam	= id;
					msg.lParam	= 0;
					m_pApp->PostMessage (&msg);	// ���ý������Ϣ
				*/
					msg.pWnd	= pWnd;
					msg.message = TMSG_PUSHDOWN;
					msg.wParam	= 0;
					msg.lParam	= 0;
					m_pApp->PostMessage (&msg);	// ��ť����֪ͨ�䰴�µ���Ϣ 
				}
				else
				{
					_TMSG msg;
					msg.pWnd	= this;
					msg.message = TMSG_NOTIFY_PARENT;
					msg.wParam	= id;
					msg.lParam	= 0;
					m_pApp->PostMessage (&msg);	// ���Լ����Ϳ�ݼ���Ϣ 
				}
			}
		}

	}
	iReturn = 1;
	return iReturn;
}

#if defined(MOUSE_SUPPORT)
int CTDialog::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	if (iReturn != 1)
	{
		if (iMsg == TMSG_LBUTTONDOWN)
		{
			if (PtInWindow (wParam, lParam) )
			{
				// �����رհ�ť
				// �ɼ�������ͷ����ʾ�رհ�ť���Ŵ����رհ�ť
				if (((m_wStatus & TWND_STATUS_INVISIBLE)   == 0) &&
					((m_wStyle  & TWND_STYLE_NO_TITLE)     == 0) &&
					((m_wStyle  & TWND_STYLE_CLOSE_BUTTON) >  0))
				{
					int x = wParam;
					int y = lParam;
					int L = m_x + m_w - 15;
					int T = m_y;
					int R = m_x + m_w;
					int B = m_y + 15;
					if ((x >= L) && (y >= T) && (x <= R) && (y <= B))
					{
						// �رմ���
						// ���Լ�����TMSG_CLOSE��Ϣ
						_TMSG msg;
						msg.pWnd    = this;
						msg.message = TMSG_CLOSE;
						msg.wParam  = m_ID;
						msg.lParam  = 0;
						m_pApp->PostMessage (&msg);
					}
				}
				iReturn = 1;
			}
		}
	}

	iReturn = 1;
	return iReturn;
}
#endif // defined(MOUSE_SUPPORT)

// ����ģʽ״̬
int CTDialog::DoModal()
{
	if( m_pApp == NULL )
		return -1;

	int iMsgInfo = 1;
	_TMSG msg;

	while( iMsgInfo != 0 )
	{
		int iMsgInfo = m_pApp->m_pMsgQ->GetMessage( &msg );

		if ( iMsgInfo == 1 )
		{
			// ����Delete�����ڵ���Ϣ��ִ�к��˳�
			if( (msg.pWnd    == m_pParent)        &&
				(msg.message == TMSG_DELETECHILD) &&
				(msg.wParam  == m_ID) )
			{
				int iRet = m_iDoModalReturn;
				m_pApp->DespatchMessage( &msg );
				return iRet;
			}
			else
				m_pApp->DespatchMessage( &msg );
		}
		else if( iMsgInfo == -1 )
			m_pApp->Idle();


// Windows����Ҫά��Win32������Ϣ�õĹ���
#if defined(RUN_ENVIRONMENT_WIN32)
		MSG win_msg;
		if( !GetMessage(&win_msg, NULL, 0, 0) )
		{
			// ��Win32��Ϣ�����WM_QUIT��Ϣ���˳�
			::PostQuitMessage( WM_QUIT );
			// ��TGUIϵͳ����Ϣ�������һ��QUIT��Ϣ
			msg.message = TMSG_QUIT;
			m_pApp->PostMessage( &msg );
			return 0;
		}
		TranslateMessage(&win_msg);
		// ��ϵͳ��Ϣת�͸�TGUI����Ϣ����
		m_pApp->InsertMessageToTGUI(&win_msg);
		// ������Ϣ�Ĵ���
		DispatchMessage(&win_msg);
		::Sleep(10);
#endif // defined(RUN_ENVIRONMENT_WIN32)

	}

	// ����Ϣ�������һ��QUIT��Ϣ
	msg.message = TMSG_QUIT;
	m_pApp->PostMessage( &msg );

	return 0;
}

// �ӶԻ�����Դ�ļ������Ի���
BOOL CTDialog::CreateFromFile(CTWindow* pParent, const char* filename)
{
	CTDlgTemplet DialogTemplet;

	// �ӶԻ�����Դ�ļ���ʼ���Ի���ģ��
	struct _DLGTEMPLET* pT = DialogTemplet.OpenDlgTemplet( filename );
	if( pT == NULL )
		return FALSE;

	// ��ģ�崴���Ի���
	if( !CreateFromTemplet(pParent, pT) )
	{
		DialogTemplet.DeleteDlgTemplet();
		return FALSE;
	}

	DialogTemplet.DeleteDlgTemplet();
	return TRUE;
}

// �ӶԻ���ģ��ṹ�崴���Ի���
BOOL CTDialog::CreateFromTemplet( CTWindow* pParent, _DLGTEMPLET* pT )
{
	// ���û����Create�����򸸴���ע�᱾�Ի���
	CTWindow::Create( pParent,
					TWND_TYPE_DIALOG,
					pT->wStyle,
					TWND_STATUS_FOCUSED,
					pT->x, pT->y, pT->w, pT->h, pT->id);

	// ����ģ���޸ĶԻ���ĸ�����Ա����
	SetText (pT->caption, 255);

	// ��ʼ����ݼ��б�
	m_pAccell->Create (pT->pAccellTable, m_pApp->m_pKeyMap);

	// ����ģ�崴�����ֿؼ�
	CTRLDATA* pNext = pT->controls;
	int i;
	for (i=0; i<pT->controlnr; i++)
	{
		if (pNext == NULL)
			return FALSE;

		// �ؼ�����������ڶԻ������ϵ��λ�ã�Ӧ��ת���ɾ�������
		int ix = pT->x + pNext->x;
		int iy = pT->y + pNext->y;

		if ((pT->wStyle & TWND_STYLE_NO_TITLE) == 0)
			iy += 15;			// ������ͷ�ĸ߶�

		int iw = pNext->w;
		int ih = pNext->h;
		int id = pNext->id;

		switch( pNext->wType )
		{
		case TWND_TYPE_STATIC:
			{
			  //DebugPrintf( "Debug: Here Create a Static %s \n",pNext->caption );
				CTStatic* pCtrl = new CTStatic();
				pCtrl->Create( this,
							TWND_TYPE_STATIC,
							pNext->wStyle,
							TWND_STATUS_INVALID,
							ix, iy, iw, ih, id );
				pCtrl->SetText (pNext->caption, 255);
				if( pNext->iAddData > 0 )
					pCtrl->LoadImage( m_pApp->GetImage(pNext->iAddData) );
			}
			break;
		case TWND_TYPE_BUTTON:
			{
				CTButton* pCtrl = new CTButton ();
				pCtrl->Create( this,
							TWND_TYPE_BUTTON,
							pNext->wStyle,
							TWND_STATUS_NORMAL,
							ix, iy, iw, ih, id );
				pCtrl->SetText (pNext->caption, 255);
				if( pNext->iAddData != 0 )
					pCtrl->LoadImage( m_pApp->GetImage(pNext->iAddData) );
			}
			break;
		case TWND_TYPE_EDIT:
			{
				CTEdit* pCtrl = new CTEdit ();
				pCtrl->Create( this,
							TWND_TYPE_EDIT,
							pNext->wStyle,
							TWND_STATUS_NORMAL,
							ix, iy, iw, ih, id );
				pCtrl->SetText (pNext->caption, 255);
				pCtrl->LimitText (pNext->iAddData);
			}
			break;
		case TWND_TYPE_LIST:
			{
				CTList* pCtrl = new CTList ();
				pCtrl->Create( this,
							TWND_TYPE_LIST,
							pNext->wStyle,
							TWND_STATUS_NORMAL,
							ix, iy, iw, ih, id );
				pCtrl->SetLinage( pNext->iAddData );
			}
			break;
		case TWND_TYPE_COMBO:
			{
				CTCombo* pCtrl = new CTCombo ();
				pCtrl->Create( this,
							TWND_TYPE_COMBO,
							pNext->wStyle,
							TWND_STATUS_NORMAL,
							ix, iy, iw, ih, id );
				pCtrl->SetText( pNext->caption, 255 );
				pCtrl->SetDroppedLinage( pNext->iAddData );
			}
			break;
		case TWND_TYPE_PROGRESS:
			{
				CTProgress* pCtrl = new CTProgress ();
				pCtrl->Create( this,
							TWND_TYPE_PROGRESS,
							pNext->wStyle,
							TWND_STATUS_NORMAL,
							ix, iy, iw, ih, id );
			}
			break;
		case TWND_TYPE_CHECK_BOX:
			{
				CTCheckBox* pCtrl = new CTCheckBox();
				pCtrl->Create( this,
							TWND_TYPE_CHECK_BOX,
							pNext->wStyle,
							TWND_STATUS_NORMAL,
							ix, iy, iw, ih, id );
				pCtrl->SetText( pNext->caption, 255 );
				pCtrl->SetCheck( pNext->iAddData );
			}
			break;
		default:
			return false;
		}
		pNext = pNext->next;
	}

	// ����Ĭ�ϰ�ť
	m_pDefaultButton = FindDefaultButton();

	// ����һ���ؼ�����Ϊ����
	SetFocus( m_pChildren );
	if( m_pActive != NULL )
	{
		_TMSG msg;
		msg.pWnd = m_pActive;
		msg.message = TMSG_SETFOCUS;
		msg.wParam = m_pActive->m_ID;
		msg.lParam = 0;
		m_pApp->SendMessage( &msg );
	}

	return TRUE;
}

// ����ID�ţ�ʹ����Ӧ��ģ���ļ������Ի���
BOOL CTDialog::CreateFromID(CTWindow* pWnd, int iID)
{
	char cFileName[256];
	memset( cFileName, 0x0, 256 );
	sprintf( cFileName, "dtm/%d.dtm", iID );
	return( CreateFromFile( pWnd, cFileName ) );
}

// ���ƶԻ���
// 0,�ǽ��㴰�ڣ�1,������ʾ(���㴰��)
void CTDialog::DrawDialog (CLCD* pLCD, int iMode)
{
	// ������ɼ�����ʲôҲ������
	if ((m_wStatus & TWND_STATUS_INVISIBLE) != 0)
		return;

	int iTitleLength = GetDisplayLimit( m_cCaption, m_w-4 );

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// ����Բ�Ƿ���ҳߴ��㹻��ĶԻ���ſ���ʹ��Բ�Ƿ��
	if (((m_wStyle & TWND_STYLE_ROUND_EDGE) == 0) && (m_w > 12) && (m_h > 12))
	{
		// ������ͨ��ʽ�ĶԻ���
		// �ڴ��ڷ�Χ����һ���߿�
		pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
		pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
		pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
		pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);

		if (iMode == 1)
		{
			if ((m_wStyle & TWND_STYLE_NO_TITLE) == 0)
			{
				// ���Ʒ�����ͷ
				pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+1, m_w-2, 15, 1);
				pLCD->LCDTextOut (fb,w,h,m_x+2,m_y+2,1,(unsigned char*)m_cCaption,iTitleLength);
				// ���ƹرհ�ť
				if( (m_wStyle & TWND_STYLE_CLOSE_BUTTON) > 0 )
					pLCD->LCDDrawImage (fb,w,h, m_x+m_w-15,m_y,15,15,LCD_MODE_INVERSE,pBitmap_Close_Button1,15,15,0,0);
			}

			// ���ָ��������Ч��������ƶԻ������Ӱ
			if ((m_wStyle & TWND_STYLE_SOLID) > 0)
			{
				pLCD->LCDHLine (fb, w, h, m_x+1, m_y+m_h, m_w, 1);
				pLCD->LCDVLine (fb, w, h, m_x+m_w, m_y+1, m_h, 1);
			}
		}
		else
		{
			if ((m_wStyle & TWND_STYLE_NO_TITLE) == 0)
			{
				// ����������ͷ
				pLCD->LCDTextOut (fb,w,h,m_x+2,m_y+2,0,(unsigned char*)m_cCaption,iTitleLength);
				pLCD->LCDHLine (fb, w, h, m_x+1, m_y+15, m_w-2, 1);
				// ���ƹرհ�ť
				if( (m_wStyle & TWND_STYLE_CLOSE_BUTTON) > 0 )
					pLCD->LCDDrawImage (fb,w,h, m_x+m_w-15,m_y,15,15,LCD_MODE_NORMAL,pBitmap_Close_Button1,15,15,0,0);
			}
		}
	}
	else
	{
		// ����Բ�ǵĶԻ���
		// ����һ���߿�
		pLCD->LCDFillRect (fb, w, h, m_x, m_y+6, m_w, m_h-12, 0);
		pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, 5, 0);
		pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+m_h-6, m_w-12, 5, 0);
		pLCD->LCDHLine (fb, w, h, m_x+6, m_y, m_w-12, 1);
		pLCD->LCDHLine (fb, w, h, m_x+6, m_y+m_h-1, m_w-12, 1);
		pLCD->LCDVLine (fb, w, h, m_x, m_y+6, m_h-12, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y+6, m_h-12, 1);

		iTitleLength -= 2;

		if (iMode == 1)
		{
			// ���ϽǺ����Ͻ�������ͼ���
			if ((m_wStyle & TWND_STYLE_NO_TITLE) == 0)
			{
				if ((m_wStyle & TWND_STYLE_SOLID) > 0)
				{
					pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Default, 13, 13, 0, 0);
					pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Default, 13, 13, 6, 0);
				}
				else
				{
					pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal, 13, 13, 0, 0);
					pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal, 13, 13, 6, 0);
				}
				// ���ƹرհ�ť
				if( (m_wStyle & TWND_STYLE_CLOSE_BUTTON) > 0 )
					pLCD->LCDDrawImage (fb,w,h, m_x+m_w-15,m_y,15,15,LCD_MODE_NORMAL,pBitmap_Close_Button2,15,15,0,0);
			}
			else
			{
				if ((m_wStyle & TWND_STYLE_SOLID) > 0)
				{
					pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Default, 13, 13, 0, 0);
					pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Default, 13, 13, 6, 0);
				}
				else
				{
					pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 0, 0);
					pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 6, 0);
				}
			}
			// ���ָ��������Ч������ѡ�ô�����Ӱ�Ľ�ͼ�������ƶԻ������Ӱ
			if ((m_wStyle & TWND_STYLE_SOLID) > 0)
			{
				pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Default, 13, 13, 0, 0);
				pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Default, 13, 13, 6, 0);
				pLCD->LCDDrawImage (fb, w, h, m_x, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL , pBitmap_Button_Default, 13, 13, 0, 6);
				pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL , pBitmap_Button_Default, 13, 13, 6, 6);
				pLCD->LCDHLine (fb, w, h, m_x+6, m_y+m_h, m_w-12, 1);
				pLCD->LCDVLine (fb, w, h, m_x+m_w, m_y+6, m_h-12, 1);
			}
			else
			{
				pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal, 13, 13, 0, 0);
				pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal, 13, 13, 6, 0);
				pLCD->LCDDrawImage (fb, w, h, m_x, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 0, 6);
				pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 6, 6);
			}

			if ((m_wStyle & TWND_STYLE_NO_TITLE) == 0)
			{
				// ���Ʒ�����ͷ
				pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, 15, 1);
				pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+6, m_w-2, 10, 1);
				pLCD->LCDTextOut (fb,w,h,m_x+7,m_y+2,1,(unsigned char*)m_cCaption,iTitleLength);
				// ���ƹرհ�ť
				if( (m_wStyle & TWND_STYLE_CLOSE_BUTTON) > 0 )
					pLCD->LCDDrawImage (fb,w,h, m_x+m_w-15,m_y,15,15,LCD_MODE_INVERSE,pBitmap_Close_Button2,15,15,0,0);
			}
		}
		else
		{
			// ����ģʽ��Բ�ǣ��Ȼ��Ʊ���
			pLCD->LCDDrawImage (fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 0, 0);
			pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 6, 0);
			pLCD->LCDDrawImage (fb, w, h, m_x, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 0, 6);
			pLCD->LCDDrawImage (fb, w, h, m_x+m_w-6, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL, pBitmap_Button_Normal, 13, 13, 6, 6);

			if ((m_wStyle & TWND_STYLE_NO_TITLE) == 0)
			{
				// ����������ͷ
				pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, 15, 0);
				pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+6, m_w-2, 10, 0);
				pLCD->LCDTextOut (fb,w,h,m_x+7,m_y+2,0,(unsigned char*)m_cCaption,iTitleLength);
				pLCD->LCDHLine (fb, w, h, m_x+1, m_y+15, m_w-2, 1);
				// ���ƹرհ�ť
				if( (m_wStyle & TWND_STYLE_CLOSE_BUTTON) > 0 )
					pLCD->LCDDrawImage (fb,w,h, m_x+m_w-15,m_y,15,15,LCD_MODE_NORMAL,pBitmap_Close_Button2,15,15,0,0);
			}
		}
	}
}

// ���������л�
BOOL CTDialog::OnChangeFocus()
{
  return TRUE;
}

// �رնԻ���
void CTDialog::CloseDialog()
{
	_TMSG msg;
	msg.pWnd = this;
	msg.message = TMSG_CLOSE;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->SendMessage( &msg );
}
/* END */
