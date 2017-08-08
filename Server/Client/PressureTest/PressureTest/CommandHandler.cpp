﻿#include "stdafx.h"
#include "CommandHandler.h"
#include <complex>
#include "..\Src\Message\Msg_RetCode.pb.h"
#include "PacketHeader.h"
#include "..\Src\Message\Msg_Move.pb.h"
#include "Utility\CommonConvert.h"
#include "..\Src\Message\Game_Define.pb.h"
#include "Utility\CommonFunc.h"
#include "..\Src\Message\Msg_Copy.pb.h"
#include "..\Src\Message\Msg_Game.pb.h"
#include "Utility\XMath.h"

int g_LoginReqCount = 0;
int g_LoginCount = 0;
int g_EnterCount = 0;

CClientCmdHandler::CClientCmdHandler(void)
{
	m_bLoginOK = FALSE;

	m_dwAccountID = 0;
	m_dwHostState = ST_NONE;
	m_vx = 0;
	m_vy = 0;
	m_vz = 0;
	m_x = 0;
	m_y = 0;
	m_z = 13;

	m_ClientConnector.RegisterMsgHandler((IMessageHandler*)this);
}

CClientCmdHandler::~CClientCmdHandler(void)
{
}

BOOL CClientCmdHandler::DispatchPacket(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	BOOL bHandled = TRUE;
	switch(dwMsgID)
	{
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_ACCOUNT_REG_ACK,		OnCmdNewAccountAck);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_ACCOUNT_LOGIN_ACK,		OnMsgAccountLoginAck);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_SELECT_SERVER_ACK,      OnMsgSelectServerAck);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_ROLE_LIST_ACK,			OnMsgRoleListAck);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_NOTIFY_INTO_SCENE,		OnMsgNotifyIntoScene);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_ROLE_CREATE_ACK,		OnMsgCreateRoleAck);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_OBJECT_NEW_NTY,			OnMsgObjectNewNty);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_OBJECT_ACTION_NTY,		OnMsgObjectActionNty);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_OBJECT_REMOVE_NTY,		OnMsgObjectRemoveNty);
			PROCESS_MESSAGE_ITEM_CLIENT(MSG_ENTER_SCENE_ACK,		OnCmdEnterSceneAck);


		default:
		{
			bHandled = FALSE;
		}
		break;
	}

	return bHandled;
}


BOOL CClientCmdHandler::OnCmdEnterSceneAck( UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen )
{
	EnterSceneAck Ack;
	Ack.ParsePartialFromArray(PacketBuf, BufLen);
	PacketHeader* pHeader = (PacketHeader*)PacketBuf;
	if(Ack.copyid() == 6)
	{
		m_dwHostState = ST_EnterSceneOK;

		//表示进入主城完成
	}
	else if(Ack.copyid() == m_dwToCopyID)
	{
		m_dwHostState = ST_EnterCopyOK;
	}

	m_dwCopyID = Ack.copyid();
	m_dwCopyGuid = Ack.copyguid();
	//if(Ack.copytype() == 6)
	//{
	//	SendMainCopyReq();
	//}
	//else
	//{
	//	SendAbortCopyReq();
	//}


	return TRUE;
}

BOOL CClientCmdHandler::OnMsgNotifyIntoScene(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	NotifyIntoScene Nty;
	Nty.ParsePartialFromArray(PacketBuf, BufLen);
	EnterSceneReq Req;
	Req.set_roleid(Nty.roleid());
	Req.set_serverid(Nty.serverid());
	Req.set_copyguid(Nty.copyguid());
	Req.set_copyid(Nty.copyid());

	m_dwCopySvrID = Nty.serverid();
	m_dwCopyID = Nty.copyid();
	m_dwCopyGuid = Nty.copyguid();

	m_ClientConnector.SendData(MSG_ENTER_SCENE_REQ, Req, Nty.serverid(), Nty.copyguid());
	return TRUE;
}

BOOL CClientCmdHandler::OnMsgObjectNewNty(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	return TRUE;
}

BOOL CClientCmdHandler::OnMsgObjectActionNty(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	return TRUE;
}

BOOL CClientCmdHandler::OnMsgObjectRemoveNty(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	return TRUE;
}

BOOL CClientCmdHandler::OnUpdate( UINT32 dwTick )
{
	m_ClientConnector.Render();

	if(m_dwHostState == ST_NONE)
	{
		if(m_ClientConnector.GetConnectState() == Not_Connect)
		{
			m_ClientConnector.SetClientID(0);

			m_ClientConnector.ConnectToServer("47.93.31.69", 5678);
		}

		if(m_ClientConnector.GetConnectState() == Succ_Connect)
		{
			SendNewAccountReq(m_strAccountName, m_strPassword);

			m_dwHostState = ST_Register;
		}
	}

	if(m_dwHostState == ST_RegisterOK)
	{
		SendAccountLoginReq(m_strAccountName, m_strPassword);

		m_dwHostState = ST_Login;
	}

	if(m_dwHostState == ST_LoginOK)
	{
		SendSelectSvrReq(0);

		m_dwHostState = ST_SelectSvr;
	}

	if(m_dwHostState == ST_SelectSvrOK)
	{
		SendRoleListReq();

		m_dwHostState = ST_RoleList;
	}

	if(m_dwHostState == ST_RoleListOk)
	{
		SendRoleLoginReq(0);
		m_dwHostState = ST_EnterScene;
	}

	if(m_dwHostState == ST_EnterSceneOK)
	{
		TestMove();
		//TestCopy();
	}

	if(m_dwHostState == ST_EnterCopyOK)
	{
		//TestMove();
		//TestExitCopy();
	}

	if(m_dwHostState == ST_Disconnected)
	{
		int randValue = rand() % 100;
		if((randValue < 80) && (randValue > 70))
		{
			m_dwHostState = ST_NONE;
		}
	}

	return TRUE;
}


BOOL CClientCmdHandler::SendNewAccountReq( std::string szAccountName, std::string szPassword )
{
	AccountRegReq Req;
	Req.set_accountname(szAccountName);
	Req.set_password(szPassword);
	m_ClientConnector.SendData(MSG_ACCOUNT_REG_REQ, Req, 0, 0);
	return TRUE;
}



BOOL CClientCmdHandler::SendAccountLoginReq(std::string szAccountName, std::string szPassword)
{
	AccountLoginReq Req;
	Req.set_accountname(szAccountName);
	Req.set_password(szPassword);
	m_ClientConnector.SendData(MSG_ACCOUNT_LOGIN_REQ, Req, 0, 0);

	return TRUE;
}

BOOL CClientCmdHandler::SendSelectSvrReq(UINT32 dwSvrID)
{
	SelectServerReq Req;
	Req.set_serverid(201);
	m_ClientConnector.SendData(MSG_SELECT_SERVER_REQ, Req, 0, 0);
	return TRUE;
}

BOOL CClientCmdHandler::OnMsgAccountLoginAck(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	AccountLoginAck Ack;
	Ack.ParsePartialFromArray(PacketBuf, BufLen);
	PacketHeader* pHeader = (PacketHeader*)PacketBuf;

	if(Ack.retcode() == MRC_FAILED)
	{
		MessageBox(NULL, "登录失败! 密码或账号不对!!", "提示", MB_OK);
	}
	else
	{
		m_dwAccountID = Ack.accountid();
	}

	m_dwHostState = ST_LoginOK;
	return TRUE;
}


BOOL CClientCmdHandler::OnMsgSelectServerAck(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	SelectServerAck Ack;
	Ack.ParsePartialFromArray(PacketBuf, BufLen);
	PacketHeader* pHeader = (PacketHeader*)PacketBuf;
	m_ClientConnector.DisConnect();
	m_ClientConnector.ConnectToServer(Ack.serveraddr(), Ack.serverport());
	m_dwHostState = ST_SelectSvrOK;
	return TRUE;
}

BOOL CClientCmdHandler::OnMsgRoleListAck(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	RoleListAck Ack;
	Ack.ParsePartialFromArray(PacketBuf, BufLen);
	PacketHeader* pHeader = (PacketHeader*)PacketBuf;

	for( int i = 0 ; i < Ack.rolelist_size(); i++)
	{
		m_RoleIDList.push_back(Ack.rolelist(i).roleid());
		m_dwHostState = ST_RoleListOk;
	}

	if(Ack.rolelist_size() <= 0)
	{
		SendCreateRoleReq(m_dwAccountID, m_strAccountName + CommonConvert::IntToString(rand() % 1000), rand() % 4 + 1);
	}

	return TRUE;
}

BOOL CClientCmdHandler::OnMsgCreateRoleAck(UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	RoleCreateAck Ack;
	Ack.ParsePartialFromArray(PacketBuf, BufLen);
	PacketHeader* pHeader = (PacketHeader*)PacketBuf;
	m_RoleIDList.push_back(Ack.roleid());
	m_dwHostState = ST_RoleListOk;
	return TRUE;
}

BOOL CClientCmdHandler::OnCmdNewAccountAck( UINT32 dwMsgID, CHAR* PacketBuf, INT32 BufLen)
{
	AccountRegAck Ack;
	Ack.ParsePartialFromArray(PacketBuf, BufLen);
	PacketHeader* pHeader = (PacketHeader*)PacketBuf;
	m_dwHostState = ST_RegisterOK;
	return TRUE;
}

BOOL CClientCmdHandler::SendCreateRoleReq( UINT64 dwAccountID, std::string strName, UINT32 dwCarrerID)
{
	RoleCreateReq Req;
	Req.set_accountid(dwAccountID);
	Req.set_name(strName);
	Req.set_carrer(dwCarrerID);
	m_ClientConnector.SendData(MSG_ROLE_CREATE_REQ, Req, 0, 0);
	return TRUE;
}



BOOL CClientCmdHandler::SendDelCharReq( UINT64 dwAccountID, UINT64 u64RoleID )
{
	RoleDeleteReq Req;
	Req.set_accountid(dwAccountID);
	Req.set_roleid(u64RoleID);
	m_ClientConnector.SendData(MSG_ROLE_DELETE_REQ, Req, 0, 0);
	return TRUE;
}

VOID CClientCmdHandler::TestCopy()
{
	MainCopyReq Req;
	Req.set_copyid(rand() % 67 + 10001);
	m_dwToCopyID = Req.copyid();
	m_ClientConnector.SendData(MSG_MAIN_COPY_REQ, Req, m_RoleIDList[0], 0);
	m_dwHostState = ST_EnterCopy;
}


VOID CClientCmdHandler::TestExitCopy()
{
	AbortCopyReq Req;
	Req.set_copyid(m_dwCopyID);
	Req.set_copyguid(m_dwCopyGuid);
	Req.set_serverid(m_dwCopySvrID);
	m_ClientConnector.SendData(MSG_COPY_ABORT_REQ, Req, m_RoleIDList[0], 0);
	m_dwHostState = ST_EnterCopy;
}

VOID CClientCmdHandler::TestMove()
{
	ObjectActionReq Req;
	ActionItem* pItem =  Req.add_actionlist();
	pItem->set_actionid(AT_WALK);
	pItem->set_objectguid(m_RoleIDList[0]);

	UINT32 dwTimeDiff = CommonFunc::GetTickCount() - m_dwMoveTime;
	if(dwTimeDiff < 100)
	{
		return ;
	}

	m_dwMoveTime = CommonFunc::GetTickCount();

	UINT32 dwRand = m_RoleIDList[0] % 4;

	m_vx = rand() % 40 - 20;
	m_vz = rand() % 40 - 20;

	CPoint2d Dir(m_vx, m_vz);
	Dir.Normalized();

	m_x += Dir.m_x;
	m_z += Dir.m_y;
	m_y = m_y;

	if(m_x > 20) { m_x = 20; }
	if(m_z > 20) { m_z = 20; }
	if(m_x < -20) { m_x = -20; }
	if(m_z < -20) { m_z = -20; }

	pItem->set_x(m_x);
	pItem->set_y(0);
	pItem->set_z(m_z);
	pItem->set_vx(m_vx);
	pItem->set_vy(m_vy);
	pItem->set_vz(m_vz);

	m_ClientConnector.SendData(MSG_OBJECT_ACTION_REQ, Req, m_RoleIDList[0], m_dwCopyGuid);
}

BOOL CClientCmdHandler::SendRoleLogoutReq( UINT64 u64CharID )
{
	return TRUE;
}

BOOL CClientCmdHandler::SendRoleLoginReq(UINT64 u64CharID)
{
	RoleLoginReq Req;
	Req.set_roleid(m_RoleIDList[0]);
	m_ClientConnector.SendData(MSG_ROLE_LOGIN_REQ, Req, 0, 0);
	return TRUE;
}



BOOL CClientCmdHandler::SendRoleListReq()
{
	RoleListReq Req;
	Req.set_accountid(m_dwAccountID);
	Req.set_logincode(12345678);
	m_ClientConnector.SendData(MSG_ROLE_LIST_REQ, Req, 0, 0);
	return TRUE;
}

BOOL CClientCmdHandler::SendMainCopyReq()
{
	MainCopyReq Req;
	Req.set_copyid(rand() % 67 + 10000);
	m_ClientConnector.SendData(MSG_MAIN_COPY_REQ, Req, m_RoleIDList[0], 0);
	return TRUE;
}

BOOL CClientCmdHandler::SendAbortCopyReq()
{
	AbortCopyReq Req;
	Req.set_copyid(m_dwCopyGuid);
	m_ClientConnector.SendData(MSG_COPY_ABORT_REQ, Req, m_RoleIDList[0], 0);

	m_dwHostState = ST_AbortCopy;
	return TRUE;
}
