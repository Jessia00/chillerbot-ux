// ChillerDragon 2020 - chillerbot ux

#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chillerbotux.h>
#include <game/client/gameclient.h>

#include <cinttypes>

#include "chathelper.h"

CChatHelper::CChatHelper()
{
	mem_zero(m_aaChatFilter, sizeof(m_aaChatFilter));
#define CHILLERBOT_CHAT_CMD(name, params, flags, callback, userdata, help) RegisterCommand(name, params, flags, help);
#include "chatcommands.h"
	m_Commands.sort_range();
}

void CChatHelper::RegisterCommand(const char *pName, const char *pParams, int flags, const char *pHelp)
{
	m_Commands.add_unsorted(CCommand{pName, pParams});
}

void CChatHelper::OnInit()
{
	m_pChillerBot = &m_pClient->m_ChillerBotUX;

	m_aGreetName[0] = '\0';
	m_NextGreetClear = 0;
	m_NextMessageSend = 0;
	m_NextPingMsgClear = 0;
	m_aLastAfkPing[0] = '\0';
	m_aLastPingMessage[0] = '\0';
	m_aLastPingName[0] = '\0';
	mem_zero(m_aSendBuffer, sizeof(m_aSendBuffer));
}

void CChatHelper::OnRender()
{
	int64_t time_now = time_get();
	if(time_now % 10 == 0)
	{
		if(m_NextGreetClear < time_now)
		{
			m_NextGreetClear = 0;
			m_aGreetName[0] = '\0';
		}
		if(m_NextPingMsgClear < time_now)
		{
			m_NextPingMsgClear = 0;
			m_aLastPingMessage[0] = '\0';
		}
		if(m_NextMessageSend < time_now)
		{
			if(m_aSendBuffer[0][0])
			{
				m_pClient->m_Chat.Say(0, m_aSendBuffer[0]);
				for(int i = 0; i < MAX_CHAT_BUFFER_LEN - 1; i++)
					str_copy(m_aSendBuffer[i], m_aSendBuffer[i + 1], sizeof(m_aSendBuffer[i]));
				m_aSendBuffer[MAX_CHAT_BUFFER_LEN - 1][0] = '\0';
				m_NextMessageSend = time_now + time_freq() * 5;
			}
		}
	}
}

void CChatHelper::SayBuffer(const char *pMsg, bool StayAfk)
{
	if(StayAfk)
		m_pClient->m_ChillerBotUX.m_IgnoreChatAfk++;
	// append at end
	for(auto &buf : m_aSendBuffer)
	{
		if(buf[0])
			continue;
		str_copy(buf, pMsg, sizeof(buf));
		return;
	}
	// full -> shift buffer and overwrite oldest element (index 0)
	for(int i = 0; i < MAX_CHAT_BUFFER_LEN - 1; i++)
		str_copy(m_aSendBuffer[i], m_aSendBuffer[i + 1], sizeof(m_aSendBuffer[i]));
	str_copy(m_aSendBuffer[MAX_CHAT_BUFFER_LEN - 1], pMsg, sizeof(m_aSendBuffer[MAX_CHAT_BUFFER_LEN - 1]));
}

void CChatHelper::OnConsoleInit()
{
	Console()->Register("reply_to_last_ping", "", CFGFLAG_CLIENT, ConReplyToLastPing, this, "Respond to the last ping in chat");
	Console()->Register("say_hi", "", CFGFLAG_CLIENT, ConSayHi, this, "Respond to the last greeting in chat");
	Console()->Register("say_format", "s[message]", CFGFLAG_CLIENT, ConSayFormat, this, "send message replacing %n with last ping name");
	Console()->Register("chat_filter_add", "s[text]", CFGFLAG_CLIENT, ConAddChatFilter, this, "Add string to chat filter. All messages containing that string will be ignored.");
	Console()->Register("chat_filter_list", "", CFGFLAG_CLIENT, ConListChatFilter, this, "list all active filters");
	Console()->Register("chat_filter_delete", "i[index]", CFGFLAG_CLIENT, ConDeleteChatFilter, this, "");
}

void CChatHelper::ConReplyToLastPing(IConsole::IResult *pResult, void *pUserData)
{
	CChatHelper *pSelf = (CChatHelper *)pUserData;
	char aResponse[1024];
	if(pSelf->ReplyToLastPing(pSelf->m_aLastPingName, pSelf->m_aLastPingMessage, aResponse, sizeof(aResponse)))
	{
		pSelf->m_aLastPingMessage[0] = '\0';
		if(aResponse[0])
			pSelf->m_pClient->m_Chat.Say(0, aResponse);
	}
}

void CChatHelper::ConSayHi(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->DoGreet();
}

void CChatHelper::ConSayFormat(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->SayFormat(pResult->GetString(0));
}

void CChatHelper::ConAddChatFilter(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->AddChatFilter(pResult->GetString(0));
}

void CChatHelper::ConListChatFilter(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->ListChatFilter();
}

void CChatHelper::ConDeleteChatFilter(IConsole::IResult *pResult, void *pUserData)
{
}

bool CChatHelper::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_find_nocase(pLine, pName);

	if(pHL)
	{
		int Length = str_length(pName);

		if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}

	return false;
}

void CChatHelper::SayFormat(const char *pMsg)
{
	char aBuf[1028] = {0};
	long unsigned int i = 0;
	long unsigned int buf_i = 0;
	for(i = 0; pMsg[i] && i < sizeof(aBuf); i++)
	{
		if(pMsg[i] == '%' && pMsg[maximum((int)i - 1, 0)] != '\\')
		{
			if(pMsg[i + 1] == 'n')
			{
				str_append(aBuf, m_aLastPingName, sizeof(aBuf));
				buf_i += str_length(m_aLastPingName);
				i++;
				continue;
			}
			else if(pMsg[i + 1] == 'g')
			{
				str_append(aBuf, m_aGreetName, sizeof(aBuf));
				buf_i += str_length(m_aGreetName);
				i++;
				continue;
			}
		}
		aBuf[buf_i++] = pMsg[i];
	}
	aBuf[minimum((unsigned long)sizeof(aBuf) - 1, buf_i)] = '\0';
	m_pClient->m_Chat.Say(0, aBuf);
}

bool CChatHelper::ReplyToLastPing(const char *pMessageAuthor, const char *pMessage, char *pResponse, int SizeOfResponse)
{
	pResponse[0] = '\0';
	if(pMessageAuthor[0] == '\0')
		return false;
	if(pMessage[0] == '\0')
		return false;

	int MsgLen = str_length(pMessage);
	int NameLen = 0;
	const char *pName = m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName;
	const char *pDummyName = m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName;

	if(LineShouldHighlight(pMessage, pName))
		NameLen = str_length(pName);
	else if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMessage, pDummyName))
		NameLen = str_length(pDummyName);

	// ping without further context
	if(MsgLen < NameLen + 2)
	{
		str_format(pResponse, SizeOfResponse, "%s ?", pMessageAuthor);
		return true;
	}
	// greetings
	if(m_LangParser.IsGreeting(pMessage))
	{
		str_format(pResponse, SizeOfResponse, "hi %s", pMessageAuthor);
		return true;
	}
	if(m_LangParser.IsBye(pMessage))
	{
		str_format(pResponse, SizeOfResponse, "bye %s", pMessageAuthor);
		return true;
	}
	// check war for others
	const char *pWhy = str_find_nocase(pMessage, "why");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "stop");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "do not");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "don't");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "do u");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "do you");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "is u");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "is you");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "are u");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "are you");
	if(pWhy)
	{
		const char *pKill = NULL;
		pKill = str_find_nocase(pWhy, "kill "); // why do you kill foo?
		if(pKill)
			pKill = pKill + str_length("kill ");
		else if((pKill = str_find_nocase(pWhy, "kil "))) // why kil foo?
			pKill = pKill + str_length("kil ");
		else if((pKill = str_find_nocase(pWhy, "killed "))) // why killed foo?
			pKill = pKill + str_length("killed ");
		else if((pKill = str_find_nocase(pWhy, "block "))) // why do you block foo?
			pKill = pKill + str_length("block ");
		else if((pKill = str_find_nocase(pWhy, "blocked "))) // why do you blocked foo?
			pKill = pKill + str_length("blocked ");
		else if((pKill = str_find_nocase(pWhy, "help "))) // why no help foo?
			pKill = pKill + str_length("help ");
		else if((pKill = str_find_nocase(pWhy, "war with "))) // why do you have war with foo?
			pKill = pKill + str_length("war with ");
		else if((pKill = str_find_nocase(pWhy, "war "))) // why war foo?
			pKill = pKill + str_length("war ");
		else if((pKill = str_find_nocase(pWhy, "wared "))) // why wared foo?
			pKill = pKill + str_length("wared ");
		else if((pKill = str_find_nocase(pWhy, "waring "))) // why waring foo?
			pKill = pKill + str_length("waring ");
		else if((pKill = str_find_nocase(pWhy, "bully "))) // why bully foo?
			pKill = pKill + str_length("bully ");
		else if((pKill = str_find_nocase(pWhy, "bullying "))) // why bullying foo?
			pKill = pKill + str_length("bullying ");
		else if((pKill = str_find_nocase(pWhy, "bullied "))) // why bullied foo?
			pKill = pKill + str_length("bullied ");
		else if((pKill = str_find_nocase(pWhy, "freeze "))) // why freeze foo?
			pKill = pKill + str_length("freeze ");

		if(pKill)
		{
			bool HasWar = true;
			// aVictim also has to hold the full own name to match the chop off
			char aVictim[MAX_NAME_LENGTH + 3 + MAX_NAME_LENGTH];
			str_copy(aVictim, pKill, sizeof(aVictim));
			if(!m_pClient->m_WarList.IsWarlist(aVictim) && !m_pClient->m_WarList.IsTraitorlist(aVictim))
			{
				HasWar = false;
				if(str_endswith(aVictim, "?")) // cut off the question mark from the victim name
					aVictim[str_length(aVictim) - 1] = '\0';
				// cut off own name from the victime name if question in this format "why do you war foo (your name)"
				char aOwnName[MAX_NAME_LENGTH + 3];
				// main tee
				str_format(aOwnName, sizeof(aOwnName), " %s", m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName);
				if(str_endswith(aVictim, aOwnName))
					aVictim[str_length(aVictim) - str_length(aOwnName)] = '\0';
				if(m_pClient->Client()->DummyConnected())
				{
					str_format(aOwnName, sizeof(aOwnName), " %s", m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName);
					if(str_endswith(aVictim, aOwnName))
						aVictim[str_length(aVictim) - str_length(aOwnName)] = '\0';
				}

				// cut off descriptions like this
				// why do you block foo he is new here!
				// why do you block foo she is my friend!!
				for(int i = 0; i < str_length(aVictim); i++)
				{
					// c++ be like...
					if(i < 2)
						continue;
					if(aVictim[i - 1] != ' ')
						continue;
					if((aVictim[i] != 'h' || !aVictim[i + 1] || aVictim[i + 1] != 'e' || !aVictim[i + 2] || aVictim[i + 2] != ' ') &&
						(aVictim[i] != 's' || !aVictim[i + 1] || aVictim[i + 1] != 'h' || !aVictim[i + 2] || aVictim[i + 2] != 'e' || !aVictim[i + 3] || aVictim[i + 3] != ' '))
						continue;

					aVictim[i - 1] = '\0';
					break;
				}

				// do not kill my friend foo
				const char *pFriend = NULL;
				if((pFriend = str_find_nocase(aVictim, " friend ")))
					pFriend += str_length(" friend ");
				else if((pFriend = str_find_nocase(aVictim, " frint ")))
					pFriend += str_length(" frint ");
				else if((pFriend = str_find_nocase(aVictim, " mate ")))
					pFriend += str_length(" mate ");
				else if((pFriend = str_find_nocase(aVictim, " bff ")))
					pFriend += str_length(" bff ");
				else if((pFriend = str_find_nocase(aVictim, " girlfriend ")))
					pFriend += str_length(" girlfriend ");
				else if((pFriend = str_find_nocase(aVictim, " boyfriend ")))
					pFriend += str_length(" boyfriend ");
				else if((pFriend = str_find_nocase(aVictim, " dog ")))
					pFriend += str_length(" dog ");
				else if((pFriend = str_find_nocase(aVictim, " gf ")))
					pFriend += str_length(" gf ");
				else if((pFriend = str_find_nocase(aVictim, " bf ")))
					pFriend += str_length(" bf ");

				if(pFriend)
					str_copy(aVictim, pFriend, sizeof(aVictim));
			}

			char aWarReason[128];
			if(HasWar || m_pClient->m_WarList.IsWarlist(aVictim) || m_pClient->m_WarList.IsTraitorlist(aVictim))
			{
				m_pClient->m_WarList.GetWarReason(aVictim, aWarReason, sizeof(aWarReason));
				if(aWarReason[0])
					str_format(pResponse, SizeOfResponse, "%s: %s has war because: %s", pMessageAuthor, aVictim, aWarReason);
				else
					str_format(pResponse, SizeOfResponse, "%s: the name %s is on my warlist.", pMessageAuthor, aVictim);
				return true;
			}
			for(auto &Client : m_pClient->m_aClients)
			{
				if(!Client.m_Active)
					continue;
				if(str_comp(Client.m_aName, aVictim))
					continue;

				if(m_pClient->m_WarList.IsWarClanlist(Client.m_aClan))
				{
					str_format(pResponse, SizeOfResponse, "%s i war %s because his clan %s is on my warlist.", pMessageAuthor, aVictim, Client.m_aClan);
					return true;
				}
			}
		}
	}
	// why? (check war for self)
	if(m_LangParser.IsQuestionWhy(pMessage) || (str_find(pMessage, "?") && MsgLen < NameLen + 4) ||
		((str_find(pMessage, "stop") || str_find_nocase(pMessage, "help")) && (m_pClient->m_WarList.IsWarlist(pMessageAuthor) || m_pClient->m_WarList.IsTraitorlist(pMessageAuthor))))
	{
		char aWarReason[128];
		if(m_pClient->m_WarList.IsWarlist(pMessageAuthor) || m_pClient->m_WarList.IsTraitorlist(pMessageAuthor))
		{
			m_pClient->m_WarList.GetWarReason(pMessageAuthor, aWarReason, sizeof(aWarReason));
			if(aWarReason[0])
				str_format(pResponse, SizeOfResponse, "%s has war because: %s", pMessageAuthor, aWarReason);
			else
				str_format(pResponse, SizeOfResponse, "%s you are on my warlist.", pMessageAuthor);
			return true;
		}
		else if(m_pClient->m_WarList.IsWarClanlist(m_aLastPingClan))
		{
			str_format(pResponse, SizeOfResponse, "%s your clan is on my warlist.", pMessageAuthor);
			return true;
		}
	}

	// spec me
	if(str_find_nocase(pMessage, "spec") || str_find_nocase(pMessage, "watch") || (str_find_nocase(pMessage, "look") && !str_find_nocase(pMessage, "looks")) || str_find_nocase(pMessage, "schau"))
	{
		str_format(pResponse, SizeOfResponse, "/pause %s", pMessageAuthor);
		str_format(pResponse, SizeOfResponse, "%s ok i am watching you", pMessageAuthor);
		return true;
	}
	// wanna? (always say no automated if motivated to do something type yes manually)
	if(str_find_nocase(pMessage, "wanna") || str_find_nocase(pMessage, "want"))
	{
		// TODO: fix tone
		// If you get asked to be given something "no sorry" sounds weird
		// If you are being asked to do something together "no thanks" sounds weird
		// the generic "no" might be a bit dry
		str_format(pResponse, SizeOfResponse, "%s no", pMessageAuthor);
		return true;
	}
	// help
	if(str_find_nocase(pMessage, "help") || str_find_nocase(pMessage, "hilfe"))
	{
		str_format(pResponse, SizeOfResponse, "%s where? what?", pMessageAuthor);
		return true;
	}
	// small talk
	if(str_find_nocase(pMessage, "how are you") ||
		str_find_nocase(pMessage, "how r u") ||
		str_find_nocase(pMessage, "how ru ") ||
		str_find_nocase(pMessage, "how ru?") ||
		str_find_nocase(pMessage, "how r you") ||
		str_find_nocase(pMessage, "how are u") ||
		str_find_nocase(pMessage, "how is it going") ||
		str_find_nocase(pMessage, "ca va") ||
		(str_find_nocase(pMessage, "как") && str_find_nocase(pMessage, "дела")))
	{
		str_format(pResponse, SizeOfResponse, "%s good, and you? :)", pMessageAuthor);
		return true;
	}
	if(str_find_nocase(pMessage, "wie gehts") || str_find_nocase(pMessage, "wie geht es") || str_find_nocase(pMessage, "was geht"))
	{
		str_format(pResponse, SizeOfResponse, "%s gut, und dir? :)", pMessageAuthor);
		return true;
	}
	if(str_find_nocase(pMessage, "about you") || str_find_nocase(pMessage, "and you") || str_find_nocase(pMessage, "and u") ||
		(str_find_nocase(pMessage, "u?") && MsgLen < NameLen + 5) ||
		(str_find_nocase(pMessage, "wbu") && MsgLen < NameLen + 8) ||
		(str_find_nocase(pMessage, "hbu") && MsgLen < NameLen + 8))
	{
		str_format(pResponse, SizeOfResponse, "%s good", pMessageAuthor);
		return true;
	}
	// advertise chillerbot
	if(str_find_nocase(pMessage, "what client") || str_find_nocase(pMessage, "which client") || str_find_nocase(pMessage, "wat client") ||
		str_find_nocase(pMessage, "good client") ||
		((str_find_nocase(pMessage, "ddnet") || str_find_nocase(pMessage, "vanilla")) && str_find_nocase(pMessage, "?")))
	{
		str_format(pResponse, SizeOfResponse, "%s I use chillerbot-ux ( https://chillerbot.github.io )", pMessageAuthor);
		return true;
	}
	// whats your setting (mousesense, distance, dyn)
	if((str_find_nocase(pMessage, "?") ||
		   str_find_nocase(pMessage, "what") ||
		   str_find_nocase(pMessage, "which") ||
		   str_find_nocase(pMessage, "wat") ||
		   str_find_nocase(pMessage, "much") ||
		   str_find_nocase(pMessage, "many") ||
		   str_find_nocase(pMessage, "viel") ||
		   str_find_nocase(pMessage, "hoch")) &&
		(str_find_nocase(pMessage, "sens") || str_find_nocase(pMessage, "sesn") || str_find_nocase(pMessage, "snse") || str_find_nocase(pMessage, "senes") || str_find_nocase(pMessage, "inp") || str_find_nocase(pMessage, "speed")))
	{
		str_format(pResponse, SizeOfResponse, "%s my current inp_mousesens is %d", pMessageAuthor, g_Config.m_InpMousesens);
		return true;
	}
	if((str_find_nocase(pMessage, "?") || str_find_nocase(pMessage, "what") || str_find_nocase(pMessage, "which") || str_find_nocase(pMessage, "wat") || str_find_nocase(pMessage, "much") || str_find_nocase(pMessage, "many")) &&
		str_find_nocase(pMessage, "dist"))
	{
		str_format(pResponse, SizeOfResponse, "%s my current cl_mouse_max_distance is %d", pMessageAuthor, g_Config.m_ClMouseMaxDistance);
		return true;
	}
	if((str_find_nocase(pMessage, "?") || str_find_nocase(pMessage, "do you") || str_find_nocase(pMessage, "do u")) &&
		str_find_nocase(pMessage, "dyn"))
	{
		str_format(pResponse, SizeOfResponse, "%s my dyncam is currently %s", pMessageAuthor, g_Config.m_ClDyncam ? "on" : "off");
		return true;
	}
	// compliments
	if(str_find_nocase(pMessage, "good") ||
		str_find_nocase(pMessage, "happy") ||
		str_find_nocase(pMessage, "congrats") ||
		str_find_nocase(pMessage, "nice") ||
		str_find_nocase(pMessage, "pro"))
	{
		str_format(pResponse, SizeOfResponse, "%s thanks", pMessageAuthor);
		return true;
	}
	// impatient
	if(str_find_nocase(pMessage, "answer") || str_find_nocase(pMessage, "ignore") || str_find_nocase(pMessage, "antwort") || str_find_nocase(pMessage, "ignorier"))
	{
		str_format(pResponse, SizeOfResponse, "%s i am currently busy (automated reply)", pMessageAuthor);
		return true;
	}
	// ask to ask
	if(m_LangParser.IsAskToAsk(pMessage, pMessageAuthor, pResponse, SizeOfResponse))
		return true;
	// got weapon?
	if(str_find_nocase(pMessage, "got") || str_find_nocase(pMessage, "have") || str_find_nocase(pMessage, "hast"))
	{
		int Weapon = -1;
		if(str_find_nocase(pMessage, "hammer"))
			Weapon = WEAPON_HAMMER;
		else if(str_find_nocase(pMessage, "gun"))
			Weapon = WEAPON_GUN;
		else if(str_find_nocase(pMessage, "sg") || str_find_nocase(pMessage, "shotgun") || str_find_nocase(pMessage, "shotty"))
			Weapon = WEAPON_SHOTGUN;
		else if(str_find_nocase(pMessage, "nade") || str_find_nocase(pMessage, "rocket") || str_find_nocase(pMessage, "bazooka"))
			Weapon = WEAPON_GRENADE;
		else if(str_find_nocase(pMessage, "rifle") || str_find_nocase(pMessage, "laser") || str_find_nocase(pMessage, "sniper"))
			Weapon = WEAPON_LASER;
		if(CCharacter *pChar = m_pClient->m_GameWorld.GetCharacterByID(m_pClient->m_LocalIDs[!g_Config.m_ClDummy]))
		{
			char aWeapons[1024];
			aWeapons[0] = '\0';
			if(pChar->GetWeaponGot(WEAPON_HAMMER))
				str_append(aWeapons, "hammer", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_GUN))
				str_append(aWeapons, aWeapons[0] ? ", gun" : "gun", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_SHOTGUN))
				str_append(aWeapons, aWeapons[0] ? ", shotgun" : "shotgun", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_GRENADE))
				str_append(aWeapons, aWeapons[0] ? ", grenade" : "grenade", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_LASER))
				str_append(aWeapons, aWeapons[0] ? ", rifle" : "rifle", sizeof(aWeapons));

			if(pChar->GetWeaponGot(Weapon))
				str_format(pResponse, SizeOfResponse, "%s Yes I got those weapons: %s", pMessageAuthor, aWeapons);
			else
				str_format(pResponse, SizeOfResponse, "%s No I got those weapons: %s", pMessageAuthor, aWeapons);
			return true;
		}
	}
	// weeb
	if(str_find_nocase(pMessage, "uwu"))
	{
		str_format(pResponse, SizeOfResponse, "%s OwO", pMessageAuthor);
		return true;
	}
	if(str_find_nocase(pMessage, "owo"))
	{
		str_format(pResponse, SizeOfResponse, "%s UwU", pMessageAuthor);
		return true;
	}
	// no u
	if(MsgLen < NameLen + 8 && (str_find_nocase(pMessage, "no u") ||
					   str_find_nocase(pMessage, "no you") ||
					   str_find_nocase(pMessage, "noob") ||
					   str_find_nocase(pMessage, "nob") ||
					   str_find_nocase(pMessage, "nuub") ||
					   str_find_nocase(pMessage, "nub") ||
					   str_find_nocase(pMessage, "bad")))
	{
		str_format(pResponse, SizeOfResponse, "%s no u", pMessageAuthor);
		return true;
	}
	return false;
}

void CChatHelper::DoGreet()
{
	if(m_aGreetName[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "hi %s", m_aGreetName);
		m_pClient->m_Chat.Say(0, aBuf);
		return;
	}
	m_pClient->m_Chat.Say(0, "hi");
}

void CChatHelper::Get128Name(const char *pMsg, char *pName)
{
	for(int i = 0; pMsg[i] && i < 17; i++)
	{
		if(pMsg[i] == ':' && pMsg[i + 1] == ' ')
		{
			str_copy(pName, pMsg, i + 1);
			return;
		}
	}
	str_copy(pName, " ", 2);
}

void CChatHelper::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	bool Highlighted = false;
	if(LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		Highlighted = true;
	if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		Highlighted = true;
	if(Team == 3) // whisper recv
		Highlighted = true;
	if(!Highlighted)
		return;
	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return;
	str_copy(m_aLastPingName, aName, sizeof(m_aLastPingName));
	str_copy(m_aLastPingClan, m_pClient->m_aClients[ClientID].m_aClan, sizeof(m_aLastPingClan));
	if(m_LangParser.IsGreeting(pMsg))
	{
		str_copy(m_aGreetName, aName, sizeof(m_aGreetName));
		m_NextGreetClear = time_get() + time_freq() * 10;
	}
	// ignore duplicated messages
	if(!str_comp(m_aLastPingMessage, pMsg))
		return;
	str_copy(m_aLastPingMessage, pMsg, sizeof(m_aLastPingMessage));
	m_NextPingMsgClear = time_get() + time_freq() * 60;
	int64_t AfkTill = m_pChillerBot->GetAfkTime();
	if(m_pChillerBot->IsAfk())
	{
		char aBuf[256];
		char aNote[128];
		str_format(aBuf, sizeof(aBuf), "%s: I am currently afk.", aName);
		if(AfkTill > time_get() + time_freq() * 61)
			str_format(aBuf, sizeof(aBuf), "%s: I am currently afk. Estimated return in %" PRId64 " minutes.", aName, (AfkTill - time_get()) / time_freq() / 60);
		else if(AfkTill > time_get() + time_freq() * 10)
			str_format(aBuf, sizeof(aBuf), "%s: I am currently afk. Estimated return in %" PRId64 " seconds.", aName, (AfkTill - time_get()) / time_freq());
		if(m_pChillerBot->GetAfkMessage()[0])
		{
			str_format(aNote, sizeof(aNote), " (%s)", m_pChillerBot->GetAfkMessage());
			str_append(aBuf, aNote, sizeof(aBuf));
		}
		if(aBuf[0] == '/' || aBuf[0] == '.' || aBuf[0] == '!')
		{
			char aEscape[256];
			str_format(aEscape, sizeof(aEscape), ".%s", aBuf);
			SayBuffer(aEscape, true);
		}
		else
		{
			SayBuffer(aBuf, true);
		}
		str_format(m_aLastAfkPing, sizeof(m_aLastAfkPing), "%s: %s", m_pClient->m_aClients[ClientID].m_aName, pMsg);
		m_pChillerBot->SetComponentNoteLong("afk", m_aLastAfkPing);
		return;
	}
	if(g_Config.m_ClAutoReply)
		SayFormat(g_Config.m_ClAutoReplyMsg);
}

void CChatHelper::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}

void CChatHelper::ListChatFilter()
{
	for(int i = 0; i < MAX_CHAT_FILTERS; i++)
	{
		if(m_aaChatFilter[i][0] == '\0')
			continue;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%d. '%s'", i, m_aaChatFilter[i]);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}

void CChatHelper::AddChatFilter(const char *pFilter)
{
	for(auto &aChatFilter : m_aaChatFilter)
	{
		if(aChatFilter[0] == '\0')
		{
			str_copy(aChatFilter, pFilter, sizeof(aChatFilter));
			return;
		}
	}
}

int CChatHelper::IsSpam(int ClientID, int Team, const char *pMsg)
{
	if(!g_Config.m_ClChatSpamFilter)
		return SPAM_NONE;
	int MsgLen = str_length(pMsg);
	int NameLen = 0;
	const char *pName = m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName;
	const char *pDummyName = m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName;
	bool Highlighted = false;
	if(LineShouldHighlight(pMsg, pName))
	{
		Highlighted = true;
		NameLen = str_length(pName);
	}
	else if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, pDummyName))
	{
		Highlighted = true;
		NameLen = str_length(pDummyName);
	}
	if(Team == 3) // whisper recv
		Highlighted = true;
	if(g_Config.m_ClChatSpamFilterInsults && m_LangParser.IsInsult(pMsg))
		return SPAM_INSULT;
	if(!Highlighted)
		return SPAM_NONE;
	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		MsgLen -= str_length(aName) + 2;
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return SPAM_NONE;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return SPAM_NONE;

	// ping without further context
	if(MsgLen < NameLen + 2)
		return SPAM_OTHER;
	else if(m_LangParser.IsAskToAsk(pMsg))
		return SPAM_OTHER;
	else if(!str_comp(aName, "nameless tee") || !str_comp(aName, "brainless tee") || str_find(aName, ")nameless tee") || str_find(aName, ")brainless te"))
		return SPAM_OTHER;
	else if(str_find(pMsg, "bro, check out this client: krxclient.pages.dev"))
		return SPAM_OTHER;
	else if((str_find(pMsg, "help") || str_find(pMsg, "hilfe")) && MsgLen < NameLen + 16)
		return SPAM_OTHER;
	else if((str_find(pMsg, "give") || str_find(pMsg, "need") || str_find(pMsg, "want") || str_find(pMsg, "please") || str_find(pMsg, "pls") || str_find(pMsg, "plz")) &&
		(str_find(pMsg, "rcon") || str_find(pMsg, "password") || str_find(pMsg, "admin") || str_find(pMsg, "helper") || str_find(pMsg, "mod") || str_find(pMsg, "money") || str_find(pMsg, "moni") || str_find(pMsg, "flag")))
		return SPAM_OTHER;
	return SPAM_NONE;
}

bool CChatHelper::FilterChat(int ClientID, int Team, const char *pLine)
{
	for(auto &aChatFilter : m_aaChatFilter)
	{
		if(aChatFilter[0] == '\0')
			continue;
		if(str_find(pLine, aChatFilter))
			return true;
	}
	int Spam = IsSpam(ClientID, Team, pLine);
	if(Spam)
	{
		if(g_Config.m_ClChatSpamFilter != 2)
			return true;
		if(Spam >= SPAM_INSULT)
			return true;

		// if not afk auto respond to pings
		if(!m_pChillerBot->IsAfk())
		{
			char aName[64];
			str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
			if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
			{
				Get128Name(pLine, aName);
				// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
			}
			char aResponse[1024];
			if(ReplyToLastPing(aName, pLine, aResponse, sizeof(aResponse)))
			{
				if(aResponse[0])
					SayBuffer(aResponse);
			}
			else
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s your message got spam filtered", aName);
				SayBuffer(aBuf);
			}
			m_aLastPingMessage[0] = '\0';
		}
		return true;
	}
	return false;
}

bool CChatHelper::OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB)
{
	if(pCompletionBuffer[0] != '.')
		return false;

	CCommand *pCompletionCommand = 0;

	const size_t NumCommands = m_Commands.size();

	if(ReverseTAB)
		*pCompletionChosen = (*pCompletionChosen - 1 + 2 * NumCommands) % (2 * NumCommands);
	else
		*pCompletionChosen = (*pCompletionChosen + 1) % (2 * NumCommands);

	const char *pCommandStart = pCompletionBuffer + 1;
	for(size_t i = 0; i < 2 * NumCommands; ++i)
	{
		int SearchType;
		int Index;

		if(ReverseTAB)
		{
			SearchType = ((*pCompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
			Index = (*pCompletionChosen - i + NumCommands) % NumCommands;
		}
		else
		{
			SearchType = ((*pCompletionChosen + i) % (2 * NumCommands)) / NumCommands;
			Index = (*pCompletionChosen + i) % NumCommands;
		}

		auto &Command = m_Commands[Index];

		if(str_comp_nocase_num(Command.pName, pCommandStart, str_length(pCommandStart)) == 0)
		{
			pCompletionCommand = &Command;
			*pCompletionChosen = Index + SearchType * NumCommands;
			break;
		}
	}
	if(!pCompletionCommand)
		return false;

	char aBuf[256];
	// add part before the name
	str_truncate(aBuf, sizeof(aBuf), pInput->GetString(), PlaceholderOffset);

	// add the command
	str_append(aBuf, ".", sizeof(aBuf));
	str_append(aBuf, pCompletionCommand->pName, sizeof(aBuf));

	// add separator
	const char *pSeparator = pCompletionCommand->pParams[0] == '\0' ? "" : " ";
	str_append(aBuf, pSeparator, sizeof(aBuf));
	if(*pSeparator)
		str_append(aBuf, pSeparator, sizeof(aBuf));

	// add part after the name
	str_append(aBuf, pInput->GetString() + PlaceholderOffset + PlaceholderLength, sizeof(aBuf));

	PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->pName) + 1;
	*pOldChatStringLength = pInput->GetLength();
	pInput->Set(aBuf); // TODO: Use Add instead
	pInput->SetCursorOffset(PlaceholderOffset + PlaceholderLength);
	return true;
}
