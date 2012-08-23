//this file is part of eMule IS Mod
//Copyright (C)2011 eMule IS Mod
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once

class CPartFile;

class TransferDebugWnd : public CWnd
{
	DECLARE_DYNAMIC(TransferDebugWnd)

public:
	TransferDebugWnd();
	virtual ~TransferDebugWnd();

	void updateInfo( CPartFile * file );

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();

	CString m_infoString;	
};


