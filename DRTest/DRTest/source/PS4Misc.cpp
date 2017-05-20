// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
#include "CorePrivatePCH.h"
#include "PS4Application.h"
 
#if !UE_BUILD_SHIPPING
    #include <dbg_keyboard.h>
#endif
 
#include <libnetctl.h>
#include <system_service.h>
#include "PS4ChunkInstall.h"
 
#if (!defined SUPPORT_DEBUG_KEYBOARD) || SUPPORT_DEBUG_KEYBOARD
    #include <libsysmodule.h>
#endif
 
extern uint32 GGPUFrameTime;
extern bool GLargeCameraRotation;
 
void FPS4Misc::PlatformInit()
{
    // Identity.
    UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
    UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());
 
    // Get CPU info.
 
    // Timer resolution.
    UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}
 
 
void FPS4Misc::PlatformPostInit(bool ShowSplashScreen)
{
    // at this point in startup, the loading movie has just started, so now we can hide the splash screen.
    // @todo ps4: If the loading movie starts earlier, which it should, this will need to go much earlier!!
    sceSystemServiceHideSplashScreen();
}
 
 
GenericApplication* FPS4Misc::CreateApplication()
{
    return FPS4Application::CreatePS4Application();
}
 
 
uint32 FPS4Misc::GetKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
#define ADDKEYMAP(KeyCode, KeyName)     if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };
 
    uint32 NumMappings = 0;
 
    if (KeyCodes && KeyNames && (MaxMappings > 0))
    {
        // set up the basic printable character mapping
//      NumMappings += FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, false);
 
        ADDKEYMAP(0xA,  TEXT("Enter"));
        ADDKEYMAP(0x8,  TEXT("BackSpace"));
        ADDKEYMAP(0x1B, TEXT("Escape"));
        ADDKEYMAP('\t', TEXT("Tab"));
        ADDKEYMAP('~',  TEXT("Tilde"));
 
#if !UE_BUILD_SHIPPING
 
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_CAPS_LOCK,          TEXT("CapsLock"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F1,                 TEXT("F1"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F2,                 TEXT("F2"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F3,                 TEXT("F3"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F4,                 TEXT("F4"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F5,                 TEXT("F5"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F6,                 TEXT("F6"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F7,                 TEXT("F7"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F8,                 TEXT("F8"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F9,                 TEXT("F9"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F10,                TEXT("F10"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F11,                TEXT("F11"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F12,                TEXT("F12"));
//      ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_PRINTSCREEN,        TEXT("PrintScreen"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_SCROLL_LOCK,        TEXT("ScrollLock"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_PAUSE,              TEXT("Pause"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_INSERT,             TEXT("Insert"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_HOME,               TEXT("Home"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_PAGE_UP,            TEXT("PageUp"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_DELETE,             TEXT("Delete"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_END,                TEXT("End"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_PAGE_DOWN,          TEXT("PageDown"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_RIGHT_ARROW,        TEXT("Right"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_LEFT_ARROW,         TEXT("Left"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_DOWN_ARROW,         TEXT("Down"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_UP_ARROW,           TEXT("Up"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_NUM_LOCK,           TEXT("NumLock"));
//      ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_APPLICATION,        TEXT("Application"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_A,                  TEXT("A"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_B,                  TEXT("B"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_C,                  TEXT("C"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_D,                  TEXT("D"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_E,                  TEXT("E"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_F,                  TEXT("F"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_G,                  TEXT("G"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_H,                  TEXT("H"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_I,                  TEXT("I"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_J,                  TEXT("J"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_K,                  TEXT("K"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_L,                  TEXT("L"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_M,                  TEXT("M"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_N,                  TEXT("N"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_O,                  TEXT("O"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_P,                  TEXT("P"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_Q,                  TEXT("Q"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_R,                  TEXT("R"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_S,                  TEXT("S"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_T,                  TEXT("T"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_U,                  TEXT("U"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_V,                  TEXT("V"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_W,                  TEXT("W"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_X,                  TEXT("X"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_Y,                  TEXT("Y"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_Z,                  TEXT("Z"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_1,                  TEXT("One"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_2,                  TEXT("Two"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_3,                  TEXT("Three"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_4,                  TEXT("Four"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_5,                  TEXT("Five"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_6,                  TEXT("Six"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_7,                  TEXT("Seven"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_8,                  TEXT("Eight"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_9,                  TEXT("Nine"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_0,                  TEXT("Zero"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_ENTER,              TEXT("Enter"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_ESC,                TEXT("Escape"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_BS,                 TEXT("BackSpace"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_TAB,                TEXT("Tab"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_SPACE,              TEXT("SpaceBar"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_MINUS,              TEXT("Underscore"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_EQUAL_101,          TEXT("Equals"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_LEFT_BRACKET_101,   TEXT("LeftBracket"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_RIGHT_BRACKET_101,  TEXT("RightBracket"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_BACKSLASH_101,      TEXT("BackSlash"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_SEMICOLON,          TEXT("Semicolon"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_QUOTATION_101,      TEXT("Quote"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_COMMA,              TEXT("Comma"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_PERIOD,             TEXT("Period"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_SLASH,              TEXT("Slash"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_NUMLOCK,       TEXT("NumLock"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_SLASH,         TEXT("Divide"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_ASTERISK,      TEXT("Multiply"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_MINUS,         TEXT("Subtract"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_PLUS,          TEXT("Add"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_ENTER,         TEXT("Enter"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_1,             TEXT("NumPadOne"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_2,             TEXT("NumPadTwo"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_3,             TEXT("NumPadThree"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_4,             TEXT("NumPadFour"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_5,             TEXT("NumPadFive"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_6,             TEXT("NumPadSix"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_7,             TEXT("NumPadSeven"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_8,             TEXT("NumPadEight"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_9,             TEXT("NumPadNine"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_0,             TEXT("NumPadZero"));
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_KPAD_PERIOD,        TEXT("Decimal"));
 
        // this is the backquote/tilde key for whatever reason
        ADDKEYMAP(SCE_DBG_KEYBOARD_CODE_106_KANJI,          TEXT("Tilde"));
 
// #define SCE_DBG_KEYBOARD_CODE_NO_EVENT               0x00
// #define SCE_DBG_KEYBOARD_CODE_E_ROLLOVER         0x01
// #define SCE_DBG_KEYBOARD_CODE_E_POSTFAIL         0x02
// #define SCE_DBG_KEYBOARD_CODE_E_UNDEF                0x03
// #define SCE_DBG_KEYBOARD_CODE_ESCAPE             0x29
//#define SCE_DBG_KEYBOARD_CODE_KANA                        /* Katakana/Hiragana/Romaji key */
//#define SCE_DBG_KEYBOARD_CODE_HENKAN                  /* Conversion key */
//#define SCE_DBG_KEYBOARD_CODE_MUHENKAN                    /* no Conversion key */
//#define SCE_DBG_KEYBOARD_CODE_COLON_106                   /* : and * */
//#define SCE_DBG_KEYBOARD_CODE_BACKSLASH_106               
//#define SCE_DBG_KEYBOARD_CODE_YEN_106                     
//#define SCE_DBG_KEYBOARD_CODE_ACCENT_CIRCONFLEX_106       /* ^ and ~ */
//#define SCE_DBG_KEYBOARD_CODE_LEFT_BRACKET_106            /* [ */
//#define SCE_DBG_KEYBOARD_CODE_RIGHT_BRACKET_106           /* ] */
//#define SCE_DBG_KEYBOARD_CODE_ATMARK_106              /* @ */
 
#endif
    }
 
    return NumMappings;
}
 
 
IPlatformChunkInstall* FPS4Misc::GetPlatformChunkInstall()
{
    static FPS4ChunkInstall Singleton;
    return &Singleton;
}
 
 
bool FPS4Misc::SupportsMessaging()
{
    SceNetCtlInfo info;
    int32 err = sceNetCtlGetInfo(4, &info);
//  err = sceNetCtlGetInfo(1, &info);
//  err = sceNetCtlGetInfo(2, &info);
 
    return info.link == 1;
}
 
 
FString FPS4Misc::GetDefaultLocale()
{
    int32 SystemLanguage = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
    int32 Ret = sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_LANG, &SystemLanguage);
    checkf(Ret == SCE_OK, TEXT("sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_LANG) failed: 0x%x"), Ret);
 
    switch (SystemLanguage)
    {
        case SCE_SYSTEM_PARAM_LANG_JAPANESE:
            return FString(TEXT("ja"));
        case SCE_SYSTEM_PARAM_LANG_FRENCH:
            return FString(TEXT("fr"));
        case SCE_SYSTEM_PARAM_LANG_SPANISH:
            return FString(TEXT("es"));
        case SCE_SYSTEM_PARAM_LANG_GERMAN:
            return FString(TEXT("de"));
        case SCE_SYSTEM_PARAM_LANG_ITALIAN:
            return FString(TEXT("it"));
        case SCE_SYSTEM_PARAM_LANG_DUTCH:
            return FString(TEXT("nl"));
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
            return FString(TEXT("pt"));
        case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
            return FString(TEXT("ru"));
        case SCE_SYSTEM_PARAM_LANG_KOREAN:
            return FString(TEXT("ko"));
        case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
            return FString(TEXT("zh-Hant"));
        case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
            return FString(TEXT("zh-Hans"));
        case SCE_SYSTEM_PARAM_LANG_FINNISH:
            return FString(TEXT("fi"));
        case SCE_SYSTEM_PARAM_LANG_SWEDISH:
            return FString(TEXT("sv"));
        case SCE_SYSTEM_PARAM_LANG_DANISH:
            return FString(TEXT("da"));
        case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:
            return FString(TEXT("no"));
        case SCE_SYSTEM_PARAM_LANG_POLISH:
            return FString(TEXT("pl"));
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
            return FString(TEXT("pt-BR"));
        case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:
            return FString(TEXT("en-GB"));
        case SCE_SYSTEM_PARAM_LANG_TURKISH:
            return FString(TEXT("tr"));
        case SCE_SYSTEM_PARAM_LANG_SPANISH_LA:
            return FString(TEXT("es-LA"));
        default:
        case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
            return FString(TEXT("en-US"));
            break;
    }
}
 
 
bool FPS4Misc::IsRunningOnDevKit()
{
    // Hack! We can only use the debug keyboard on devkits in development mode so if we can't open it then we must be
    // running on a test kit or a devkit in non-development mode
    // @TODO: Find a better way of detecting the mode that the devkit is on
 
    static bool bIsInitialized = false;
    static bool bIsDevKit = false;
 
#if (!defined SUPPORT_DEBUG_KEYBOARD) || SUPPORT_DEBUG_KEYBOARD
 
    if( bIsInitialized == false )
    {
        int32 Result = sceSysmoduleLoadModule( SCE_SYSMODULE_DEBUG_KEYBOARD );
        if( Result == SCE_OK )
        {
            bIsDevKit = true;
        }
 
        bIsInitialized = true;
    }
#endif
    return bIsDevKit;
}
 
//make these lowercase on PS4 to be consistent with staged and fileserved paths.
const TCHAR* FPS4Misc::RootDir()
{
    static FString RootDirectory = TEXT("");
    if (RootDirectory.Len() == 0)
    {
        RootDirectory = FGenericPlatformMisc::RootDir();
        RootDirectory = RootDirectory.ToLower();
    }
    return *RootDirectory;
}
 
const TCHAR* FPS4Misc::EngineDir()
{
    static FString EngineDirectory = TEXT("");
    if (EngineDirectory.Len() == 0)
    {
        EngineDirectory = FGenericPlatformMisc::EngineDir();
        EngineDirectory = EngineDirectory.ToLower();
    }
    return *EngineDirectory;
}
 
const TCHAR* FPS4Misc::GameDir()
{
    static FString GameDirectory = TEXT("");
    if (GameDirectory.Len() == 0)
    {
        GameDirectory = FGenericPlatformMisc::GameDir();
        GameDirectory = GameDirectory.ToLower();
    }
    return *GameDirectory;
}
 
 
void FPS4Misc::BeginNamedEvent(const struct FColor& Color,const TCHAR* Text)
{
#if PS4_PROFILING_ENABLED
    if (IsRunningOnDevKit() && sceRazorCpuIsCapturing())
    {      
        sceRazorCpuPushMarker(TCHAR_TO_ANSI(Text), SCE_RAZOR_COLOR_RED , SCE_RAZOR_MARKER_ENABLE_HUD);
    }
#endif
}
 
 
void FPS4Misc::BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text)
{
#if PS4_PROFILING_ENABLED
    if (IsRunningOnDevKit() && sceRazorCpuIsCapturing())
    {      
        sceRazorCpuPushMarker(Text, SCE_RAZOR_COLOR_RED , SCE_RAZOR_MARKER_ENABLE_HUD);
    }
#endif
}
 
void FPS4Misc::BeginNamedEventEx(const struct FColor& Color,const ANSICHAR* Text)
{
#if PS4_PROFILING_ENABLED
    if (sceRazorCpuIsCapturing())
    {      
        sceRazorCpuPushMarker(Text, Color.DWColor() , SCE_RAZOR_MARKER_ENABLE_HUD);
    }
#endif
}
 
void FPS4Misc::EndNamedEvent()
{
#if PS4_PROFILING_ENABLED
    if (IsRunningOnDevKit() && sceRazorCpuIsCapturing())
    {      
        sceRazorCpuPopMarker();
    }
#endif
}
 
void FPS4Misc::MemoryBarrier()
{
    // x86 guarantees all but StoreLoad barriers normally, but it's not clear that we need a full mfence here.  Define this in the C++
    // with FORCENOINLINE to make sure the call with external linkage keeps the 'compiler' from reordering writes. 
}
 
#define IDEAL_FRAME 33366.7
 
extern bool GUseCustomFixedResolution; // @Virtuos [Ding Yifei]
 
// @Virtuos [Ding Yifei]: Introduce to indicate whether game is paused
#if VIRTUOS_PAIR_UE3UE4
namespace UE3
{
    extern ::uint32 GUE3IsPaused;
}
#endif
 
template<typename T, typename U>
void adjust_and_clamp(T& Val, const T Min, const T Max, const U Adj )
{
    //assert( Val >= Min );
    //assert( Val <= Max );
 
    if (Val + Adj > Max )
        Val = Max;
    else if( Val + Adj < Min )
        Val = Min;
    else
        Val += Adj;
}
 
bool FPS4Misc::DynamicResolution(float* outPercent)
{  
    // @Virtuos [Ding Yifei] BEGIN:
    // Stop dynamic resolution when game is paused, fixing BMBUG-16105
    bool bStopDynamicResolution = false;
#if VIRTUOS_PAIR_UE3UE4
    bStopDynamicResolution = (UE3::GUE3IsPaused == 1);
#endif
    if (bStopDynamicResolution)
    {
        return false;
    }
 
    // Use custom fixed resolution to avoid abrupt reflection refresh on glass and mirror in chapter one
    if (GUseCustomFixedResolution)
    {
        *outPercent = (1536.0f / 1920.0f) * 100.0f;
        return true;
    }
    // @Virtuos END
     
    bool result = false;
 
    if(GGPUFrameTime > 0)
    {
        float fscreenWidth      = 1920.0f * (*outPercent/100.0f);
         
        double dFrameTime       = (double)GGPUFrameTime;
        double PercentGPUUsed   = (dFrameTime/IDEAL_FRAME) * 100.0;
 
        // apply simple dynamic changes to main scene view port
        double Used = PercentGPUUsed;
        static double Delta = 0.0;
        static int State = 0;
        if (Used > 99.0)
        {
            if( State == -1 || State == 0 )
                Delta = (Used - 99.0) * 3.0;
            else
                Delta = Delta / 2;
            adjust_and_clamp(fscreenWidth, 1000.0f, 1920.0f, -1.0f * (float)int( Delta + 0.5 ));
            State = -1;
            float newPercent = (fscreenWidth/1920.0f) * 100.0f;
            if(*outPercent - newPercent >= 2.0f && GLargeCameraRotation)
            {
                *outPercent = newPercent;
                result = true;
            }
        }
        else if (Used < 95.0)
        {
            if( State == 1 || State == 0 )
                Delta = (95.0 - Used) * 3.0;
            else
                Delta /= 2;
            adjust_and_clamp(fscreenWidth, 1000.0f, 1920.0f, (float) int( Delta + 0.5 ));
            State = 1;
            float newPercent = (fscreenWidth/1920.0f) * 100.0f;
            if(newPercent - *outPercent >= 2.0f && GLargeCameraRotation)
            {
                *outPercent = newPercent;
                result = true;
            }
        }
        else
            State = 0;
    }
 
    return result;
}