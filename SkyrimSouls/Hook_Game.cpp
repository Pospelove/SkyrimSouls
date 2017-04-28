#include "Hook_Game.h"
#include "Settings.h"
#include "Tools.h"
#include <SKSE/GameMenus.h>
#include <SKSE/SafeWrite.h>
#include <SKSE/GameRTTI.h>
#include <SKSE/PluginAPI.h>
#include <Skyrim/BSDevices/InputDevice.h>
#include <Skyrim/BSDevices/InputManager.h>
#include <Skyrim/BSDevices/KeyCode.h>
#include <Skyrim/Forms/BGSEquipSlot.h>
#include <Skyrim/Forms/PlayerCharacter.h>
#include <Skyrim/BSDevices/InputMappingManager.h>
#include <Skyrim/BSDevices/MenuControls.h>
#include <Skyrim/Camera/PlayerCamera.h>
#include <SKyrim/BSDevices/PlayerControls.h>
#include <Skyrim/NetImmerse/NiAVObject.h>
#include <Skyrim/Forms/Character/Components/ActorProcessManager.h>
#include <Skyrim/Animation/IAnimationGraphManagerHolder.h>
#include <Skyrim/Menus/Inventory3DManager.h>
#include <Skyrim/FileIO/BGSSaveLoadManager.h>

#include <vector>
#include <string>
#include <string>
#include <algorithm>

#define STDFN __stdcall


class MenuOpenCloseEventHandler : public BSTEventSink<MenuOpenCloseEvent>
{
public:
	static SInt32							unpausedCount;
	//127248C
	virtual EventResult ReceiveEvent(MenuOpenCloseEvent *evn, BSTEventSource<MenuOpenCloseEvent> *src) override
	{
		auto it = settings.m_menuConfig.find(std::string(evn->menuName.c_str()));
		if (it != settings.m_menuConfig.end())
		{
			MenuManager* mm = MenuManager::GetSingleton();
			UIStringHolder* holder = UIStringHolder::GetSingleton();

			if (evn->opening)
			{
				MenuControls* menuControls = MenuControls::GetSingleton();
				*((bool*)(menuControls->unk28) + 0x25) = true;

				InputMappingManager* input = InputMappingManager::GetSingleton();
				if (evn->menuName == holder->containerMenu 
					|| evn->menuName == holder->barterMenu 
					|| evn->menuName == holder->giftMenu 
					|| evn->menuName == holder->inventoryMenu 
					|| evn->menuName == holder->magicMenu)
				{
					input->DisableControl(InputMappingManager::ContextType::kContext_Gameplay);
				}
				if (unpausedCount++ == 0)
				{
					if (mm->IsMenuOpen(holder->dialogueMenu) && (evn->menuName == holder->containerMenu 
						|| evn->menuName == holder->barterMenu 
						|| evn->menuName == holder->giftMenu 
						|| evn->menuName == holder->trainingMenu))
					{
						GFxMovieView* view = mm->GetMovieView(holder->dialogueMenu);
						view->SetVisible(false);
					}
					PlayerControls* control = PlayerControls::GetSingleton();
					for (auto element : control->handlers)
					{
						*((UInt32*)element + 1) = false;
					}
				}
			}
			else
			{
				InputMappingManager* input = InputMappingManager::GetSingleton();
				if (evn->menuName == holder->containerMenu 
					|| evn->menuName == holder->barterMenu 
					|| evn->menuName == holder->giftMenu 
					|| evn->menuName == holder->inventoryMenu 
					|| evn->menuName == holder->magicMenu)
				{
					input->EnableControl(InputMappingManager::ContextType::kContext_Gameplay);
				}
				if (--unpausedCount < 0)
					unpausedCount = 0;
				if (unpausedCount == 0)
				{
					PlayerControls* control = PlayerControls::GetSingleton();
					for (auto element : control->handlers)
					{
						*((UInt32*)element + 1) = true;
					}
					MenuControls* menuControls = MenuControls::GetSingleton();
					*((bool*)(menuControls->unk28) + 0x25) = false;

					if (mm->IsMenuOpen(holder->dialogueMenu))
					{
						GFxMovieView* view = mm->GetMovieView(holder->dialogueMenu);
						if (!view->GetVisible())
						{
							view->SetVisible(true);
						}
					}
				}
				if (evn->menuName == holder->console)
				{
					IMenu* console = mm->GetMenu(holder->console);
					if (console != nullptr)
					{
						if (settings.m_menuConfig[holder->console.c_str()])
							console->flags &= ~IMenu::kType_PauseGame;
						else
							console->flags |= IMenu::kType_PauseGame;
					}
				}
			}
		}
		return kEvent_Continue;
	}

	static MenuOpenCloseEventHandler* GetSingleton()
	{
		static MenuOpenCloseEventHandler instance;
		return &instance;
	}
};
SInt32		MenuOpenCloseEventHandler::unpausedCount = 0;


void* STDFN Hook_GetFavoritesItem(void* form)
{
	MenuManager* manager = MenuManager::GetSingleton();
	UIStringHolder* holder = UIStringHolder::GetSingleton();
	if (holder && manager && manager->IsMenuOpen(holder->favoritesMenu))
	{
		form = nullptr;
	}
	return form;
}

void STDFN Hook_KillActor(Actor* actor)
{
	PlayerCharacter* player = DYNAMIC_CAST<PlayerCharacter*>(actor);
	if (player != nullptr)
	{
		bool isProcessing = false;
		MenuManager* mm = MenuManager::GetSingleton();
		for (auto pair : settings.m_menuConfig)
		{
			BSFixedString menu(pair.first.c_str());
			if (mm->IsMenuOpen(menu))
			{
				mm->CloseMenu(menu);
				isProcessing = true;
			}
		}
		if (isProcessing) Sleep(200);
	}
}

bool STDFN Hook_IsInMenuMode()
{
	MenuManager* mm = MenuManager::GetSingleton();
	for (auto pair : settings.m_menuConfig)
	{
		BSFixedString menu(pair.first.c_str());
		if (mm->IsMenuOpen(menu))
			return true;
	}
	if (*(UInt8*)0x1B2E85F || *(UInt8*)0x1B2E85E)
		return true;
	return false;
}

bool STDFN Hook_AddUIMessage(const BSFixedString& strData, UInt32 msgID)
{
	UIStringHolder* holder = UIStringHolder::GetSingleton();
	if (msgID == UIMessage::kMessage_Open && MenuOpenCloseEventHandler::unpausedCount != 0 && (strData == holder->sleepWaitMenu || strData == holder->favoritesMenu|| strData == "Loot Menu"))
	{
		return false;
	}
	return true;
}


class FavoritesHandler : public MenuEventHandler
{
public:
	typedef bool(FavoritesHandler::*FnCanProcess)(InputEvent *);

	static FnCanProcess fnCanProcess;

	bool CanProcess_Hook(InputEvent *evn)
	{
		return (this->*fnCanProcess)(evn);
	}

	static void InitHook()
	{
		SafeWrite8(0x0085BE1D, 0x00);
		//fnCanProcess = SafeWrite32(0x010E4DFC + 0x01 * 4, &CanProcess_Hook);
	}
};

FavoritesHandler::FnCanProcess FavoritesHandler::fnCanProcess;


//BookMenu
//0x60
class BookMenu : public IMenu,
	public SimpleAnimationGraphManagerHolder,
	public BSTEventSink<BSAnimationGraphEvent>
{
public:

	class TurnPageUpdater : public TaskDelegate
	{
	public:
		TES_FORMHEAP_REDEFINE_NEW();

		TurnPageUpdater(bool direction) : m_direction(direction)
		{

		}

		virtual void Run() override
		{
			UIStringHolder* stringHolder = UIStringHolder::GetSingleton();
			MenuManager* mm = MenuManager::GetSingleton();
			if (mm->IsMenuOpen(stringHolder->bookMenu))
			{
				BookMenu* bookMenu = static_cast<BookMenu*>(mm->GetMenu(stringHolder->bookMenu));
				if (bookMenu != nullptr)
				{
					bookMenu->TurnPage(m_direction);
					if (mm->numPauseGame && (bookMenu->flags & IMenu::kType_PauseGame))
					{
						mm->numPauseGame -= 1;
						bookMenu->flags &= ~IMenu::kType_PauseGame;
					}
				}
			}
		}

		virtual void Dispose() override
		{
			delete this;
		}

		static void Queue(bool direction)
		{
			UIStringHolder* stringHolder = UIStringHolder::GetSingleton();
			MenuManager* mm = MenuManager::GetSingleton();

			if (mm->IsMenuOpen(stringHolder->bookMenu))
			{
				BookMenu* bookMenu = static_cast<BookMenu*>(mm->GetMenu(stringHolder->bookMenu));
				if (bookMenu != nullptr)
				{
					if (mm->numPauseGame)
					{
						bookMenu->TurnPage(direction);
					}
					else
					{
						mm->numPauseGame += 1;
						bookMenu->flags |= IMenu::kType_PauseGame;
						const SKSEPlugin *plugin = SKSEPlugin::GetSingleton();
						const SKSETaskInterface *task = plugin->GetInterface(SKSETaskInterface::Version_2);
						if (task)
						{
							TurnPageUpdater *delg = new TurnPageUpdater(direction);
							task->AddTask(delg);
						}
					}
				}
			}
		}

	private:
		bool						m_direction;
	};


	class GlobalBookData
	{
	public:
		ExtraTextDisplayData*			extraData;
		UInt32							unk04;
		TESObjectBOOK*					book;
		UInt32							padding;
		char*							content;
		UInt16							textLen;
		UInt16							buffLen;
		TESObjectREFR*					reference;
		float							unk1C;
		float							unk20;
		float							unk24;
		UInt32							unk28;

		static GlobalBookData* GetSingleton()
		{
			return (GlobalBookData*)0x01B3E520;
		}
	};

	typedef UInt32(BookMenu::*FnProcessMessage)(UIMessage*);
	typedef EventResult(BookMenu::*FnReceiveEvent)(BSAnimationGraphEvent*, BSTEventSource<BSAnimationGraphEvent>*);

	static FnProcessMessage fnProcessMessage;
	static FnReceiveEvent	fnReceiveEvent;
	static bool enableMenuControl;
	static bool enableCloseMenu;
	static bool bSlowndownRefreshFreq;

	EventResult	ReceiveEvent_Hook(BSAnimationGraphEvent * evn, BSTEventSource<BSAnimationGraphEvent> * source)
	{
		EventResult result = (this->*fnReceiveEvent)(evn, source);
		if (evn->animName == "PageBackStop" || evn->animName == "PageForwardStop" || evn->animName == "OpenStop")
		{
			bSlowndownRefreshFreq = false;
		}
		return result;
	}

	void TurnPage(bool direction)
	{
		bool animationVariable = false;
		this->GetAnimationVariableBool("bPageFlipping", animationVariable);//�Ƿ����ڷ�ҳ�С�
		if (!animationVariable)
		{
			GFxValue result;
			GFxValue argument;

			((void(__stdcall*)(GFxValue&, UInt32, UInt32, void*))0x00402290)(argument, 0x10, 1, (void*)0x0040F490);//����GFxValue����,����ֻ��Ҫһ���͹��ˡ�

			double turnPageNumber = (this->isNote) ? 1.00f : 2.00f;
			if (!direction)
				turnPageNumber = -turnPageNumber;

			argument.SetNumber(turnPageNumber);
			result.SetBoolean(false);

			bool(__fastcall* Invoke)(GFxMovieView*, void*, char*, GFxValue*, GFxValue*, UInt32) = (bool(__fastcall*)(GFxMovieView*, void*, char*, GFxValue*, GFxValue*, UInt32))(*(*(UInt32**)this->bookView + 0x17));
			if (Invoke(this->bookView, nullptr, "BookMenu.BookMenuInstance.TurnPage", &result, &argument, 1) && result.GetBool())
			{
				if (direction)
					this->SendAnimationEvent("PageForward");
				else
					this->SendAnimationEvent("PageBack");

				if (this->isNote)
				{
					((void(__cdecl*)(char*))0x00899620)("ITMNotePageTurn");
				}
			}
			((void(__stdcall*)(GFxValue&, UInt32, UInt32, void*))0x004022D0)(argument, 0x10, 1, (void*)0x00691680); //GFxValue���������������
		}
	}


	UInt32 ProcessMessage_Hook(UIMessage* msg)
	{
		UIStringHolder* holder = UIStringHolder::GetSingleton();

		if (settings.m_menuConfig[holder->bookMenu.c_str()])
		{
			InputStringHolder* input = InputStringHolder::GetSingleton();
			MenuManager* mm = MenuManager::GetSingleton();

			if (msg->type == UIMessage::kMessage_Open)
			{
				this->menuDepth = 4;
				bSlowndownRefreshFreq = true;
			}

			if (msg->type == UIMessage::kMessage_Refresh)
			{
				if (this->refreshTimes > 5 && !*(bool*)0x1B3E5D4)
				{
					float(__fastcall* sub_844D60)(void*, void*) = (float(__fastcall*)(void*, void*))0x00844D60; //update *(bool)0x1B3E5D4;
					sub_844D60((void*)0x1B3E580, nullptr);

					if (!*(bool*)0x1B3E5D4)
					{
						float(__fastcall* sub_844FD0)(void*) = (float(__fastcall*)(void*))0x00844FD0;
						float unk1 = sub_844FD0((void*)0x1B3E580);

						void(__fastcall* sub_420630)(void*, void*, float) = (void(__fastcall*)(void*, void*, float))0x00420630;
						sub_420630(this->unk3C, nullptr, unk1);

						struct
						{
							float unk0;
							float unk1;
							float unk2;
							float unk3;
						} unk2;

						void(__fastcall* sub_844F60)(void*, void*, void*) = (void(__fastcall*)(void*, void*, void*))0x00844F60;
						sub_844F60((void*)0x1B3E580, nullptr, &unk2);

						void(__fastcall* sub_4719A0)(void*, void*, void*) = (void(__fastcall*)(void*, void*, void*))0x004719A0;
						sub_4719A0(&unk2, nullptr, &(this->unk3C->m_localTransform));

						NiPoint3 unk3;
						void(__fastcall* sub_844ED0)(void*, void*, void*) = (void(__fastcall*)(void*, void*, void*))0x00844ED0;
						sub_844ED0((void*)0x1B3E580, nullptr, &unk3);

						if (this->isNote)
						{
							this->unk3C->m_localTransform.pos = unk3;
						}
						else
						{
							NiPoint3 unk4;
							NiPoint3*(__fastcall* sub_420360)(void*, void*, void*, void*) = (NiPoint3*(__fastcall*)(void*, void*, void*, void*))0x00420360;
							NiPoint3* unk5 = sub_420360(&this->unk3C->m_unkPoint, nullptr, &unk4, &this->unk3C->m_localTransform.pos);

							float(__fastcall* sub_844F80)(void*) = (float(__fastcall*)(void*))0x00844F80;
							float unk6 = sub_844F80((void*)0x1B3E580);

							NiPoint3 unk7;

							NiPoint3*(__cdecl* sub_420430)(void*, float, void*) = (NiPoint3*(__cdecl*)(void*, float, void*))0x00420430;
							NiPoint3* var5 = sub_420430(&unk7, unk6, unk5);

							NiPoint3 unk8;

							NiPoint3* unk9 = sub_420360(&unk3, nullptr, &unk8, var5);

							this->unk3C->m_localTransform.pos = *unk9;
						}

						void(__fastcall* sub_6504A0)(void*, void*, void*) = (void(__fastcall*)(void*, void*, void*))0x006504A0;
						sub_6504A0((char*)this + 0x1C, nullptr, *(void**)0x1B4ADE0);

						if (this->refreshTimes == 6)
						{
							if (holder && mm && mm->IsMenuOpen(holder->bookMenu) && (this->flags & IMenu::kType_PauseGame) == IMenu::kType_PauseGame && mm->numPauseGame)
							{
								mm->numPauseGame -= 1;
								this->flags &= ~IMenu::kType_PauseGame;
								if (!mm->numPauseGame)
								{
									static MenuModeChangeEvent event;
									event.unk0 = 0;
									event.menuMode = 0;

									mm->BSTEventSource<MenuModeChangeEvent>::SendEvent(&event);
									typedef void(__fastcall * Fn)(void*, void*); //MenuModeChangeEvent
									Fn fn = (Fn)0xA511B0;
									fn(&event, nullptr);
								}
							}
							this->refreshTimes += 1;
						}
					}

					if (bSlowndownRefreshFreq)
						Sleep(settings.m_waitTime);  //Slow down the refresh frequency, otherwise updating maybe cause CTD.

					static NiAVObject::ControllerUpdateContext ctx;
					ctx.delta = 0.0f;
					ctx.flags = 0;
					this->unk3C->UpdateNode(&ctx);

					return 0;
				}
			}

			if (msg->type == UIMessage::kMessage_Message)
			{
				BSUIMessageData* msgData = static_cast<BSUIMessageData*>(msg->data);

				if (msgData->unk0C == input->prevPage || msgData->unk0C == input->leftEquip)
				{
					TurnPageUpdater::Queue(false);
				}
				else if (msgData->unk0C == input->nextPage || msgData->unk0C == input->rightEquip)
				{
					TurnPageUpdater::Queue(true);
				}
				else if (msgData->unk0C == input->accept)
				{
					if (!*(bool*)0x1B3E5D4 || this->disableCloseMsg)
						return 0;
					GlobalBookData* data = GlobalBookData::GetSingleton();
					if (data->reference != nullptr)
					{
						g_thePlayer->PickUpItem(data->reference, 1, false, true);
						this->SendAnimationEvent("SoundPlay");
						this->SendAnimationEvent("CloseOut");
						mm->CloseMenu(holder->bookMenu);
					}
				}
				else if (msgData->unk0C == input->cancel)
				{
					if (!*(bool*)0x1B3E5D4 || this->disableCloseMsg)
						return 0;
					this->SendAnimationEvent("SoundPlay");
					this->SendAnimationEvent("CloseOut");
					mm->CloseMenu(holder->bookMenu);
				}
				return 1;
			}

			if (msg->type == UIMessage::kMessage_Scaleform)
			{
				if (!*(bool*)0x1B3E5D4)
					return 0;

				BSUIScaleformData* scaleformData = static_cast<BSUIScaleformData*>(msg->data);
				GFxEvent* event = scaleformData->event;

				if (event->type == GFxEvent::KeyDown)
				{
					GFxKeyEvent* key = static_cast<GFxKeyEvent*>(event);
					if (key->keyCode == GFxKey::Code::Left || key->keyCode == GFxKey::Code::Right)
					{
						TurnPageUpdater::Queue(key->keyCode == GFxKey::Code::Right);
					}
					else if (this->isNote && (key->keyCode == GFxKey::Code::Up || key->keyCode == GFxKey::Code::Down))
					{
						TurnPageUpdater::Queue(key->keyCode == GFxKey::Code::Down);
					}
				}
				return 1;
			}
			return (this->*fnProcessMessage)(msg);
		}
		else
		{
			return (this->*fnProcessMessage)(msg);
		}
	}

	static void InitHook()
	{
		fnProcessMessage = SafeWrite32(0x010E3AA4 + 0x04 * 4, &ProcessMessage_Hook);

		fnReceiveEvent = SafeWrite32(0x010E3A98 + 0x01 * 4, &ReceiveEvent_Hook);
	}

	//members:
	UInt32					unk2C;					// ini'd 0   char*  bookContent?
	UInt32					unk30;
	UInt32					unk34;
	GFxMovieView*			bookView;				// ini'd 0   "Book" swf name?   //__cdecl sub_926020(unk38, "BookMenu.BookMenuInstance.PrepForClose", 0);  //invoke???
	NiAVObject*				unk3C;					// ini'd 0   !=0 can process message.  //bookContent
	UInt32					unk40;					// ini'd 0
	UInt32					unk44;					// ini'd 0
	UInt32					unk48;					// ini'd 0
	UInt32					unk4C;					// ini'd 0
	NiSourceTexture*		unk50;					// ini'd 0
	NiTriShape*				unk54;					// ini'd 0
	UInt16					unk58;					// ini'd 0
	UInt16					refreshTimes;			// ini'd 0  0 ~ 6
	UInt8					disableCloseMsg;		// ini'd 0  ==1 can't process close msg. after process a close msg, this will be set to 1;
	bool					isNote;					// ini'd 0  0x5D
	bool					isInit;					// ini'd 0  bookConten initial?
	UInt8					padding;				// ini'd 0

	DEFINE_MEMBER_FN(InitBookMenu, void, 0x008451C0);
	//DEFINE_MEMBER_FN(CreateBookContent, void, 0x00845D60);
	DEFINE_MEMBER_FN(CreateBookContent, void, 0x00845F70);

};
static_assert(sizeof(BookMenu) == 0x60, "BookMenu");

BookMenu::FnProcessMessage	BookMenu::fnProcessMessage = nullptr;
BookMenu::FnReceiveEvent	BookMenu::fnReceiveEvent = nullptr;
bool						BookMenu::enableMenuControl = false;
bool						BookMenu::enableCloseMenu = false;
bool						BookMenu::bSlowndownRefreshFreq = false;



class LockpickingMenu : public IMenu
{
public:
	typedef UInt32(LockpickingMenu::*FnProcessMessage)(UIMessage*);

	static FnProcessMessage fnProcessMessage;


	UInt32 ProcessMessage_Hook(UIMessage* msg)
	{
		return (this->*fnProcessMessage)(msg);
	}

	static void STDFN Hook_SetLockInfo()
	{
		UIStringHolder* holder = UIStringHolder::GetSingleton();
		MenuManager* manager = MenuManager::GetSingleton();
		if (holder && manager && manager->IsMenuOpen(holder->lockpickingMenu))
		{
			IMenu* lockpickingMenu = manager->GetMenu(holder->lockpickingMenu);
			if (settings.m_menuConfig[holder->lockpickingMenu.c_str()] && (lockpickingMenu->flags & IMenu::kType_PauseGame) == IMenu::kType_PauseGame)
			{
				manager->numPauseGame -= 1;
				lockpickingMenu->flags &= ~IMenu::kType_PauseGame;
				if (!manager->numPauseGame)
				{
					static MenuModeChangeEvent event;
					event.unk0 = 0;
					event.menuMode = 0;
					manager->BSTEventSource<MenuModeChangeEvent>::SendEvent(&event);

					typedef void(__fastcall * Fn)(void*, void*);
					Fn ReleaseObject = (Fn)0xA511B0;
					ReleaseObject(&event, nullptr);
				}
			}
		}
	}

	static void InitHook()
	{
		{
			static const UInt32 kHook_SetLockInfo_Call = 0x0086FE10;

			START_ASM(Hook_SetLockInfo, 0x0087053D, 0x00870549, 1);
				add esp, 0xC
				lea ecx, [esp + 0x38]
				call [kHook_SetLockInfo_Call]
				pushad
				call Hook_SetLockInfo
				popad
			END_ASM(Hook_SetLockInfo);
		}
	}
};
LockpickingMenu::FnProcessMessage	LockpickingMenu::fnProcessMessage = nullptr;



class DialogueMenuEx :public IMenu
{
public:
	typedef UInt32(DialogueMenuEx::*FnProcessMessage)(UIMessage*);

	static FnProcessMessage fnProcessMessage;

	UInt32 ProcessMessage_Hook(UIMessage* msg)
	{
		if (msg->type == UIMessage::kMessage_Open && MenuOpenCloseEventHandler::unpausedCount != 0)
		{
			this->menuDepth = 2;
			//GFxMovieView* view = this->GetMovieView();
		}
		return (this->*fnProcessMessage)(msg);
	}

	static void InitHook()
	{
		fnProcessMessage = SafeWrite32(0x010E4C9C + 0x04 * 4, &ProcessMessage_Hook);
	}
};
DialogueMenuEx::FnProcessMessage	DialogueMenuEx::fnProcessMessage = nullptr;



class InventoryMenuEx : public IMenu
{
public:
	typedef UInt32(InventoryMenuEx::*FnProcessMessage)(UIMessage*);

	static FnProcessMessage fnProcessMessage;

	void Release_Hook()
	{
		if (*(UInt32*)this == 0x010E5B90 && !(this->flags & IMenu::kType_PauseGame))
		{
			if (InterlockedExchangeAdd((volatile long*)((char*)this + 4), -1) == 1)
			{
				void(__fastcall* sub_897160)(void*, void*) = (void(__fastcall*)(void*, void*))0x00897160;//Camera mode.
				sub_897160(*(void**)0x1B2E4C0, nullptr);
				//  sub_897130(dword_1B2E4C0);

				void(__cdecl* sub_861470)(void*) = (void(__cdecl*)(void*))0x861470;  //Enable prompt.
				sub_861470((void*)0x10E3824);

				InputMappingManager* input = InputMappingManager::GetSingleton();
				input->DisableControl(InputMappingManager::ContextType::kContext_Inventory);
				input->DisableControl(InputMappingManager::ContextType::kContext_ItemMenu);
				input->DisableControl(InputMappingManager::ContextType::kContext_MenuMode);

				//A67260  Disable();
				//void(__fastcall* sub_42F240)(void*, void*, bool) = (void(__fastcall*)(void*, void*, bool))0x0042F240;  //???????????
				//sub_42F240(*(void**)0x12E3534, nullptr, 1);

				if (*((UInt32*)this + 0xF)) // Need test.
				{
					void(__fastcall* sub_A49D90)(void*, void*) = (void(__fastcall*)(void*, void*))0x00A49D90;
					sub_A49D90((char*)this + 0x3C, nullptr);
					*((UInt32*)this + 0x11) = 0;
				}
				//Another funciton here.
				GFxValue* root = (GFxValue*)((char*)this + 0x20);
				if ((*(UInt32*)((char*)this + 0x24) >> 6) & 1)
				{
					void(__fastcall* ObjectRelease)(void*, void*, void*, void*) = (void(__fastcall*)(void*, void*, void*, void*))0x922660;
					ObjectRelease(*(void**)root, nullptr, root, *((void**)this + 0xA));
					*(void**)root = nullptr;
				}

				typedef void(__fastcall* Fn)(void*, void*);
				Fn fn = (Fn)0x00A64A10;
				fn(this, nullptr);

				GMemory::Free(this);
			}
			return;
		}
		this->Release();
	}

	UInt32 ProcessMessage_Hook(UIMessage* msg)
	{
		UInt32 result = (this->*fnProcessMessage)(msg);

		if (msg->type == UIMessage::kMessage_Scaleform)
		{
			BSUIScaleformData* scaleformData = static_cast<BSUIScaleformData*>(msg->data);
			GFxEvent* event = scaleformData->event;
			if (event->type == GFxEvent::KeyDown)
			{
				GFxKeyEvent* key = static_cast<GFxKeyEvent*>(event);
				if (key->keyCode == GFxKey::Code::I)
				{
					GFxValue result;
					GFxMovieView* view = this->GetMovieView();
					if (view != nullptr)
					{
						view->Invoke("_root.Menu_mc.onExitMenuRectClick", &result, nullptr, 0);
					}
				}
			}
		}
		return result;
	}

	static void InitHook()
	{
		fnProcessMessage = SafeWrite32(0x010E5B90 + 0x04 * 4, &ProcessMessage_Hook);
		WriteRelCall(0x00A5D686, &Release_Hook);
	}
};
InventoryMenuEx::FnProcessMessage	InventoryMenuEx::fnProcessMessage = nullptr;



class MagicMenuEx : public IMenu
{
public:
	typedef UInt32(MagicMenuEx::*FnProcessMessage)(UIMessage*);

	static FnProcessMessage fnProcessMessage;

	UInt32 ProcessMessage_Hook(UIMessage* msg)
	{
		UInt32 result = (this->*fnProcessMessage)(msg);

		if (msg->type == UIMessage::kMessage_Scaleform)
		{
			BSUIScaleformData* scaleformData = static_cast<BSUIScaleformData*>(msg->data);
			GFxEvent* event = scaleformData->event;
			if (event->type == GFxEvent::KeyDown)
			{
				GFxKeyEvent* key = static_cast<GFxKeyEvent*>(event);
				if (key->keyCode == GFxKey::Code::P)
				{
					GFxValue result;
					GFxMovieView* view = this->GetMovieView();
					if (view != nullptr)
					{
						view->Invoke("_root.Menu_mc.onExitMenuRectClick", &result, nullptr, 0);
					}
				}
			}
		}
		return result;
	}

	static void InitHook()
	{
		fnProcessMessage = SafeWrite32(0x010E6594 + 0x04 * 4, &ProcessMessage_Hook);

		SafeWrite16(0x00873884, 0x9090);
		SafeWrite32(0x00873886, 0x90909090);
		SafeWrite8(0x0087388A, 0x90);
	}
};
MagicMenuEx::FnProcessMessage	MagicMenuEx::fnProcessMessage = nullptr;



class SleepWaitMenu : public IMenu
{
public:
	typedef UInt32(SleepWaitMenu::*FnProcessMessage)(UIMessage*);

	static FnProcessMessage fnProcessMessage;

	UInt32 ProcessMessage_Hook(UIMessage* msg)
	{
		UInt32 result = (this->*fnProcessMessage)(msg);

		if (msg->type == UIMessage::kMessage_Open)
		{
			MenuManager* mm = MenuManager::GetSingleton();
			UIStringHolder* holder = UIStringHolder::GetSingleton();

			if (holder && mm && mm->IsMenuOpen(holder->sleepWaitMenu) && (this->flags & IMenu::kType_PauseGame) && mm->numPauseGame)
			{
				mm->numPauseGame -= 1;
				this->flags &= ~IMenu::kType_PauseGame;
				if (!mm->numPauseGame)
				{
					static MenuModeChangeEvent event;
					event.unk0 = 0;
					event.menuMode = 0;
					mm->BSTEventSource<MenuModeChangeEvent>::SendEvent(&event);

					typedef void(__fastcall * Fn)(void*, void*);
					Fn ReleaseObject = (Fn)0xA511B0;
					ReleaseObject(&event, nullptr);
				}
			}
		}
		return result;
	}

	static void InitHook()
	{
		fnProcessMessage = SafeWrite32(0x010E7754 + 0x04 * 4, &ProcessMessage_Hook);
	}
};
SleepWaitMenu::FnProcessMessage	SleepWaitMenu::fnProcessMessage = nullptr;



void UICallBack_DropItem(FxDelegateArgs* pargs)
{
	InventoryMenu* inventory = reinterpret_cast<InventoryMenu*>(pargs->pThisMenu);
	if (inventory != nullptr && inventory->inventoryData)
	{
		StandardItemData* itemData = inventory->inventoryData->GetSelectedItemData();
		InventoryEntryData* objDesc = nullptr;
		if (itemData)
			objDesc = itemData->objDesc;
		if (objDesc != nullptr)
		{
			void(__cdecl* sub_8997A0)(void*, char*, bool) = (void(__cdecl*)(void*, char*, bool))0x8997A0;
			if (objDesc->IsQuestItem())
			{
				sub_8997A0(*(void**)0x1B1950C, nullptr, true);  //cdecl
			}
			else
			{
				TESForm* form = objDesc->baseForm;
				if (form->formType == FormType::Key)
				{
					sub_8997A0(*(void**)0x1B1953C, nullptr, true);  //cdecl
				}
				else
				{
					GFxValue* args = pargs->args;
					UInt32 count = static_cast<UInt32>(args->GetNumber());
					BaseExtraList* (__cdecl* GetBaseExtraList)(InventoryEntryData*, UInt32, bool) = (BaseExtraList* (__cdecl*)(InventoryEntryData*, UInt32, bool))0x00868950;
					BaseExtraList* extraList = GetBaseExtraList(objDesc, count, false);
					RefHandle handle;
					g_thePlayer->DropItem(&handle, form, extraList, count, 0, 0);

					UInt32(__fastcall * sub_755420)(void*, void*) = (UInt32(__fastcall *)(void*, void*))0x00755420;
					sub_755420(*(void**)0x12E32E8, nullptr);

					inventory->inventoryData->Update(g_thePlayer);

					if (inventory->flags & IMenu::kType_PauseGame)
					{
						g_thePlayer->processManager->UpdateEquipment(g_thePlayer);
					}
				}
			}
		}
	}
}


void UICallBack_CloseTweenMenu(FxDelegateArgs* pargs)
{
	PlayerCamera* camera = PlayerCamera::GetSingleton();
	camera->ResetCamera();

	MenuManager* mm = MenuManager::GetSingleton();
	UIStringHolder* holder = UIStringHolder::GetSingleton();
	mm->CloseMenu(holder->tweenMenu);

	IMenu* inventoryMenu = reinterpret_cast<IMenu*>(pargs->pThisMenu);
	*((bool*)inventoryMenu + 0x51) = 1;
}


class PlayerInfoUpdater : public UIDelegate
{
public:
	TES_FORMHEAP_REDEFINE_NEW();

	void Run() override
	{
		UIStringHolder* stringHolder = UIStringHolder::GetSingleton();
		MenuManager* mm = MenuManager::GetSingleton();
		if (mm->IsMenuOpen(stringHolder->inventoryMenu))
		{
			InventoryMenu* inventory = static_cast<InventoryMenu*>(mm->GetMenu(stringHolder->inventoryMenu));
			inventory->UpdatePlayerInfo();
		}
	}

	void Dispose() override
	{
		delete this;
	}

	static void Register()
	{
		const SKSEPlugin *plugin = SKSEPlugin::GetSingleton();
		const SKSETaskInterface *task = plugin->GetInterface(SKSETaskInterface::Version_2);
		if (task)
		{
			PlayerInfoUpdater *delg = new PlayerInfoUpdater();
			task->AddUITask(delg);
		}
	}
};


void UICallBack_SelectItem(FxDelegateArgs* pargs)
{
	InventoryMenu* inventory = reinterpret_cast<InventoryMenu*>(pargs->pThisMenu);
	GFxValue* args = pargs->args;

	TESForm* form = nullptr;
	BaseExtraList* extraList = nullptr;

	void(__fastcall* EquipItem)(InventoryMenu*, void*, BGSEquipSlot*) = (void(__fastcall*)(InventoryMenu*, void*, BGSEquipSlot*))0x00869DB0;
	if (inventory != nullptr && inventory->bPCControlsReady && pargs->numArgs && args->Type == GFxValue::ValueType::VT_Number)
	{
		if (args->GetNumber() == 0.0f)
		{
			BGSEquipSlot* rightHandSlot = GetRightHandSlot();
			EquipItem(inventory, nullptr, rightHandSlot);
		}
		else if (args->GetNumber() == 1.0f)
		{
			BGSEquipSlot* leftHandSlot = GetLeftHandSlot();
			EquipItem(inventory, nullptr, leftHandSlot);
		}
	}
	else
	{
		EquipItem(inventory, nullptr, nullptr);
	}
	if (inventory->flags & IMenu::kType_PauseGame)
	{
		g_thePlayer->processManager->UpdateEquipment(g_thePlayer);
	}
	auto fn = [=](UInt32 time)->bool {std::this_thread::sleep_for(std::chrono::milliseconds(time)); PlayerInfoUpdater::Register(); return true; };
	really_async(fn, 100);
}


void UICallBack_SetSaveDisabled(FxDelegateArgs* pargs)
{
	GFxValue* args = pargs->args; //һ���������� GFxVaue[5];

	bool isBool = true;
	if (pargs->numArgs <= 5 || args[5].Type != GFxValue::ValueType::VT_Boolean || !args[5].GetBool())
	{
		isBool = false;
	}
	for (UInt32 i = 0; i < 5 && args[i].GetType() != GFxValue::ValueType::VT_Undefined; ++i)
	{
		GFxValue value(args[i]);
		BGSSaveLoadManager* saveLoadManager = BGSSaveLoadManager::GetSingleton();
		GFxValue arg;
		if (i == 1) //Load
		{
			arg.SetBoolean(false);
		}
		else
		{
			arg.SetBoolean(!saveLoadManager->CanSaveLoadGame(!isBool));
		}
		args[i].pObjectInterface->SetMember(args[i].Value.pData, "disabled", arg, value.GetType() == GFxValue::ValueType::VT_DisplayObject);
		//_MESSAGE("pGFxValue: %p    pObjectInterface: %p    pData:%p", &args[i], args[i].pObjectInterface, args[i].Value.pData);
		if ((arg.GetType() >> 6) & 1)
		{
			arg.pObjectInterface->ObjectRelease(&arg, arg.Value.pData);
		}
		if ((value.GetType() >> 6) & 1)
		{
			value.pObjectInterface->ObjectRelease(&value, value.Value.pData);
		}
	}
}


void RegisterEventHandler()
{
	MenuManager* mm = MenuManager::GetSingleton();
	if (mm)
	{
		BSTEventSource<MenuOpenCloseEvent>* eventDispatcher = mm->GetMenuOpenCloseEventSource();
		eventDispatcher->AddEventSink(MenuOpenCloseEventHandler::GetSingleton());
	}
}


void Hook_Game_Commit()
{

	FavoritesHandler::InitHook();
	BookMenu::InitHook();
	LockpickingMenu::InitHook();
	InventoryMenuEx::InitHook();

	DialogueMenuEx::InitHook();
	MagicMenuEx::InitHook();
	//SleepWaitMenu::InitHook();

	//Fix Inventory.
	SafeWrite16(0x0086BF6F, 0x9090);
	SafeWrite32(0x0086BF71, 0x90909090);

	//Fix Container
	SafeWrite16(0x0084C068, 0x9090);
	SafeWrite32(0x0084C06A, 0x90909090);

	SafeWrite16(0x0084AC57, 0x9090);
	SafeWrite32(0x0084AC59, 0x90909090);

	//Drop item in Inventory menu.
	SafeWrite32(0x0086B437, (UInt32)UICallBack_DropItem);
	//Equip item in Inventory Menu
	SafeWrite32(0x0086B3F4, (UInt32)UICallBack_SelectItem);
	//SetSaveDisabled in Journal Menu
	SafeWrite32(0x008A7F73, (UInt32)UICallBack_SetSaveDisabled);
	//Fix camera in Tweenmenu.
	SafeWrite8(0x008951C4, 0x90);
	SafeWrite32(0x008951C5, 0x90909090);

	//Redefine papyrus function: Utility.IsInMenuMode()
	WriteRelJump(0x00918D90, (UInt32)Hook_IsInMenuMode);

	{
		static const UInt32 kHook_GetFavoritesSpell_Jmp = 0x85BA03;

		START_ASM(Hook_GetFavoritesSpell, 0x0085B919, 0x0085B921, 1);
			push eax
			call Hook_GetFavoritesItem
			test eax, eax
			je InvalidSpell
			JMP_ASM(Hook_GetFavoritesSpell)
			InvalidSpell:
			jmp [kHook_GetFavoritesSpell_Jmp]
		END_ASM(Hook_GetFavoritesSpell);
	}

	{
		static const UInt32 kHook_GetFavoritesItem_Jmp = 0x85BB34;

		START_ASM(Hook_GetFavoritesItem, 0x0085BA52, 0x0085BA5A, 1);
			push eax
			call Hook_GetFavoritesItem
			test eax, eax
			je InvalidItem
			JMP_ASM(Hook_GetFavoritesItem)
			InvalidItem:
			jmp [kHook_GetFavoritesItem_Jmp]
		END_ASM(Hook_GetFavoritesItem);
	}

	{
		START_ASM(Hook_KillActor, 0x006AC3A0, 0x006AC3A9, 1);
			push ecx
			push ecx
			call Hook_KillActor
			pop ecx
			push esi
			mov esi, ecx
			mov ecx, [esi + 0x88]
		END_ASM(Hook_KillActor);
	}

	{
		START_ASM(Hook_AddUIMessage, 0x00431B00, 0x00431B0A, 1);
			push ecx
			mov eax, [esp + 0xC]
			push eax
			mov eax, [esp + 0xC]
			push eax
			call Hook_AddUIMessage
			pop ecx
			test al, al
			jnz Continue
			retn 0xC
			Continue:
			push ecx
			push esi
			mov esi, ecx
			mov eax, [esi + 0x1C8]
		END_ASM(Hook_AddUIMessage);
	}

	{
		START_ASM(Hook_RequstAutoSave, 0x00681770, 0x00681776, 1);
			push ecx
			mov eax, MenuOpenCloseEventHandler::unpausedCount
			cmp eax, 0
			jz	ContinueSave
			pop ecx
			retn
			ContinueSave:
			pop ecx
			sub esp, 0x104
		END_ASM(Hook_RequstAutoSave)
	}
}

