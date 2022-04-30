/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLCONSOLE_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLCONSOLE_H
#include <engine/shared/ringbuffer.h>
#include <game/client/component.h>
#include <game/client/components/console.h>
#include <game/client/lineinput.h>

enum
{
	CONSOLE_OPENING_2 = CONSOLE_CLOSING + 1,
	CONSOLE_OPEN_2,
	CONSOLE_READ_ONLY,
	CONSOLE_CLOSING_2,
};

class CChillConsole : public CComponent
{
	class CInstance
	{
	public:
		struct CBacklogEntry
		{
			float m_YOffset;
			ColorRGBA m_PrintColor;
			char m_aText[1];
		};
		CStaticRingBuffer<CBacklogEntry, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_Backlog;
		CStaticRingBuffer<char, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
		char *m_pHistoryEntry;

		CLineInput m_Input;
		int m_Type;
		int m_CompletionEnumerationCount;
		int m_BacklogActPage;

	public:
		CChillConsole *m_pGameConsole;

		char m_aCompletionBuffer[128];
		int m_CompletionChosen;
		int m_CompletionFlagmask;
		float m_CompletionRenderOffset;
		bool m_ReverseTAB;

		char m_aUser[32];
		bool m_UserGot;
		bool m_UsernameReq;

		bool m_IsCommand;
		char m_aCommandName[IConsole::TEMPCMD_NAME_LENGTH];
		char m_aCommandHelp[IConsole::TEMPCMD_HELP_LENGTH];
		char m_aCommandParams[IConsole::TEMPCMD_PARAMS_LENGTH];

		CInstance(int t);
		void Init(CChillConsole *pGameConsole);

		void ClearBacklog();
		void ClearHistory();

		void ExecuteLine(const char *pLine);

		void OnInput(IInput::CEvent Event);
		void PrintLine(const char *pLine, ColorRGBA PrintColor = {1, 1, 1, 1});

		const char *GetString() const { return m_Input.GetString(); }
		static void PossibleCommandsCompleteCallback(const char *pStr, void *pUser);
	};

	class IConsole *m_pConsole;

	CInstance m_LocalConsole;
	CInstance m_RemoteConsole;

	CInstance *CurrentConsole();
	float TimeNow();
	int m_PrintCBIndex;

	int m_ConsoleType;
	int m_ConsoleState;
	float m_StateChangeEnd;
	float m_StateChangeDuration;

	void Toggle(int Type);
	void Dump(int Type);

	static void PossibleCommandsRenderCallback(const char *pStr, void *pUser);
	static void ClientConsolePrintCallback(const char *pStr, void *pUserData, ColorRGBA PrintColor = {1, 1, 1, 1});
	static void ConToggleLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageUp(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageDown(IConsole::IResult *pResult, void *pUserData);

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	enum
	{
		CONSOLETYPE_LOCAL = 0,
		CONSOLETYPE_REMOTE,
	};

	CChillConsole();

	void PrintLine(int Type, const char *pLine);
	void RequireUsername(bool UsernameReq);

	virtual void OnStateChange(int NewState, int OldState) override;
	virtual void OnConsoleInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual bool OnInput(IInput::CEvent Events) override;

	bool IsClosed() { return m_ConsoleState == CONSOLE_CLOSED; }
};
#endif