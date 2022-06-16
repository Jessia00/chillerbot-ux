// ChillerDragon 2021 - chillerbot ux

#include <vector>

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

#include "chillerbotux.h"

#include "warlist.h"

void CWarList::OnInit()
{
	m_Verbose = true;
	ReloadList();
	m_Verbose = false;
	m_NextReload = time_get() + time_freq();
}

void CWarList::ReloadList()
{
	m_WarDirs = 0;
	m_TeamDirs = 0;
	m_TraitorDirs = 0;
	m_vWarlist.clear();
	m_vTeamlist.clear();
	m_vTraitorlist.clear();
	m_vWarClanlist.clear();
	m_vTeamClanlist.clear();
	LoadWarList();
	LoadTeamList();
	LoadTraitorList();
	LoadWarClanList();
	LoadTeamClanList();
	LoadWarClanPrefixList();
	for(auto &WarPlayer : m_aWarPlayers)
		WarPlayer.m_aName[0] = '\0';
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team: %d war: %d", m_TeamDirs, (m_WarDirs + m_TraitorDirs));
	// TODO: fix on initial load
	// 		 maybe https://github.com/chillerbot/chillerbot-ux/issues/22 is needed
	m_pClient->m_ChillerBotUX.SetComponentNoteLong("war list", aBuf);
}

void CWarList::GetWarlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vWarlist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetTraitorlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTraitorlist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

int CWarList::LoadWarDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s", pDirname);
	return pSelf->LoadWarNames(aFilename);
}

int CWarList::LoadTeamDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/team/%s/names.txt", pDirname);
	return pSelf->LoadTeamNames(aFilename);
}

int CWarList::LoadTraitorDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/traitor/%s", pDirname);
	return pSelf->LoadTraitorNames(aFilename);
}

void CWarList::LoadWarList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/war", LoadWarDir, this);
}

void CWarList::LoadTeamList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/team", LoadTeamDir, this);
}

void CWarList::LoadTraitorList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/traitor", LoadTraitorDir, this);
}

void CWarList::LoadWarClanList()
{
	LoadWarClanNames("chillerbot/warlist/warlist_clans.txt");
}

void CWarList::LoadTeamClanList()
{
	LoadTeamClanNames("chillerbot/warlist/teamlist_clans.txt");
}

void CWarList::LoadWarClanPrefixList()
{
	LoadWarClanPrefixNames("chillerbot/warlist/warlist_clans_prefix.txt");
}

bool CWarList::IsWarlist(const char *pName)
{
	for(auto &WarClanPrefix : m_vWarClanPrefixlist)
		if(str_startswith(pName, WarClanPrefix.c_str()))
			return true;
	return std::any_of(std::begin(m_vWarlist), std::end(m_vWarlist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsTeamlist(const char *pName)
{
	return (std::find(m_vTeamlist.begin(), m_vTeamlist.end(), std::string(pName)) != m_vTeamlist.end());
}

bool CWarList::IsTraitorlist(const char *pName)
{
	return std::any_of(std::begin(m_vTraitorlist), std::end(m_vTraitorlist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsWarClanlist(const char *pClan)
{
	return (std::find(m_vWarClanlist.begin(), m_vWarClanlist.end(), std::string(pClan)) != m_vWarClanlist.end());
}

bool CWarList::IsTeamClanlist(const char *pClan)
{
	return (std::find(m_vTeamClanlist.begin(), m_vTeamClanlist.end(), std::string(pClan)) != m_vTeamClanlist.end());
}

bool CWarList::IsWarClanmate(const char *pClan)
{
	for(auto &WarPlayer : m_aWarPlayers)
	{
		if(WarPlayer.m_aClan[0] == '\0')
			continue;
		if(!WarPlayer.m_IsWar && !WarPlayer.m_IsTraitor)
			continue;

		if(!str_comp(pClan, WarPlayer.m_aClan))
			return true;
	}
	return false;
}

void CWarList::GetWarReason(const char *pName, char *pReason, int ReasonSize)
{
	if(!pReason)
		return;
	pReason[0] = '\0';
	if(!Storage())
		return;

	char aFilenames[2][128];
	char aBuf[182];
	GetWarlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[0], sizeof(aFilenames[0]), "%s/reason.txt", aBuf);
	else
		aFilenames[0][0] = '\0';
	GetTraitorlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[1], sizeof(aFilenames[1]), "%s/reason.txt", aBuf);
	else
		aFilenames[1][0] = '\0';

	for(auto &pFilename : aFilenames)
	{
		if(!pFilename[0])
			continue;

		IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

		if(!File)
			continue;

		char *pLine;
		CLineReader Reader;
		Reader.Init(File);
		// read one line only
		pLine = Reader.Get();
		if(pLine)
			str_copy(pReason, pLine, ReasonSize);

		io_close(File);
		break;
	}
}

bool CWarList::IsWar(const char *pName, const char *pClan)
{
	return IsWarlist(pName) || IsTraitorlist(pName) || IsWarClanlist(pClan);
}

bool CWarList::IsWar(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsWar;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTeam(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsTeam;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTraitor(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsTraitor;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWarClan(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientID].m_aClan))
	{
		return m_aWarPlayers[ClientID].m_IsWarClan;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTeamClan(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientID].m_aClan))
	{
		return m_aWarPlayers[ClientID].m_IsTeamClan;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWarClanmate(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientID].m_aClan))
	{
		return m_aWarPlayers[ClientID].m_IsWarClanmate;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

void CWarList::SetNameplateColor(int ClientID, STextRenderColor *pColor)
{
	if(!g_Config.m_ClWarList)
		return;

	if(IsWar(ClientID))
	{
		// TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
		pColor->Set(7.0f, 0.2f, 0.2f, 1.0f);
	}
	else if(IsTeam(ClientID))
	{
		// TextRender()->TextColor(ColorRGBA(2.0f, 7.0f, 2.0f));
		pColor->Set(0.0f, 0.9f, 0.2f, 1.0f);
	}
	else if(IsTraitor(ClientID))
	{
		// TextRender()->TextColor(ColorRGBA(0.1f, 0.1f, 0.1f));
		pColor->Set(0.1f, 0.1f, 0.1f, 1.0f);
	}
	else if(IsWarClan(ClientID))
	{
		pColor->Set(7.0f, 0.2f, 0.2f, 1.0f);
	}
	else if(IsTeamClan(ClientID))
	{
		pColor->Set(0.0f, 0.9f, 0.2f, 1.0f);
	}
	else if(IsWarClanmate(ClientID))
	{
		pColor->Set(7.0f, 0.5f, 0.2f, 1.0f);
	}
	else
	{
		// TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f));
		pColor->Set(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

int CWarList::LoadWarNames(const char *pDir)
{
	if(!Storage())
		return 1;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_WarDirs++;
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war list file '%s'", aFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vWarlist.emplace_back(Entry);
	}

	io_close(File);
	return 0;
}

int CWarList::LoadTeamNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	m_TeamDirs++;
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vTeamlist.emplace_back(std::string(pLine));
	}

	io_close(File);
	return 0;
}

int CWarList::LoadTraitorNames(const char *pDir)
{
	if(!Storage())
		return 1;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_TraitorDirs++;
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading traitor list file '%s'", aFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vTraitorlist.emplace_back(Entry);
	}

	io_close(File);
	return 0;
}

int CWarList::LoadWarClanNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war clans list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war clans list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vWarClanlist.emplace_back(std::string(pLine));
	}

	io_close(File);
	return 0;
}

int CWarList::LoadTeamClanNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open team clans list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team clans list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vTeamClanlist.emplace_back(std::string(pLine));
	}

	io_close(File);
	return 0;
}

int CWarList::LoadWarClanPrefixNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war clan prefix list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war clan prefix list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vWarClanPrefixlist.emplace_back(std::string(pLine));
	}

	io_close(File);
	return 0;
}

void CWarList::OnConsoleInit()
{
	Console()->Register("warlist", "?s[command]?s[arg1]?s[arg2]", CFGFLAG_CLIENT, ConWarlist, this, "Activate afk mode (auto chat respond)");

	Console()->Chain("cl_war_list", ConchainWarList, this);
}

void CWarList::ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
	{
		pSelf->m_pClient->m_ChillerBotUX.EnableComponent("war list");
		pSelf->ReloadList();
	}
	else
		pSelf->m_pClient->m_ChillerBotUX.DisableComponent("war list");
}

void CWarList::OnRender()
{
	if(!g_Config.m_ClWarList)
		return;

	if(g_Config.m_ClWarListAutoReload && time_get() > m_NextReload)
	{
		m_NextReload = time_get() + time_freq() * g_Config.m_ClWarListAutoReload;
		ReloadList();
	}
}

void CWarList::ConWarlist(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	if(!str_comp(pResult->GetString(0), "reload"))
	{
		pSelf->m_Verbose = true;
		pSelf->ReloadList();
		pSelf->m_Verbose = false;
	}
	else if(!str_comp(pResult->GetString(0), "help"))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "warlist ?s[command]?s[arg1]?s[arg2]");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "commands:");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "  reload - loads the war list again from filesystem");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "  help - shows this help");
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "unkown warlist command try help");
	}
}

void CWarList::Print(const char *pMsg)
{
	if(!m_Verbose)
		return;

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", pMsg);
}

void CWarList::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	if(!g_Config.m_ClWarList)
		return;
	if(ClientID != m_pClient->m_Snap.m_LocalClientID)
		return;
	char aBuf[512];
	if(pMsg[0] == '.' || pMsg[0] == ':' || pMsg[0] == '!' || pMsg[0] == '#' || pMsg[0] == '$' || pMsg[0] == '/')
	{
		const char *pBuf = pMsg + 1;
		if(str_startswith(pBuf, "addwar ")) // "addwar <folder> <name can contain spaces>"
		{
			pBuf += str_length("addwar ");
			aBuf[0] = '\0';
			unsigned int i = 0;
			for(i = 0; pBuf[i] != '\0' && pBuf[i] != ' ' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aFolder[512];
			str_copy(aFolder, aBuf, sizeof(aFolder));
			aBuf[0] = '\0';
			pBuf += i + 1;
			for(i = 0; pBuf[i] != '\0' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aName[512];
			str_copy(aName, aBuf, sizeof(aName));
			char aFilename[1024];
			str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s/names.txt", aFolder);
			IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
			if(!File)
			{
				str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return;
			}

			io_write(File, aName, str_length(aName));
			io_write_newline(File);
			io_close(File);

			str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", aName, aFolder);
			ReloadList();
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
		if(str_startswith(pBuf, "addteam ")) // "addteam <folder> <name can contain spaces>"
		{
			pBuf += str_length("addteam ");
			aBuf[0] = '\0';
			unsigned int i = 0;
			for(i = 0; pBuf[i] != '\0' && pBuf[i] != ' ' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aFolder[512];
			str_copy(aFolder, aBuf, sizeof(aFolder));
			aBuf[0] = '\0';
			pBuf += i + 1;
			for(i = 0; pBuf[i] != '\0' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aName[512];
			str_copy(aName, aBuf, sizeof(aName));
			char aFilename[1024];
			str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/team/%s/names.txt", aFolder);
			IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
			if(!File)
			{
				str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return;
			}

			io_write(File, aName, str_length(aName));
			io_write_newline(File);
			io_close(File);

			str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", aName, aFolder);
			ReloadList();
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
		else if(str_startswith(pBuf, "peace ")) // "peace <folder>"
		{
			pBuf += str_length("peace ");
			aBuf[0] = '\0';
			unsigned int i = 0;
			for(i = 0; pBuf[i] != '\0' && pBuf[i] != ' ' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aFolder[512];
			str_copy(aFolder, aBuf, sizeof(aFolder));
			aBuf[0] = '\0';
			char aPath[1024];
			char aPeacePath[1024];
			str_format(aPath, sizeof(aPath), "chillerbot/warlist/war/%s/names.txt", aFolder);
			str_format(aPeacePath, sizeof(aPeacePath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
			IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(!File)
			{
				str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return;
			}
			io_close(File);
			File = Storage()->OpenFile(aPeacePath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(File)
			{
				str_format(aBuf, sizeof(aBuf), "Peace entry already exists '%s'", aPeacePath);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				io_close(File);
				return;
			}

			str_format(aPath, sizeof(aPath), "chillerbot/warlist/war/%s", aFolder);
			str_format(aPeacePath, sizeof(aPeacePath), "chillerbot/warlist/neutral/%s", aFolder);
			Storage()->RenameFile(aPath, aPeacePath, IStorage::TYPE_SAVE);

			str_format(aBuf, sizeof(aBuf), "Moved folder %s from war/ to neutral/", aFolder);
			ReloadList();
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
		else if(str_startswith(pBuf, "team ")) // "team <folder>"
		{
			pBuf += str_length("team ");
			aBuf[0] = '\0';
			unsigned int i = 0;
			for(i = 0; pBuf[i] != '\0' && pBuf[i] != ' ' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aFolder[512];
			str_copy(aFolder, aBuf, sizeof(aFolder));
			aBuf[0] = '\0';
			char aPath[1024];
			char aTeamPath[1024];
			str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
			str_format(aTeamPath, sizeof(aTeamPath), "chillerbot/warlist/team/%s/names.txt", aFolder);
			IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(!File)
			{
				str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return;
			}
			io_close(File);
			File = Storage()->OpenFile(aTeamPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(File)
			{
				str_format(aBuf, sizeof(aBuf), "Peace entry already exists '%s'", aTeamPath);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				io_close(File);
				return;
			}

			str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s", aFolder);
			str_format(aTeamPath, sizeof(aTeamPath), "chillerbot/warlist/team/%s", aFolder);
			Storage()->RenameFile(aPath, aTeamPath, IStorage::TYPE_SAVE);

			str_format(aBuf, sizeof(aBuf), "Moved folder %s from neutral/ to team/", aFolder);
			ReloadList();
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
		else if(str_startswith(pBuf, "war ")) // "war <folder>"
		{
			pBuf += str_length("war ");
			aBuf[0] = '\0';
			unsigned int i = 0;
			for(i = 0; pBuf[i] != '\0' && pBuf[i] != ' ' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aFolder[512];
			str_copy(aFolder, aBuf, sizeof(aFolder));
			aBuf[0] = '\0';
			char aPath[1024];
			char aWarPath[1024];
			str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
			str_format(aWarPath, sizeof(aWarPath), "chillerbot/warlist/war/%s/names.txt", aFolder);
			IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(!File)
			{
				str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return;
			}
			io_close(File);
			File = Storage()->OpenFile(aWarPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(File)
			{
				str_format(aBuf, sizeof(aBuf), "War entry already exists '%s'", aWarPath);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				io_close(File);
				return;
			}

			str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s", aFolder);
			str_format(aWarPath, sizeof(aWarPath), "chillerbot/warlist/war/%s", aFolder);
			Storage()->RenameFile(aPath, aWarPath, IStorage::TYPE_SAVE);

			str_format(aBuf, sizeof(aBuf), "Moved folder %s from neutral/ to war/", aFolder);
			ReloadList();
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
		else if(str_startswith(pBuf, "addreason ")) // "addreason <folder> <reason can contain spaces>"
		{
			pBuf += str_length("addreason ");
			aBuf[0] = '\0';
			unsigned int i = 0;
			for(i = 0; pBuf[i] != '\0' && pBuf[i] != ' ' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aFolder[512];
			str_copy(aFolder, aBuf, sizeof(aFolder));
			aBuf[0] = '\0';
			pBuf += i + 1;
			for(i = 0; pBuf[i] != '\0' && i < sizeof(aBuf); i++)
				aBuf[i] = pBuf[i];
			aBuf[i] = '\0';
			char aReason[512];
			str_copy(aReason, aBuf, sizeof(aReason));
			const char *pReason = aReason;
			bool Force = true;
			if(str_startswith(aReason, "-f "))
				pReason += str_length("-f ");
			else if(str_startswith(aReason, "--force "))
				pReason += str_length("--force ");
			else
				Force = false;
			char aFilename[1024];
			str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s/reason.txt", aFolder);
			IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(File)
			{
				char *pLine;
				CLineReader Reader;
				Reader.Init(File);
				// read one line only
				pLine = Reader.Get();
				if(pLine && pLine[0] != '\0' && !Force)
				{
					str_format(aBuf, sizeof(aBuf), "Folder %s already has a reason. Use -f to overwrite reason: %s", aFolder, pLine);
					m_pClient->m_Chat.AddLine(-2, 0, aBuf);
					return;
				}
				io_close(File);
			}
			File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(!File)
			{
				str_format(aBuf, sizeof(aBuf), "Failed to open file for writing chillerbot/warlist/war/%s/reason.txt", aFolder);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return;
			}
			str_format(aBuf, sizeof(aBuf), "Adding reason to folder='%s' reason='%s'", aFolder, pReason);
			io_write(File, pReason, str_length(pReason));
			char aDate[32];
			str_timestamp(aDate, sizeof(aDate));
			str_format(aBuf, sizeof(aBuf), " (%s)", aDate);
			io_write(File, aBuf, str_length(aBuf));
			io_write_newline(File);
			io_close(File);
			ReloadList();
			m_pClient->m_Chat.AddLine(-1, 0, aBuf);
		}
	}
}

void CWarList::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}
