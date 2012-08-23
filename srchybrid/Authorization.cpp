//this file is part of eMule IS Mod
//Copyright (C)2012 eMule IS Mod Team
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

#include "stdafx.h"
#include <sstream>

#include <tinyxml/tinyxml.h>
#pragma warning(push)
#pragma warning(disable:4244 4100 4189)
#include <cryptopp/blowfish.h>
#include <cryptopp/base64.h>
#include <cryptopp/files.h>
#include <cryptopp/modes.h>
#include <cryptopp/gzip.h>
#include <cryptopp/md5.h>
#pragma warning(pop)
#include <emhelper/emhelper.h>

#include "Authorization.h"
#include "AuthDlg.h"
#include "EmuleDlg.h"
#include "menucmds.h"
#include "Log.h"
#include "Preferences.h"
#include "Base64Coding.h"
#include "StringConversion.h"
#include "emule.h"
#include "version.h"
#include "ServerSocket.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//������� �������������� ����� � ������ ������ �������� ������ �� ������� ��������� :-)
//�� ������ �������� �� �����, ��� ����� ���� ����
void convert_number(int number, byte *array, int len_array){
	const int SHIFT = 5;
	union {
		unsigned int i; 
		unsigned char c[sizeof(int)];
	} diget;

	int tmpNumber = number;
	int countArray = 0;

	//��������� ������ ������� �� ����� ������� sizeof(int)
	for(UINT i=0; i < len_array/sizeof(int); i++){
		//�� ���� ��� �� ���� ������� �������, ���������� �� ����
		diget.i = tmpNumber^((tmpNumber<<SHIFT)+i)&((tmpNumber<<(SHIFT*2))+i)|((tmpNumber<<(SHIFT*3))+i);
		tmpNumber = diget.i;
		for(int j=0; j<sizeof(int); j++){
			array[countArray++] = diget.c[j];
		}
	}
	
	//��������� ������� �������
	diget.i = tmpNumber^(tmpNumber<<SHIFT)^(tmpNumber<<SHIFT*2);
	for(UINT i=0; i<len_array%sizeof(int); i++){
		array[countArray++] = diget.c[i%sizeof(int)];
	}
}

//������� ������������ �����
void CreateKey(byte *key, int len_key){
	memset( key, 0x00, len_key );

	//������ �� ������ ��������� ����� ����
	wchar_t NameBuffer[MAX_PATH];
	wchar_t SysNameBuffer[MAX_PATH];
	DWORD VSNumber;
	DWORD MCLength;
	DWORD FileSF;

	//�������� �������� ����
	if (!GetVolumeInformation(L"C:\\",NameBuffer, sizeof(NameBuffer),
			&VSNumber,&MCLength,&FileSF,SysNameBuffer,sizeof(SysNameBuffer))){
		VSNumber = 1024; // ���� ��� �� ������� ������ �������� ���� ����, ������ ������� �������������� �����
	}

	//�� ������ ���������� ������ �������� ������������������ 
	convert_number(VSNumber, key, len_key);
}

//������� ������������ ������� ������������ ������� �������������
void CreateIV(byte *src, int len_src, byte *iv, int len_iv){
	memset( iv, 0x00, len_iv );

	for(int i=0; i<len_iv; i++){
		iv[i] = src[i%len_src];
	}
}

AuthProcess::AuthProcess()
{
	m_AuthThread = NULL;
	m_AuthResult = AUTH_ERROR;
	m_uTimeout = 0;
}

AuthProcess::~AuthProcess()
{
	if(m_AuthThread)
		WaitForSingleObject(m_AuthThread->m_hThread, INFINITE);
	m_AuthThread = NULL;
}

void AuthProcess::StopAuth()
{
	m_AuthResult = AUTH_ERROR;
	m_uTimeout = 0;
}

//������� ����������� ��������� �� ��������� AES
//���� ����������� ���������� ����� �������� ���������� ����� (�������� � �������) � ������
CString AuthProcess::EncryptPasswd(const CString &passwd, const CString &nameFile)
{
	using namespace CryptoPP;
	using namespace std;

	#define MAX_NUMBER_IN_FILE 10

	//����������� ����
	string spasswd = (string)StrToUtf8(passwd);
	string snameFile = (string)StrToUtf8(nameFile);

	//������ ����� ���� �� ��������� ������
	srand((UINT)time(NULL));
	fstream rand_file(snameFile.c_str(),ios::out|ios::trunc);
	for(int i=0;i<MAX_NUMBER_IN_FILE;i++){
		rand_file<<rand();
	}
	rand_file.close();

	//���� ��� �� �����
	CryptoPP::Weak::MD5 hash;
	byte hashFile[CryptoPP::Weak::MD5::DIGESTSIZE]; //output size of the buffer
 
	try{
		CryptoPP::FileSource f(snameFile.c_str(), true,
                new CryptoPP::HashFilter(hash,new CryptoPP::ArraySink(hashFile,CryptoPP::Weak::MD5::DIGESTSIZE)));
	}catch(...){
		memset( hashFile, 0x01, CryptoPP::Weak::MD5::DIGESTSIZE );
	}
	
	// Key and IV setup
    byte key[ CryptoPP::Blowfish::DEFAULT_KEYLENGTH ],iv[ CryptoPP::Blowfish::BLOCKSIZE ];
    //������ ����
	CreateKey( key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH);
	CreateIV(hashFile, CryptoPP::Weak::MD5::DIGESTSIZE, iv, CryptoPP::Blowfish::BLOCKSIZE);

	string cipher;
    {
        CryptoPP::StringSink* sink = new CryptoPP::StringSink(cipher);
        CryptoPP::Base64Encoder* base64_enc = new CryptoPP::Base64Encoder(sink);
        CryptoPP::CBC_Mode<CryptoPP::Blowfish>::Encryption twofish(key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH, iv);
        CryptoPP::StreamTransformationFilter* enc = new CryptoPP::StreamTransformationFilter(twofish, base64_enc);
        CryptoPP::Gzip *zip = new CryptoPP::Gzip(enc);
        CryptoPP::StringSource source(spasswd, true, zip);
	}

	return OptUtf8ToStr((CStringA)cipher.c_str());
}


//������� ������������� ������ ��������������� �� ��������� AES
CString AuthProcess::DecryptPasswd(const CString &encpasswd, const CString &nameFile)
{
	using namespace std;

	//����������� ����
	string spasswd = (string)StrToUtf8(encpasswd);
	string snameFile = (string)StrToUtf8(nameFile);
	
	CryptoPP::Weak::MD5 hash;
	byte hashFile[CryptoPP::Weak::MD5::DIGESTSIZE]; //output size of the buffer
	
	try{
		CryptoPP::FileSource f(snameFile.c_str(), true,
                new CryptoPP::HashFilter(hash,new CryptoPP::ArraySink(hashFile,CryptoPP::Weak::MD5::DIGESTSIZE)));
	}catch(...){
		memset( hashFile, 0x01, CryptoPP::Weak::MD5::DIGESTSIZE );
	}
	// Key and IV setup
    byte key[ CryptoPP::Blowfish::DEFAULT_KEYLENGTH ],iv[ CryptoPP::Blowfish::BLOCKSIZE ];
    //������ ����
	CreateKey( key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH);
	CreateIV(hashFile, CryptoPP::Weak::MD5::DIGESTSIZE, iv, CryptoPP::Blowfish::BLOCKSIZE);

	string decipher;
    try {
        CryptoPP::StringSink *sink = new CryptoPP::StringSink(decipher);
        CryptoPP::Gunzip *unzip = new CryptoPP::Gunzip (sink);
        CryptoPP::CBC_Mode<CryptoPP::Blowfish>::Decryption twofish(key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH, iv);
        CryptoPP::StreamTransformationFilter *dec = new CryptoPP::StreamTransformationFilter(twofish, unzip);
        CryptoPP::Base64Decoder *base64_dec = new CryptoPP::Base64Decoder(dec);
        CryptoPP::StringSource source(spasswd, true, base64_dec);
	}catch (...) {
        decipher.clear();
    }

	return OptUtf8ToStr((CStringA)decipher.c_str());
}

UINT AFX_CDECL AuthProcess::AuthThreadWrapper(LPVOID lpParam) {
	AuthProcess *self = static_cast<AuthProcess*>(lpParam);
	self->RequestAuthorization();
	self->m_AuthThread = NULL;	
	PostMessage(theApp.emuledlg->GetSafeHwnd(), TM_AUTHEVENT, 0, (LPARAM)self);
	return 0;
}

//������� ������������� ��������� �� ������� �����������
// � ������ ������� ����� ����
bool AuthProcess::ParseMessage(const char* mess)
{
	TiXmlDocument doc;
	TiXmlElement* xmlElement;
	LPCSTR szElementText;

	doc.Parse(mess);

    TiXmlElement* root = doc.FirstChildElement("DATA");
	if(!root)
		return false;

	// m_Result, Timeout
	xmlElement = root->FirstChildElement("AuthResult");
	if(!xmlElement)
		return false;
	szElementText = xmlElement->GetText();
	if(szElementText)
		m_AuthResult = static_cast<eAuthResult>(atoi(szElementText));
	else
		m_AuthResult = AUTH_ERROR;

	// Timeout
	if(m_AuthResult == AUTH_WAIT)
	{
		xmlElement = root->FirstChildElement("Timeout");
		if(!xmlElement)
			return false;
		szElementText = xmlElement->GetText();
		if(szElementText)
			m_uTimeout = (uint32_t)atoi(szElementText);
	}

	// m_strMessage, m_MsgType
	xmlElement = root->FirstChildElement("Message");
	if(xmlElement){
		szElementText = xmlElement->Attribute("type");
		if(!szElementText){m_AuthMsgType = LOG_MESSAGE;}
		else {
			m_AuthMsgType = static_cast<eAuthMessageType>(atoi(szElementText));
		}

		LPCSTR mess_value = xmlElement->GetText();
		if(mess_value){
			m_strMessage = OptUtf8ToStr((CStringA)mess_value);
		}
	}
	else{
		m_strMessage.Empty();
	}

	/*
    // ������
	xmlElement = root->FirstChildElement( "filter" );
	if(xmlElement)
	{
		CStringA strBadWordA;
		LPSTR szBadList = (LPSTR)xmlElement->GetText();
		
		LPCSTR szTokens = ";";
		LPSTR szBadWord = strtok(szBadList, szTokens);

		thePrefs.m_strBadWordsList.RemoveAll();

		while(szBadWord != NULL)
		{
			strBadWordA	= CharLowerA(szBadWord);
			thePrefs.m_strBadWordsList.AddTail(OptUtf8ToStr(strBadWordA));
			szBadWord = strtok(NULL, szTokens);
			
		}
	}
    */

	return true;
};

void AuthProcess::ShowResponse()
{
	if(!m_strMessage.IsEmpty())
	{
		switch(m_AuthMsgType)
		{
			case LOG_MESSAGE:
				AddLogLine(true, _T("��������� ������� �����������: %s"), m_strMessage);
				break;
			case INFO_MESSAGE_NOMODAL:
				MessageBox(0, m_strMessage, _T("����������"), MB_OK|MB_ICONINFORMATION);
				AddLogLine(true, _T("��������� ������� �����������: %s"), m_strMessage);
				break;
			case INFO_MESSAGE_MODAL:
				theApp.emuledlg->activewnd->MessageBox(m_strMessage, _T("����������"), MB_OK|MB_ICONINFORMATION);
				AddLogLine(true, L"��������� ������� �����������: %s", m_strMessage);
				break;
			case ERR_MESSAGE:
				theApp.emuledlg->activewnd->MessageBox(m_strMessage, L"������", MB_OK|MB_ICONERROR);
				AddLogLine(true, L"��������� ������� �����������: %s", m_strMessage);
				break;
			case WARN_PROTOCOL:
				AddLogLine(true, L"��������� ������� �����������: %s", m_strMessage);
				break;
			case ERR_PROTOCOL:
				AddLogLine(true, L"��������� ������� �����������: %s", m_strMessage);
                theApp.emuledlg->activewnd->MessageBox(m_strMessage, L"������", MB_OK|MB_ICONERROR);
				break;
			default:
				AddLogLine(true,L"����������� ��� ���������");
		}
	}
}

void AuthProcess::SetResponse( LPCTSTR szMessage, eAuthResult res, eAuthMessageType type )
{
	m_strMessage = szMessage;
	m_AuthResult = res;
	m_AuthMsgType = type;
}

//������� ���������
CString AuthProcess::createQuery()
{
	CString request;
	request = L"login=" + CBase64Coding::Encode(thePrefs.GetAuthLogin());
	request += L"&password=" + CBase64Coding::Encode(thePrefs.GetAuthPasswd());
	request += L"&nick=" + CBase64Coding::Encode(thePrefs.GetUserNick());
	request += L"&version=" + CBase64Coding::Encode(IS_SZ_VERSION_NAME);

	return request;
}

// �������� � �������� ��� �� � �������� ������
uint32_t Resolve( LPCSTR szHost )
{
	uint32_t ret = NULL;
	//This structure should never be released!
	struct hostent * hp = gethostbyname(szHost);
	if (!hp)
	{
		ret = inet_addr(szHost);
	}
	if ((!hp)&&(ret == INADDR_NONE))
	{
		return NULL;
	}
	if (hp != NULL)
	{
		memcpy((void*)&ret, hp->h_addr, hp->h_length);
	}
	return ret;
}

//������� �������� ������� �������
void AuthProcess::RequestAuthorization()
{
	CString strPostData = createQuery(); 
	CString strUrl;
	strUrl.Format(_T("https://%s/%s"), m_szServer, thePrefs.GetAuthScript());
	
    std::ostringstream data;
    std::string url = CW2A(strUrl);
    std::string postData = CW2A(strPostData);
	int code = emCurl::httpRequest(url, postData, data);
	if ( 0 != code ) {
		CString error;
		error.Format(L"������ ����������� (%s)", CA2W( emCurl::formatError(code) ) );
		SetResponse(error, AUTH_ERROR, LOG_MESSAGE);
		return;
	}
    std::string dataArray = data.str();

	//������ ��� �����
	//strAnswer = "<?xml version=\"1.0\"><DATA><AuthResult>1</AuthResult><Message type=3><![CDATA[привет засранцы чортовы]]></Message></DATA>";
	ParseMessage( dataArray.c_str() ); //������ ����� ���������� � �������� �������
}

// �������
AuthProcess* AuthProcess::StartAuth(CServerSocket* socket, LPCTSTR szServer)
{
	// ����� ������� �������� ���������
	if(thePrefs.GetAuthScript().IsEmpty()
		|| thePrefs.GetAuthLogin().IsEmpty()
		|| thePrefs.GetAuthPasswd().IsEmpty())
	{
		AfxMessageBox(_T("�� ������ ��������� �����������"), MB_SYSTEMMODAL);

		AddLogLine(true, _T("�� ������ ��������� ����������� (Host:%s Script:%s Login:%s)"),
			szServer, thePrefs.GetAuthScript(), thePrefs.GetAuthLogin());

		CAuthDlg authDlg;
		authDlg.setCredentials(thePrefs.m_strAuthLogin, thePrefs.m_strAuthPasswd);
		if ( authDlg.DoModal() != IDOK ) {
			return NULL;
		}
		else {
			authDlg.getCredentials(thePrefs.m_strAuthLogin, thePrefs.m_strAuthPasswd);
		}
	}

	AuthProcess* fork = new AuthProcess;
	fork->m_AuthThread = CSafeThread::BeginThread(AuthThreadWrapper, fork, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (fork->m_AuthThread == NULL)
		throw CString(_T(__FUNCTION__) _T(": Failed to create worker thread!"));
	fork->m_szServer = szServer;
	fork->m_pParentSocket = socket;
	fork->m_AuthThread->ResumeThread();
	
	AddLogLine(true, _T("������ ����������� � ������� %s"), szServer);
	return fork;
}


// don't call it in non-main thread
void AuthProcess::ProcessAuthResult()
{
	// ����� ��� ��� ��������, �� ������ ������ ����.
	if ( !theApp.serverconnect->IsSocketOpened(m_pParentSocket) ) {
		return;
	}

	ShowResponse();

	switch( m_AuthResult )
	{
	case AUTH_ERROR:
		LogError(LOG_STATUSBAR, _T("����������� �� �������� (Host:%s Script:%s Login:%s)"),
			m_szServer, thePrefs.GetAuthScript(), thePrefs.GetAuthLogin());

		if ( m_AuthMsgType == ERR_MESSAGE )
		{
			CAuthDlg authDlg;
			authDlg.setCredentials(thePrefs.m_strAuthLogin, thePrefs.m_strAuthPasswd);
			if ( authDlg.DoModal() == IDOK ) {
				authDlg.getCredentials(thePrefs.m_strAuthLogin, thePrefs.m_strAuthPasswd);
				theApp.serverconnect->StopConnectionTry();
				theApp.serverconnect->TryAnotherConnectionRequest();
				return;
			} else {
				// ������!
				theApp.serverconnect->StopConnectionTry();
			}
		} else if ( m_AuthMsgType == ERR_PROTOCOL ) {
			theApp.serverconnect->StopConnectionTry();
		} else {
			m_pParentSocket->OnAuthFailed();
		}
		break;
	case AUTH_WAIT:
	{
		LogError(LOG_STATUSBAR, _T("����������� �� ��������, ������ ������������ ��� �� %u ���."), m_uTimeout);
		theApp.m_LastBlacklistedTime = time(NULL) + m_uTimeout;
		break;
	}
	default:
		Log(LOG_STATUSBAR, _T("����������� �������� �� ������� %s"), m_szServer);
		m_pParentSocket->ContinueConnectTo();
	}
}