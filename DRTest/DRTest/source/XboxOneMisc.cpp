// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include <time.h>
#include "XboxOneApplication.h"
#include "XboxOneChunkInstall.h"
 
#include "XBoxOneAllowPlatformTypes.h"
#include <pix.h>
#include <d3d11_x.h>
#include "XBoxOneHidePlatformTypes.h"
 
#define MAX_PIX_STRING_LENGTH 32
 
/** Whether support for integrating into the firewall is there. */
#define WITH_FIREWALL_SUPPORT   0
 
/** Original C- Runtime pure virtual call handler that is being called in the (highly likely) case of a double fault */
_purecall_handler DefaultPureCallHandler;
 
// @Virtuos [pengshu]: copied from WindowsPlatformMisc.cpp
char gLoadMapName[128] = "\0";
 
//@VIRTUOS jboldiga - For performing dynamic resolution when the camera is rotating (hides the change)
extern bool GLargeCameraRotation;
 
/**
 * Our own pure virtual function call handler, set by appPlatformPreInit. Falls back
 * to using the default C- Runtime handler in case of double faulting.
 */
static void PureCallHandler()
{
    static bool bHasAlreadyBeenCalled = false;
    FPlatformMisc::DebugBreak();
    if (bHasAlreadyBeenCalled)
    {
        // Call system handler if we're double faulting.
        if (DefaultPureCallHandler)
        {
            DefaultPureCallHandler();
        }
    }
    else
    {
        bHasAlreadyBeenCalled = true;
        if (GIsRunning)
        {
            FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Core", "PureVirtualFunctionCalledWhileRunningApp", "Pure virtual function being called while application was running (GIsRunning == 1)."));
        }
        UE_LOG(LogTemp, Fatal, TEXT("Pure virtual function being called"));
    }
}
 
/*-----------------------------------------------------------------------------
    SHA-1 functions.
-----------------------------------------------------------------------------*/
 
/**
 * Get the hash values out of the executable hash section
 *
 * NOTE: hash keys are stored in the executable, you will need to put a line like the
 *       following into your PCLaunch.rc settings:
 *          ID_HASHFILE RCDATA "../../../../GameName/Build/Hashes.sha"
 *
 *       Then, use the -sha option to the cooker (must be from commandline, not
 *       frontend) to generate the hashes for .ini, loc, startup packages, and .usf shader files
 *
 *       You probably will want to make and checkin an empty file called Hashses.sha
 *       into your source control to avoid linker warnings. Then for testing or final
 *       final build ONLY, use the -sha command and relink your executable to put the
 *       hashes for the current files into the executable.
 */
static void InitSHAHashes()
{
    //@todo.XBOXONE: Implement this using GetModuleSection?
}
 
void FXboxOneMisc::PlatformPreInit()
{
    FGenericPlatformMisc::PlatformPreInit();
 
    // Use our own handler for pure virtuals being called.
    DefaultPureCallHandler = _set_purecall_handler(PureCallHandler);
 
    // initialize the file SHA hash mapping
    InitSHAHashes();
}
 
void FXboxOneMisc::PlatformInit()
{
    // Identity.
    UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
    UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());
 
    // Timer resolution.
    UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}
 
GenericApplication* FXboxOneMisc::CreateApplication()
{
    return FXboxOneApplication::CreateXboxOneApplication();
}
 
void FXboxOneMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
    //@todo.XBOXONE:
    *Result = 0;
    ResultLength = 0;
}
 
// Defined in XboxOneLaunch.cpp
extern void appWinPumpMessages();
 
void FXboxOneMisc::PumpMessages(bool bFromMainLoop)
{
    //--------------------
    // Changed this function as we are receiving calls to it from the RenderThread, which is undesirable, so we ignore them
    //       This is mainly to catch the case where we have a PLM Suspend event to consume. In there we Flush the rendering thread and
    //       then suspend, but if the Rendering Thread is also calling PumpMessages when it is Flushing then it tries to consume the
    //       same event and ends up in a recursive loop until we Stack Overflow.  This change breaks that cycle and behaves correctly
    //-----
    // Handle all incoming messages if we're not using wxWindows in which case this is done by their message loop.
    if ( !bFromMainLoop )
    {
        // XboxOne - I think that the comment below only applies to Windows, and was probably Copy-n-Pasted here. DX10 is not relevent on Xbox One ****
         
        // Process pending windows messages, which is necessary to the rendering thread in some rare cases where DX10
        // sends window messages (from IDXGISwapChain::Present) to the main thread owned viewport window.
        // Only process sent messages to minimize the opportunity for re-entry in the editor, since wx messages are not deferred.
        return;
    }
 
    // Only process these messages from the Main Thread to prevent issues (PLM)
    appWinPumpMessages();
 
    // check to see if the window in the foreground was made by this process (ie, does this app
    // have focus)
    //@todo.XBOXONE: Will this always be true?
    bool HasFocus = true;
    // if its our window, allow sound, otherwise apply multiplier
    FApp::SetVolumeMultiplier(HasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier());
}
 
uint32 FXboxOneMisc::GetCharKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
    return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, false);
}
 
uint32 FXboxOneMisc::GetKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
#define ADDKEYMAP(KeyCode, KeyName)     if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };
 
    uint32 NumMappings = 0;
 
    if ( KeyCodes && KeyNames && (MaxMappings > 0) )
    {
        ADDKEYMAP(VK_LBUTTON, TEXT("LeftMouseButton"));
        ADDKEYMAP(VK_RBUTTON, TEXT("RightMouseButton"));
        ADDKEYMAP(VK_MBUTTON, TEXT("MiddleMouseButton"));
 
        ADDKEYMAP(VK_XBUTTON1, TEXT("ThumbMouseButton"));
        ADDKEYMAP(VK_XBUTTON2, TEXT("ThumbMouseButton2"));
 
        ADDKEYMAP(VK_BACK, TEXT("BackSpace"));
        ADDKEYMAP(VK_TAB, TEXT("Tab"));
        ADDKEYMAP(VK_RETURN, TEXT("Enter"));
        ADDKEYMAP(VK_PAUSE, TEXT("Pause"));
 
        ADDKEYMAP(VK_CAPITAL, TEXT("CapsLock"));
        ADDKEYMAP(VK_ESCAPE, TEXT("Escape"));
        ADDKEYMAP(VK_SPACE, TEXT("SpaceBar"));
        ADDKEYMAP(VK_PRIOR, TEXT("PageUp"));
        ADDKEYMAP(VK_NEXT, TEXT("PageDown"));
        ADDKEYMAP(VK_END, TEXT("End"));
        ADDKEYMAP(VK_HOME, TEXT("Home"));
 
        ADDKEYMAP(VK_LEFT, TEXT("Left"));
        ADDKEYMAP(VK_UP, TEXT("Up"));
        ADDKEYMAP(VK_RIGHT, TEXT("Right"));
        ADDKEYMAP(VK_DOWN, TEXT("Down"));
 
        ADDKEYMAP(VK_INSERT, TEXT("Insert"));
        ADDKEYMAP(VK_DELETE, TEXT("Delete"));
 
        ADDKEYMAP(VK_NUMPAD0, TEXT("NumPadZero"));
        ADDKEYMAP(VK_NUMPAD1, TEXT("NumPadOne"));
        ADDKEYMAP(VK_NUMPAD2, TEXT("NumPadTwo"));
        ADDKEYMAP(VK_NUMPAD3, TEXT("NumPadThree"));
        ADDKEYMAP(VK_NUMPAD4, TEXT("NumPadFour"));
        ADDKEYMAP(VK_NUMPAD5, TEXT("NumPadFive"));
        ADDKEYMAP(VK_NUMPAD6, TEXT("NumPadSix"));
        ADDKEYMAP(VK_NUMPAD7, TEXT("NumPadSeven"));
        ADDKEYMAP(VK_NUMPAD8, TEXT("NumPadEight"));
        ADDKEYMAP(VK_NUMPAD9, TEXT("NumPadNine"));
 
        ADDKEYMAP(VK_MULTIPLY, TEXT("Multiply"));
        ADDKEYMAP(VK_ADD, TEXT("Add"));
        ADDKEYMAP(VK_SUBTRACT, TEXT("Subtract"));
        ADDKEYMAP(VK_DECIMAL, TEXT("Decimal"));
        ADDKEYMAP(VK_DIVIDE, TEXT("Divide"));
 
        ADDKEYMAP(VK_F1, TEXT("F1"));
        ADDKEYMAP(VK_F2, TEXT("F2"));
        ADDKEYMAP(VK_F3, TEXT("F3"));
        ADDKEYMAP(VK_F4, TEXT("F4"));
        ADDKEYMAP(VK_F5, TEXT("F5"));
        ADDKEYMAP(VK_F6, TEXT("F6"));
        ADDKEYMAP(VK_F7, TEXT("F7"));
        ADDKEYMAP(VK_F8, TEXT("F8"));
        ADDKEYMAP(VK_F9, TEXT("F9"));
        ADDKEYMAP(VK_F10, TEXT("F10"));
        ADDKEYMAP(VK_F11, TEXT("F11"));
        ADDKEYMAP(VK_F12, TEXT("F12"));
 
        ADDKEYMAP(VK_NUMLOCK, TEXT("NumLock"));
 
        ADDKEYMAP(VK_SCROLL, TEXT("ScrollLock"));
 
        ADDKEYMAP(VK_LSHIFT, TEXT("LeftShift"));
        ADDKEYMAP(VK_RSHIFT, TEXT("RightShift"));
        ADDKEYMAP(VK_LCONTROL, TEXT("LeftControl"));
        ADDKEYMAP(VK_RCONTROL, TEXT("RightControl"));
        ADDKEYMAP(VK_LMENU, TEXT("LeftAlt"));
        ADDKEYMAP(VK_RMENU, TEXT("RightAlt"));
 
        ADDKEYMAP(VK_OEM_1, TEXT("Semicolon"));
        ADDKEYMAP(VK_OEM_PLUS, TEXT("Equals"));
        ADDKEYMAP(VK_OEM_COMMA, TEXT("Comma"));
        ADDKEYMAP(VK_OEM_MINUS, TEXT("Underscore"));
        ADDKEYMAP(VK_OEM_PERIOD, TEXT("Period"));
        ADDKEYMAP(VK_OEM_2, TEXT("Slash"));
        ADDKEYMAP(VK_OEM_3, TEXT("Tilde"));
        ADDKEYMAP(VK_OEM_4, TEXT("LeftBracket"));
        ADDKEYMAP(VK_OEM_5, TEXT("Backslash"));
        ADDKEYMAP(VK_OEM_6, TEXT("RightBracket"));
        ADDKEYMAP(VK_OEM_7, TEXT("Quote"));
    }
 
    check(NumMappings < MaxMappings);
    return NumMappings;
}
 
void FXboxOneMisc::LocalPrint(const TCHAR *Message)
{
    OutputDebugString(Message);
}
 
void FXboxOneMisc::RequestExit(bool Force)
{
    UE_LOG(LogTemp, Log, TEXT("FXboxOneMisc::RequestExit(%i)"), Force);
    if (Force)
    {
        // Force immediate exit.
        // Dangerous because config code isn't flushed, global destructors aren't called, etc.
        TerminateProcess(GetCurrentProcess(), 0);
    }
    else
    {
        GIsRequestingExit = 1;
    }
}
 
const TCHAR* FXboxOneMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
    check(OutBuffer && BufferCount);
    *OutBuffer = TEXT('\0');
    if (Error == 0)
    {
        Error = GetLastError();
    }
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), OutBuffer, BufferCount, NULL);
    TCHAR* Found = FCString::Strchr(OutBuffer,TEXT('\r'));
    if (Found)
    {
        *Found = TEXT('\0');
    }
    Found = FCString::Strchr(OutBuffer,TEXT('\n'));
    if (Found)
    {
        *Found = TEXT('\0');
    }
    return OutBuffer;
}
 
void FXboxOneMisc::CreateGuid(FGuid& Result)
{
    verify(CoCreateGuid((GUID*)&Result)==S_OK);
}
 
int32 FXboxOneMisc::NumberOfCores()
{
    SYSTEM_INFO SystemInfo = {0};
    ::GetSystemInfo( &SystemInfo );
 
    // This will return 6 or 7, depending on how the <mx:XboxSystemResources> tag of the AppXManifest file is set up.
    return SystemInfo.dwNumberOfProcessors;
}
 
/** Get the application root directory. */
/*const TCHAR* FXboxOneMisc::RootDir()
{
    return TEXT("");
}
 
const TCHAR* FXboxOneMisc::EngineDir()
{
    return TEXT("../../../Engine/");
}
 
const TCHAR* FXboxOneMisc::GameDir()
{
    static FString GameDirectory = FString::Printf(TEXT("../../../%s/"), FApp::GetGameName());
    return *GameDirectory;
}*/
 
bool FXboxOneMisc::Exec(class UWorld* InWorld, const TCHAR* Cmd, class FOutputDevice& Out)
{
    return false;
}
 
void FXboxOneMisc::RaiseException(uint32 ExceptionCode)
{
    ::RaiseException(ExceptionCode, 0, 0, NULL);
}
 
IPlatformChunkInstall* FXboxOneMisc::GetPlatformChunkInstall()
{
    static FXboxOneChunkInstall Singleton;
    return &Singleton;
}
 
// Defines the PlatformFeatures module name for Xbox One, used by PlatformFeatures.h.
const TCHAR* FXboxOneMisc::GetPlatformFeaturesModuleName()
{
    return TEXT("XboxOnePlatformFeatures");
}
 
/**
 * Platform specific function for adding a named event that can be viewed in PIX
 */
void FXboxOneMisc::BeginNamedEvent(const struct FColor& Color,const TCHAR* Text)
{
#if XBOXONE_PROFILING_ENABLED
    PIXBeginEvent(Color.DWColor(),Text);
#endif
}
 
void FXboxOneMisc::BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text)
{
#if XBOXONE_PROFILING_ENABLED
    PIXBeginEvent(Color.DWColor(),ANSI_TO_TCHAR(Text));
#endif // USE_PIX
}
 
/**
 * Platform specific function for closing a named event that can be viewed in PIX
 */
void FXboxOneMisc::EndNamedEvent()
{
#if XBOXONE_PROFILING_ENABLED
    PIXEndEvent();
#endif
}
 
FString FXboxOneMisc::ProtocolActivationUri = TEXT("");
 
void FXboxOneMisc::SetProtocolActivationUri(const FString& NewUriString)
{
    ProtocolActivationUri = NewUriString;
}
 
const FString& FXboxOneMisc::GetProtocolActivationUri()
{
    return ProtocolActivationUri;
}
 
void FXboxOneMisc::TakeKinectGPUReserve(bool bEnable)
{
    // If bEnable==true, the title can use the 4.5% Kinect GPU reserve. If false, this reserve is used by Kinect. Default is false.
    // This happens regardless of whether the player has a Kinect plugged in
    // Use this with care - titles are only allowed to use the Kinect GPU reserve during gameplay portions, not in menus, lobbies, etc so that biometric sign-in, etc still works.
    // See XDK documentation and Microsoft Xbox One Technical Requirements for details.
    Windows::ApplicationModel::Core::CoreApplication::DisableKinectGpuReservation = bEnable;
}
 
void FXboxOneMisc::BeginNamedEventEx(const FColor& Color,const ANSICHAR* Text)
{
    wchar_t ws[256];
    int32 Lent = FCStringAnsi::Strlen(Text);
    if( Lent > MAX_PIX_STRING_LENGTH )
    {
        Text += Lent-(MAX_PIX_STRING_LENGTH+64);
    }
    swprintf(ws, 256, L"%hs", Text);
#if XBOXONE_PROFILING_ENABLED
    PIXBeginEvent(Color.DWColor(),ws);
#endif // USE_PIX
}
 
//@VIRTUOS jboldiga - dynamic resolution
struct DERIVED_FRAME_STATISTICS
{
    double IdealFrameTimeInMs;
 
    double PercentCPUUsed;
    double CPUFrameTimeInMs;
    double CPUTimeLeftInMs;
    double PresentLatencyInMs;
 
    double PercentGPUUsedBySystem;
    double PercentGPUUsedByTitle;
    double PercentGPUUsedTotal;
    double GPUFrameTimeInMs;
    double GPUTimeLeftInMs;
    double FrameCompleteLatencyInMs;
 
    bool FrameDropped;
};
 
void CalculateDerivedFrameStats(
    const DXGIX_FRAME_STATISTICS& Current,
    const DXGIX_FRAME_STATISTICS& Last,
    const unsigned int PresentInterval,
    DERIVED_FRAME_STATISTICS& DerivedStats)
{
    ZeroMemory(&DerivedStats, sizeof(DerivedStats));
 
    LARGE_INTEGER CpuFreq;
    QueryPerformanceFrequency( &CpuFreq );
 
    // assume interval one "target" for immediate mode rendering
    UINT32 Intervals = PresentInterval == 0 ? 1 : PresentInterval;
 
    double CPUCountIdealFrame =
        double(CpuFreq.QuadPart) / D3D11X_XBOX_VIRTUAL_REFRESH * Intervals;
 
    double GPUCountIdealFrame =
        double(D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY) / D3D11X_XBOX_VIRTUAL_REFRESH * Intervals;
 
    DerivedStats.IdealFrameTimeInMs =
        CPUCountIdealFrame / CpuFreq.QuadPart * 1000;
 
    if( Last.GPUTimeFrameComplete != 0 && Current.GPUTimeFrameComplete != 0 )
    {
        DerivedStats.CPUFrameTimeInMs =
            double(Current.CPUTimePresentCalled - Last.CPUTimePresentCalled) /
            CpuFreq.QuadPart * 1000.0;
 
        DerivedStats.PercentCPUUsed =
            double(Current.CPUTimePresentCalled - Last.CPUTimeAddedToQueue) /
            CPUCountIdealFrame * 100.0;
 
        DerivedStats.GPUFrameTimeInMs =
            double(Current.GPUTimeFrameComplete - Last.GPUTimeFrameComplete) /
            D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY * 1000.0;
 
        DerivedStats.PercentGPUUsedBySystem =
            Current.GPUCountSystemUsed / GPUCountIdealFrame * 100.0;
 
        DerivedStats.PercentGPUUsedByTitle =
            Current.GPUCountTitleUsed / GPUCountIdealFrame * 100.0;
 
        DerivedStats.PercentGPUUsedTotal =
            DerivedStats.PercentGPUUsedBySystem + DerivedStats.PercentGPUUsedByTitle;
 
        if( Last.VSyncCount != 0 && PresentInterval != 0 )
        {
            DerivedStats.CPUTimeLeftInMs =
                ((Last.CPUTimeAddedToQueue + CPUCountIdealFrame) - double(Current.CPUTimePresentCalled))
                / CpuFreq.QuadPart * 1000.0;
 
            DerivedStats.GPUTimeLeftInMs =
                ((Last.GPUTimeVSync + GPUCountIdealFrame) - double(Current.GPUTimeFrameComplete))
                / D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY * 1000.0;
        }
    }
    if( Current.VSyncCount != 0 )
    {  
        DerivedStats.PresentLatencyInMs =
            double(Current.CPUTimeFlip - Current.CPUTimePresentCalled) /
            CpuFreq.QuadPart * 1000.0;
 
        DerivedStats.FrameCompleteLatencyInMs =
            double(Current.CPUTimeFlip - Current.CPUTimeFrameComplete) /
            CpuFreq.QuadPart * 1000.0;
        DerivedStats.FrameDropped =
            (Current.VSyncCount - Last.VSyncCount) > Intervals;
    }
}
 
extern bool GUseCustomFixedResolution; // @Virtuos [Ding Yifei]
 
// @Virtuos [Ding Yifei]: Introduce to indicate whether game is paused
#if VIRTUOS_PAIR_UE3UE4
namespace UE3
{
    extern ::uint32 GUE3IsPaused;
}
#endif
 
extern bool  ShouldLockResolutionBasedOnLevels();
extern float GetLevelBasedLockResolutionX();
 
static float CalcMaxResolutionX(double GPUUsedTotalIn, float CurrentScreenWidthIn, bool& bShouldIncreaseIfPinnedResOut, bool& bShouldDecreaseIfPinnedResOut)
{
    static bool bShouldLockResolutionBefore = false;
    static float MaxResolutionX = 1920.0f;
    const float ResolutionDiffThreshold = 10.0f;
    const bool bShouldLockResolution = ShouldLockResolutionBasedOnLevels();
    CurrentScreenWidthIn = FMath::Clamp(CurrentScreenWidthIn, 1000.0f, 1920.0f - ResolutionDiffThreshold); // in case some invalid value
    if(bShouldLockResolution)
    {
        if(bShouldLockResolutionBefore != bShouldLockResolution)
        {
            if(GetLevelBasedLockResolutionX() < CurrentScreenWidthIn || GPUUsedTotalIn < 95.0)
            {
                MaxResolutionX = GetLevelBasedLockResolutionX();
            }
            else
            {
                MaxResolutionX = CurrentScreenWidthIn;  // recent performance is worse than level based rules. just keep it.
            }
            //FPlatformMisc::LowLevelOutputDebugStringf(TEXT("PinnedRes: MaxResolutionX=%.3f, CurrentScreenWidth=%.3f, GPUUsedTotal=%.3f\n"), MaxResolutionX, CurrentScreenWidthIn, GPUUsedTotalIn);
        }
    }
    else
    {
        MaxResolutionX = 1920.0f;
    }
    bShouldLockResolutionBefore = bShouldLockResolution;
    bShouldIncreaseIfPinnedResOut = bShouldLockResolution ? MaxResolutionX > CurrentScreenWidthIn + ResolutionDiffThreshold : false;
    bShouldDecreaseIfPinnedResOut = bShouldLockResolution ? CurrentScreenWidthIn > MaxResolutionX : false;
    return FMath::Clamp(MaxResolutionX, 1000.0f, 1920.0f);
}
 
template<typename T, typename U>
void adjust_and_clamp(T& Val, const T Min, const T Max, const U Adj )
{
    assert( Val >= Min );
    assert( Val <= Max );
 
    if (Val + Adj > Max )
        Val = Max;
    else if( Val + Adj < Min )
        Val = Min;
    else
        Val += Adj;
}
 
static float sAdaptiveResolutionMinX = 1000.0f;
static FAutoConsoleVariableRef CVarAdaptiveResolutionMinX(
    TEXT("r.AdaptiveResolutionMinX"),
    sAdaptiveResolutionMinX,
    TEXT("Set the minimum resolution in the horizontal when adaptive resolution is enabled.")
    );
 
bool FXboxOneMisc::DynamicResolution(float* outPercent)
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
        *outPercent = (1000.0f / 1920.0f) * 100.0f;
        return true;
    }
    // @Virtuos END
     
    static DERIVED_FRAME_STATISTICS DerivedStats = {0};
    static DXGIX_FRAME_STATISTICS Stats[5];
    float fscreenWidth = 1920.0f * (*outPercent/100.0f);
    bool result = false;
 
    DXGIXGetFrameStatistics( 5, Stats );
 
    unsigned int i;
    for (i = 0; i < 4; i++)
    {
        if( Stats[i].GPUTimeFlip != 0 && Stats[i+1].GPUTimeFlip != 0)
        {
            break;
        }
    }
 
    unsigned int Interval = 2;
    /*
    PRESENT_PARAM_LOCKED_60: Interval = 1;
    PRESENT_PARAM_LOCKED_30: Interval = 2;
    PRESENT_PARAM_IMMEDIATE: Interval = 0;
    PRESENT_PARAM_LOCKED_60_THRESH_10: Interval = 1;
    PRESENT_PARAM_LOCKED_60_THRESH_100: Interval = 1;
    */
 
     
    if( i < 4 )  // if i == 4, we did not find valid frame stats for the last 4 frames
    {
        // calculate derived values
        CalculateDerivedFrameStats( Stats[i], Stats[i+1], Interval, DerivedStats );
         
        // apply simple dynamic changes to main scene view port
        double Used = DerivedStats.PercentGPUUsedTotal;
        static double Delta = 0.0;
        static int State = 0;
 
        // pinned resolution BEGIN
        bool bShouldIncreaseIfPinnedRes,bShouldDecreaseIfPinnedRes;
        bool bShouldLockResolution = ShouldLockResolutionBasedOnLevels();
        float MaxResolutionX = CalcMaxResolutionX(Used, fscreenWidth, bShouldIncreaseIfPinnedRes, bShouldDecreaseIfPinnedRes);
        // pinned resolution END
 
        if ((!bShouldLockResolution && Used > 99.0) || bShouldDecreaseIfPinnedRes)
        {
            if( State == -1 || State == 0 )
                Delta = (Used - 99.0) * 3.0;
            else
                Delta = Delta / 2;
            adjust_and_clamp(fscreenWidth, sAdaptiveResolutionMinX, MaxResolutionX, -1.0f * (float)int( Delta + 0.5 ));
            State = -1;
            float newPercent = (fscreenWidth/1920.0f) * 100.0f;
            if(*outPercent - newPercent >= 2.0f)
            {
                *outPercent = newPercent;
                result = true;
            }
        }
        else if ((!bShouldLockResolution && Used < 95.0) || bShouldIncreaseIfPinnedRes)
        {
            if( State == 1 || State == 0 )
                Delta = (95.0 - Used) * 3.0;
            else
                Delta /= 2;
            adjust_and_clamp(fscreenWidth, sAdaptiveResolutionMinX, MaxResolutionX, (float) int( Delta + 0.5 ));
            State = 1;
            float newPercent = (fscreenWidth/1920.0f) * 100.0f;
            if(newPercent - *outPercent >= 2.0f)
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