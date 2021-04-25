// ChillerDragon 2021 - chillerbot ux

#include <vector>

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>

#include "warlist.h"

void CWarList::OnInit()
{
	LoadWarList();
	LoadTeamList();
}

int CWarList::LoadWarDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s/names.txt", pDirname);
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

void CWarList::LoadWarList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/war", LoadWarDir, this);
}

void CWarList::LoadTeamList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/team", LoadTeamDir, this);
}

bool CWarList::IsWarlist(const char *pName)
{
	return (std::find(m_vWarlist.begin(), m_vWarlist.end(), std::string(pName)) != m_vWarlist.end());
}

bool CWarList::IsTeamlist(const char *pName)
{
	return (std::find(m_vTeamlist.begin(), m_vTeamlist.end(), std::string(pName)) != m_vTeamlist.end());
}

bool CWarList::IsWar(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsWar;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	return false;
}

bool CWarList::IsTeam(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsTeam;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	return false;
}

void CWarList::SetNameplateColor(int ClientID, STextRenderColor *pColor)
{
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
	else
	{
		// TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f));
		pColor->Set(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

int CWarList::LoadWarNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", pFilename);
		// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
		return 0;
	}
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war list file '%s'", pFilename);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vWarlist.push_back(std::string(pLine));
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
		// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
		return 0;
	}
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team list file '%s'", pFilename);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vTeamlist.push_back(std::string(pLine));
	}

	io_close(File);
	return 0;
}

void CWarList::OnConsoleInit()
{
}

void CWarList::OnRender()
{
}
