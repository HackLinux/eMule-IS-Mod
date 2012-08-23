//this file is part of eMule IS Mod
//Copyright (C)2012 IS Mod Team
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once

#include "Preferences.h"

class CServerSocket;

//������������ �������� ������ �����������
enum eAuthStatus {
	AUTH_NONE = 0,
	AUTH_SUCCESS,
	AUTH_STARTED,
	AUTH_REQUEST,
	AUTH_WAITING,
	AUTH_FAILED
};

//������������ �������� ���� ��������� �����������
enum eAuthMessageType{
	LOG_MESSAGE = 0,
	INFO_MESSAGE_NOMODAL,
	INFO_MESSAGE_MODAL,
	ERR_MESSAGE,
	WARN_PROTOCOL,
	ERR_PROTOCOL
};

// ������������ �������� ��������� ���������� �����������
enum eAuthResult{
	AUTH_OK = 0,
	AUTH_ERROR,
	AUTH_WAIT
};

// ����� ���������� �������� ����������� � �������
class AuthProcess {
public:
	AuthProcess();
	virtual ~AuthProcess();
	
	static AuthProcess* StartAuth(CServerSocket* socket, LPCTSTR szServer);
	void StopAuth();
	void ProcessAuthResult();

	// ������� ����������� ������
	static CString EncryptPasswd(const CString &passwd, const CString &login);
	static CString DecryptPasswd(const CString &encpasswd, const CString &login);

private:
	CSafeThread*	m_AuthThread; // ����� ������ �����������
/**************************************************************/
	eAuthResult			m_AuthResult;	//������ ������
	eAuthMessageType	m_AuthMsgType;	//��� ���������
	uint32_t				m_uTimeout;		//�������
	CString				m_strMessage;	//���������
	LPCTSTR				m_szServer;
	CServerSocket*      m_pParentSocket;       

	bool ParseMessage(LPCSTR szMessage);
	void SetResponse(LPCTSTR szMessage, eAuthResult res, eAuthMessageType type);
	void ShowResponse();
/**************************************************************/
	void RequestAuthorization();
	static UINT AFX_CDECL AuthThreadWrapper(LPVOID lpParam);
	CString createQuery();
};