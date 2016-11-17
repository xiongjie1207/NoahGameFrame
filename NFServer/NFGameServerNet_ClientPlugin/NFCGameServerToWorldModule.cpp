// -------------------------------------------------------------------------
//    @FileName			:    NFCGameServerNet_ClientModule.cpp
//    @Author           :    LvSheng.Huang
//    @Date             :    2013-01-02
//    @Module           :    NFCGameServerNet_ClientModule
//    @Desc             :
// -------------------------------------------------------------------------

#include "NFCGameServerToWorldModule.h"
#include "NFGameServerNet_ClientPlugin.h"
#include "NFComm/NFMessageDefine/NFMsgDefine.h"
#include "NFComm/NFPluginModule/NFINetClientModule.hpp"
#include "NFComm/NFMessageDefine/NFProtocolDefine.hpp"

bool NFCGameServerToWorldModule::Init()
{
	m_pNetClientModule = NF_NEW NFINetClientModule(pPluginManager);

	m_pNetClientModule->Init();

    return true;
}

bool NFCGameServerToWorldModule::Shut()
{

    return true;
}


bool NFCGameServerToWorldModule::Execute()
{
	m_pNetClientModule->Execute();
	ServerReport();
	return true;
}

void NFCGameServerToWorldModule::Register(NFINet* pNet)
{
    NF_SHARE_PTR<NFIClass> xLogicClass = m_pClassModule->GetElement(NFrame::Server::ThisName());
    if (xLogicClass)
    {
        NFList<std::string>& strIdList = xLogicClass->GetIdList();
        std::string strId;
        for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
        {
            const int nServerType = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Type());
            const int nServerID = m_pElementModule->GetPropertyInt(strId, NFrame::Server::ServerID());
            if (nServerType == NF_SERVER_TYPES::NF_ST_GAME && pPluginManager->GetAppID() == nServerID)
            {
                const int nPort = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Port());
                const int nMaxConnect = m_pElementModule->GetPropertyInt(strId, NFrame::Server::MaxOnline());
                const int nCpus = m_pElementModule->GetPropertyInt(strId, NFrame::Server::CpuCount());
                const std::string& strName = m_pElementModule->GetPropertyString(strId, NFrame::Server::Name());
                const std::string& strIP = m_pElementModule->GetPropertyString(strId, NFrame::Server::IP());

                NFMsg::ServerInfoReportList xMsg;
                NFMsg::ServerInfoReport* pData = xMsg.add_server_list();

                pData->set_server_id(nServerID);
                pData->set_server_name(strName);
                pData->set_server_cur_count(0);
                pData->set_server_ip(strIP);
                pData->set_server_port(nPort);
                pData->set_server_max_online(nMaxConnect);
                pData->set_server_state(NFMsg::EST_NARMAL);
                pData->set_server_type(nServerType);

                NF_SHARE_PTR<ConnectData> pServerData = m_pNetClientModule->GetServerNetInfo(pNet);
                if (pServerData)
                {
                    int nTargetID = pServerData->nGameID;
					m_pNetClientModule->SendToServerByPB(nTargetID, NFMsg::EGameMsgID::EGMI_GTW_GAME_REGISTERED, xMsg);

                    m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, pData->server_id()), pData->server_name(), "Register");
                }
            }
        }
    }
}

void NFCGameServerToWorldModule::ServerReport()
{
	if (mLastReportTime + 10 > pPluginManager->GetNowTime())
	{
		return;
	}
	mLastReportTime = pPluginManager->GetNowTime();
	std::shared_ptr<NFIClass> xLogicClass = m_pClassModule->GetElement("Server");
	if (xLogicClass)
	{
		NFList<std::string>& strIdList = xLogicClass->GetIdList();
		std::string strId;
		for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
		{
			const int nServerType = m_pElementModule->GetPropertyInt(strId, "Type");
			const int nServerID = m_pElementModule->GetPropertyInt(strId, "ServerID");
			if ( pPluginManager->GetAppID() == nServerID)
			{
				const int nPort = m_pElementModule->GetPropertyInt(strId, "Port");
				const int nMaxConnect = m_pElementModule->GetPropertyInt(strId, "MaxOnline");
				const std::string& strName = m_pElementModule->GetPropertyString(strId, "Name");
				const std::string& strIP = m_pElementModule->GetPropertyString(strId, "IP");

				NFMsg::ServerInfoReport reqMsg;

				reqMsg.set_server_id(nServerID);
				reqMsg.set_server_name(strName);
				reqMsg.set_server_cur_count(m_pKernelModule->GetOnLineCount());
				reqMsg.set_server_ip(strIP);
				reqMsg.set_server_port(nPort);
				reqMsg.set_server_max_online(nMaxConnect);
				reqMsg.set_server_state(NFMsg::EST_NARMAL);
				reqMsg.set_server_type(nServerType);

				std::shared_ptr<ConnectData> pServerData = m_pNetClientModule->GetServerList().First();
				if (pServerData)
				{
					m_pNetClientModule->SendToServerByPB(pServerData->nGameID, NFMsg::EGMI_STS_SERVER_REPORT, reqMsg);
				}
			}
		}
	}
}


void NFCGameServerToWorldModule::RefreshWorldInfo()
{
    //     _tagPT_KEY_BASE_MSG baseMsg;
    //     NFMsg::ServerInfoReport xMsg;
    //
    //     xMsg.set_server_id(m_pGameLogicModule->GetGameID());
    //     xMsg.set_server_port(m_pGameServerModule->GetServerPort());
    //     xMsg.set_server_cur_count(0);
    //     xMsg.set_server_max_online(50000);
    //
    //     if (xMsg.SerializeToString(&baseMsg.strSyncInfo))
    //     {
    //         baseMsg._unMsgID = GTW_GAME_REFRESH;
    //         RakNet::BitStream oBitStream;
    //         baseMsg.EnCode(&oBitStream);
    //         SendBitStream(&oBitStream);
    //     }
}

bool NFCGameServerToWorldModule::AfterInit()
{
    m_pKernelModule = pPluginManager->FindModule<NFIKernelModule>();
    m_pClassModule = pPluginManager->FindModule<NFIClassModule>();
    m_pElementModule = pPluginManager->FindModule<NFIElementModule>();
    m_pLogModule = pPluginManager->FindModule<NFILogModule>();
    m_pGameServerNet_ServerModule = pPluginManager->FindModule<NFIGameServerNet_ServerModule>();

	m_pNetClientModule->AddReceiveCallBack(this, &NFCGameServerToWorldModule::TransPBToProxy);
	m_pNetClientModule->AddEventCallBack(this, &NFCGameServerToWorldModule::OnSocketWSEvent);

    m_pKernelModule->AddClassCallBack(NFrame::Player::ThisName(), this, &NFCGameServerToWorldModule::OnObjectClassEvent);

    NF_SHARE_PTR<NFIClass> xLogicClass = m_pClassModule->GetElement(NFrame::Server::ThisName());
    if (xLogicClass)
    {
        NFList<std::string>& strIdList = xLogicClass->GetIdList();
        std::string strId;
        for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
        {
            const int nServerType = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Type());
            const int nServerID = m_pElementModule->GetPropertyInt(strId, NFrame::Server::ServerID());
            if (nServerType == NF_SERVER_TYPES::NF_ST_WORLD)
            {
                const int nPort = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Port());
                const int nMaxConnect = m_pElementModule->GetPropertyInt(strId, NFrame::Server::MaxOnline());
                const int nCpus = m_pElementModule->GetPropertyInt(strId, NFrame::Server::CpuCount());
                const std::string& strName = m_pElementModule->GetPropertyString(strId, NFrame::Server::Name());
                const std::string& strIP = m_pElementModule->GetPropertyString(strId, NFrame::Server::IP());

                ConnectData xServerData;

                xServerData.nGameID = nServerID;
                xServerData.eServerType = (NF_SERVER_TYPES)nServerType;
                xServerData.strIP = strIP;
                xServerData.nPort = nPort;
                xServerData.strName = strName;

                m_pNetClientModule->AddServer(xServerData);
            }
        }
    }

    return true;
}

void NFCGameServerToWorldModule::OnSocketWSEvent(const int nSockIndex, const NF_NET_EVENT eEvent, NFINet* pNet)
{
    if (eEvent & NF_NET_EVENT_EOF)
    {
    }
    else if (eEvent & NF_NET_EVENT_ERROR)
    {
    }
    else if (eEvent & NF_NET_EVENT_TIMEOUT)
    {
    }
    else  if (eEvent == NF_NET_EVENT_CONNECTED)
    {
        m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, nSockIndex), "NF_NET_EVENT_CONNECTED", "connectioned success", __FUNCTION__, __LINE__);
        Register(pNet);

    }
}

int NFCGameServerToWorldModule::OnObjectClassEvent(const NFGUID& self, const std::string& strClassName, const CLASS_OBJECT_EVENT eClassEvent, const NFIDataList& var)
{
    if (strClassName == NFrame::Player::ThisName())
    {
        if (CLASS_OBJECT_EVENT::COE_DESTROY == eClassEvent)
        {
            SendOffline(self);
        }
        else if (CLASS_OBJECT_EVENT::COE_CREATE_FINISH == eClassEvent)
        {
            SendOnline(self);
        }
    }

    return 0;
}

void NFCGameServerToWorldModule::SendOnline(const NFGUID& self)
{
    NFMsg::RoleOnlineNotify xMsg;

    const NFGUID& xGuild = m_pKernelModule->GetPropertyObject(self, "GuildID");
    *xMsg.mutable_guild() = NFINetModule::NFToPB(xGuild);

	m_pNetClientModule->SendSuitByPB(xGuild.nData64, NFMsg::EGMI_ACK_ONLINE_NOTIFY, xMsg);

}

void NFCGameServerToWorldModule::SendOffline(const NFGUID& self)
{
    NFMsg::RoleOfflineNotify xMsg;

    const NFGUID& xGuild = m_pKernelModule->GetPropertyObject(self, "GuildID");
    *xMsg.mutable_guild() = NFINetModule::NFToPB(xGuild);

	m_pNetClientModule->SendSuitByPB(xGuild.nData64, NFMsg::EGMI_ACK_OFFLINE_NOTIFY, xMsg);

}

void NFCGameServerToWorldModule::TransPBToProxy(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
    NFGUID nPlayerID;
    std::string strData;
    if (!NFINetModule::ReceivePB(nSockIndex, nMsgID, msg, nLen, strData, nPlayerID))
    {
        return;
    }

    m_pGameServerNet_ServerModule->SendMsgPBToGate(nMsgID, strData, nPlayerID);

    return;
}

void NFCGameServerToWorldModule::SendBySuit(const int& nHashKey, const int nMsgID, const char* msg, const uint32_t nLen)
{
    m_pNetClientModule->SendBySuit(nHashKey, nMsgID, msg, nLen);
}

NFINetClientModule* NFCGameServerToWorldModule::GetClusterClientModule()
{
    return m_pNetClientModule;
}