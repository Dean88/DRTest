// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
#include "EnginePrivate.h"
 
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroupInst.h"
#include "SubtitleManager.h"
#include "Net/UnrealNetwork.h"
#include "OnlineSubsystemUtils.h"
#include "PhysicsPublic.h"
 
#include "RenderCore.h"
#include "ColorList.h"
#include "SlateBasics.h"
#include "UObjectAnnotation.h"
 
#include "IHeadMountedDisplay.h"
#include "SceneViewExtension.h"
#include "DataChannel.h"
#include "GameFramework/OnlineSession.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerState.h"
 
#include "GameDelegates.h"
 
DEFINE_LOG_CATEGORY(LogPlayerManagement);
 
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 
static TAutoConsoleVariable<int32> CVarViewportTest(
    TEXT("r.ViewportTest"),
    0,
    TEXT("Allows to test different viewport rectangle configuations (in game only) as they can happen when using Matinee/Editor.\n")
    TEXT("0: off(default)\n")
    TEXT("1..7: Various Configuations"),
    ECVF_RenderThreadSafe);
 
#endif
 
//@VIRTUOS [lizimian] BEGIN
#if (!UE_BUILD_SHIPPING && defined(VIRTUOS_PAIR_UE3UE4))
bool GShowFarLOD;
bool GShowLOD;
#endif
//#VIRTUOS END
 
//@VIRTUOS [wuzhenhai] BEGIN
extern float UTextureRenderTarget2D_SceneCaptureViewScale;
extern int mirror_skip_update;
//#VIRTUOS END
 
//////////////////////////////////////////////////////////////////////////
// Things used by ULocalPlayer::Exec
//@TODO: EXEC
 
bool GShouldLogOutAFrameOfMoveComponent = false;
bool GShouldLogOutAFrameOfSetBodyTransform = false;
 
//////////////////////////////////////////////////////////////////////////
// ULocalPlayer
 
FLocalPlayerContext::FLocalPlayerContext()
{
 
}
 
FLocalPlayerContext::FLocalPlayerContext( const class ULocalPlayer* InLocalPlayer )
{
    SetLocalPlayer( InLocalPlayer );
}
 
FLocalPlayerContext::FLocalPlayerContext( const class APlayerController* InPlayerController )
{
    SetPlayerController( InPlayerController );
}
 
FLocalPlayerContext::FLocalPlayerContext( const FLocalPlayerContext& InPlayerContext )
{
    check(InPlayerContext.GetLocalPlayer());
    SetLocalPlayer(InPlayerContext.GetLocalPlayer());
}
 
bool FLocalPlayerContext::IsValid() const
{
    return LocalPlayer.IsValid() && GetWorld() && GetPlayerController() && GetLocalPlayer();
}
 
bool FLocalPlayerContext::IsInitialized() const
{
    return LocalPlayer.IsValid();
}
 
UWorld* FLocalPlayerContext::GetWorld() const
{
    check( LocalPlayer.IsValid() );
    return LocalPlayer->GetWorld(); 
}
 
ULocalPlayer* FLocalPlayerContext::GetLocalPlayer() const
{
    check( LocalPlayer.IsValid() );
    return LocalPlayer.Get();
}
 
APlayerController* FLocalPlayerContext::GetPlayerController() const
{
    check( LocalPlayer.IsValid() );
    return LocalPlayer->PlayerController;
}
 
AGameState* FLocalPlayerContext::GetGameState() const
{
    check(LocalPlayer.IsValid());
    UWorld* World = LocalPlayer->GetWorld();
    return World ? World->GameState : NULL;
}
 
APlayerState* FLocalPlayerContext::GetPlayerState() const
{
    check(LocalPlayer.IsValid());
    APlayerController* PC = LocalPlayer->PlayerController;
    return PC ? PC->PlayerState : NULL;
}
 
AHUD* FLocalPlayerContext::GetHUD() const
{
    check(LocalPlayer.IsValid())
    APlayerController* PC = LocalPlayer->PlayerController;
    return PC ? PC->MyHUD : NULL;
}
 
class APawn* FLocalPlayerContext::GetPawn() const
{
    check(LocalPlayer.IsValid())
    APlayerController* PC = LocalPlayer->PlayerController;
    return PC ? PC->GetPawn() : NULL;
}
 
void FLocalPlayerContext::SetLocalPlayer( const ULocalPlayer* InLocalPlayer )
{
    LocalPlayer = InLocalPlayer;
}
 
void FLocalPlayerContext::SetPlayerController( const APlayerController* InPlayerController )
{
    check( InPlayerController->IsLocalPlayerController() );
    LocalPlayer = CastChecked<ULocalPlayer>(InPlayerController->Player);
}
 
bool FLocalPlayerContext::IsFromLocalPlayer(const AActor* ActorToTest) const
{
    return (ActorToTest != nullptr) &&
        IsValid() &&
        (ActorToTest == GetPlayerController() ||
        ActorToTest == GetPlayerState() ||
        ActorToTest == GetPawn());
}
 
extern bool GAllocatedRenderTargets;
 
//////////////////////////////////////////////////////////////////////////
// ULocalPlayer
 
ULocalPlayer::ULocalPlayer(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , SlateOperations( FReply::Unhandled() )
{
    PendingLevelPlayerControllerClass = APlayerController::StaticClass();
    //@VIRTUOS [WYF] BEGIN
    m_VBP_Details_Instance = NULL;
    m_VBP_Details_Instance_tmp_actor = NULL;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    struct FConstructorStatics
    {
        ConstructorHelpers::FObjectFinder<UClass> VBP_Details;
        FConstructorStatics()
            : VBP_Details(TEXT("/Game/BM_Blueprints/VBP_Details.VBP_Details_C"))
        {
             
        }
    };
 
 
    static FConstructorStatics ConstructorStatics;
     
    if (ConstructorStatics.VBP_Details.Object)
        m_VBP_Details_Instance = (UClass*)ConstructorStatics.VBP_Details.Object;
#endif     
    // @VIRTUOS[RS]
    bIsXrayEnabled = false;
    bIsThermalEnabled = false;
    bIsScanModeEnabled = false;
    //VIRTUOS END
}
 
void ULocalPlayer::PostInitProperties()
{
    Super::PostInitProperties();
    if ( !IsTemplate() )
    {
        ViewState.Allocate();
         
        if( GEngine->StereoRenderingDevice.IsValid() )
        {
            StereoViewState.Allocate();
        }
    }
 
    if (!HasAnyFlags(RF_ClassDefaultObject))
    {  
        FCoreDelegates::OnControllerConnectionChange.AddUObject(this, &ULocalPlayer::HandleControllerConnectionChange);
    }
}
 
void ULocalPlayer::PlayerAdded(UGameViewportClient* InViewportClient, int32 InControllerID)
{
    ViewportClient      = InViewportClient;
    ControllerId        = InControllerID;
}
 
void ULocalPlayer::InitOnlineSession()
{
    if (OnlineSession == NULL)
    {
        UClass* SpawnClass = GetOnlineSessionClass();
        OnlineSession = NewObject<UOnlineSession>(this, SpawnClass);
        if (OnlineSession)
        {
            UWorld* World = GetWorld();
            if (World != NULL)
            {
                OnlineSession->RegisterOnlineDelegates(World);
            }
        }
    }
}
 
void ULocalPlayer::PlayerRemoved()
{
    // Clear all online delegates
    if (OnlineSession != NULL)
    {
        OnlineSession->ClearOnlineDelegates(GetWorld());
        OnlineSession = NULL;
    }
}
 
TSubclassOf<UOnlineSession> ULocalPlayer::GetOnlineSessionClass()
{
    return UOnlineSession::StaticClass();
}
 
void ULocalPlayer::HandleDisconnect(UWorld *World, UNetDriver *NetDriver)
{
    if (OnlineSession)
    {
        OnlineSession->HandleDisconnect(World, NetDriver);
    }
    else
    {
        // Let the engine cleanup this disconnect
        GEngine->HandleDisconnect(World, NetDriver);
    }
}
 
bool ULocalPlayer::SpawnPlayActor(const FString& URL,FString& OutError, UWorld* InWorld)
{
    check(InWorld);
    if ( InWorld->IsServer() )
    {
        FURL PlayerURL(NULL, *URL, TRAVEL_Absolute);
 
        // Get player nickname
        FString PlayerName = GetNickname();
        if (PlayerName.Len() > 0)
        {
            PlayerURL.AddOption(*FString::Printf(TEXT("Name=%s"), *PlayerName));
        }
 
        // Send any game-specific url options for this player
        FString GameUrlOptions = GetGameLoginOptions();
        if (GameUrlOptions.Len() > 0)
        {
            PlayerURL.AddOption(*FString::Printf(TEXT("%s"), *GameUrlOptions));
        }
 
        // Get player unique id
        TSharedPtr<FUniqueNetId> UniqueId = GetPreferredUniqueNetId();
 
        PlayerController = InWorld->SpawnPlayActor(this, ROLE_SimulatedProxy, PlayerURL, UniqueId, OutError, GEngine->GetGamePlayers(InWorld).Find(this));
    }
    else
    {
        // Statically bind to the specified player controller
        UClass* PCClass = PendingLevelPlayerControllerClass;
        // The PlayerController gets replicated from the client though the engine assumes that every Player always has
        // a valid PlayerController so we spawn a dummy one that is going to be replaced later.
 
        //
        // Look at APlayerController::OnActorChannelOpen + UNetConnection::HandleClientPlayer for the code the
        // replaces this fake player controller with the real replicated one from the server
        //
 
        FActorSpawnParameters SpawnInfo;
        SpawnInfo.ObjectFlags |= RF_Transient;  // We never want to save player controllers into a map
        PlayerController = CastChecked<APlayerController>(InWorld->SpawnActor<APlayerController>(PCClass, SpawnInfo));
        const int32 PlayerIndex = GEngine->GetGamePlayers(InWorld).Find(this);
        PlayerController->NetPlayerIndex = PlayerIndex;
    }
    return PlayerController != NULL;
}
 
void ULocalPlayer::SendSplitJoin()
{
    UNetDriver* NetDriver = NULL;
 
    UWorld* World = GetWorld();
    if (World)
    {
        NetDriver = World->GetNetDriver();
    }
 
    if (World == NULL || NetDriver == NULL || NetDriver->ServerConnection == NULL || NetDriver->ServerConnection->State != USOCK_Open)
    {
        UE_LOG(LogPlayerManagement, Warning, TEXT("SendSplitJoin(): Not connected to a server"));
    }
    else if (!bSentSplitJoin)
    {
        // make sure we don't already have a connection
        bool bNeedToSendJoin = false;
        if (PlayerController == NULL)
        {
            bNeedToSendJoin = true;
        }
        else if (NetDriver->ServerConnection->PlayerController != PlayerController)
        {
            bNeedToSendJoin = true;
            for (int32 i = 0; i < NetDriver->ServerConnection->Children.Num(); i++)
            {
                if (NetDriver->ServerConnection->Children[i]->PlayerController == PlayerController)
                {
                    bNeedToSendJoin = false;
                    break;
                }
            }
        }
 
        if (bNeedToSendJoin)
        {
            // use the default URL except for player name for splitscreen players
            FURL URL;
            URL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
 
            // Send the player nickname at login
            FString PlayerName = GetNickname();
            if (PlayerName.Len() > 0)
            {
                URL.AddOption(*FString::Printf(TEXT("Name=%s"), *PlayerName));
            }
 
            // Send the player unique Id at login
            FUniqueNetIdRepl UniqueIdRepl(GetPreferredUniqueNetId());
 
            FString URLString = URL.ToString();
            FNetControlMessage<NMT_JoinSplit>::Send(NetDriver->ServerConnection, URLString, UniqueIdRepl);
            bSentSplitJoin = true;
        }
    }
}
 
void ULocalPlayer::BeginDestroy()
{
    FCoreDelegates::OnControllerConnectionChange.RemoveAll(this);
    Super::BeginDestroy();
}
 
void ULocalPlayer::FinishDestroy()
{
    if ( !IsTemplate() )
    {
        ViewState.Destroy();
        StereoViewState.Destroy();
    }
    Super::FinishDestroy();
}
 
/**
 * Singleton managing saved locked views and the current per-player state.
 */
class FLockedViewState
{
public:
    /** Singleton accessor. */
    static FLockedViewState& Get()
    {
        static FLockedViewState State;
        return State;
    }
 
    /**
     * Retrieves the locked view point for the given player.
     * @param Player            The player for which to retrieve the locked view point.
     * @param OutViewLocation   The location at which the view was locked.
     * @param OutViewRotation   The rotation at which the view was locked.
     * @param OutFOV            The FOV at which the view was locked.
     * @returns true if the view is locked, false if it is not.
     */
    bool GetViewPoint(ULocalPlayer* Player, FVector& OutViewLocation, FRotator& OutViewRotation, float& OutFOV)
    {
        FPlayerState PlayerState = PlayerStates.GetAnnotation(Player);
        if (PlayerState.bLocked)
        {
            OutViewLocation = PlayerState.ViewPoint.Location;
            OutViewRotation = PlayerState.ViewPoint.Rotation;
            OutFOV = PlayerState.ViewPoint.FOV;
            return true;
        }
        return false;
    }
 
    /**
     * Returns true if the player's viewpoint is locked.
     */
    bool IsViewLocked(ULocalPlayer* Player)
    {
        FPlayerState PlayerState = PlayerStates.GetAnnotation(Player);
        return PlayerState.bLocked;
    }
 
    /**
     * Forces the player's view to be unlocked.
     */
    void UnlockView(ULocalPlayer* Player)
    {
        PlayerStates.RemoveAnnotation(Player);
    }
 
    /**
     * Processes a LockView console command.
     * @param Player    The player for which the command was given.
     * @param Args      Arguments to the LockView command.
     */
    void LockView(ULocalPlayer* Player, const TArray<FString>& Args)
    {
        bool bPrintHelp = false;
        bool bShouldLockView = false;
        FPlayerState PlayerState = PlayerStates.GetAnnotation(Player);
 
        // ? as only arg == display help.
        if (Args.Num() == 1 && Args[0] == TEXT("?"))
        {
            bPrintHelp = true;
        }
        // No args == toggle view locking.
        else if (Args.Num() == 0)
        {
            if (PlayerState.bLocked)
            {
                PlayerStates.RemoveAnnotation(Player);
            }
            else
            {
                FMinimalViewInfo MinViewInfo;
                Player->GetViewPoint(MinViewInfo, eSSP_FULL);
                PlayerState.ViewPoint.Location = MinViewInfo.Location;
                PlayerState.ViewPoint.Rotation = MinViewInfo.Rotation;
                PlayerState.ViewPoint.FOV = MinViewInfo.FOV;
                bShouldLockView = true;
            }
        }
        // One arg == lock view at named location.
        else if (Args.Num() == 1)
        {
            FName ViewName(*Args[0]);
            if (Viewpoints.Contains(ViewName))
            {
                PlayerState.ViewPoint = Viewpoints.FindRef(ViewName);
            }
            else
            {
                FMinimalViewInfo MinViewInfo;
                Player->GetViewPoint(MinViewInfo, eSSP_FULL);
                PlayerState.ViewPoint.Location = MinViewInfo.Location;
                PlayerState.ViewPoint.Rotation = MinViewInfo.Rotation;
                PlayerState.ViewPoint.FOV = MinViewInfo.FOV;
                Viewpoints.Add(ViewName,PlayerState.ViewPoint);
            }
            bShouldLockView = true;
        }
        // Six args == specify explicit location
        else if (Args.Num() == 6)
        {
            bool bAnyEmpty = false;
            for (int32 i = 0; i < Args.Num(); ++i)
            {
                bAnyEmpty |= Args[0].Len() == 0;
            }
            if (bAnyEmpty)
            {
                bPrintHelp = true;
            }
            else
            {
                PlayerState.ViewPoint = GetViewPointFromStrings(&Args[0],Args.Num());
                bShouldLockView = true;
            }
        }
        // Seven args == specify an explicit location and store it.
        else if (Args.Num() == 7)
        {
            bool bAnyEmpty = false;
            for (int32 i = 0; i < Args.Num(); ++i)
            {
                bAnyEmpty |= Args[0].Len() == 0;
            }
            if (bAnyEmpty)
            {
                bPrintHelp = true;
            }
            else
            {
                FName ViewName(*Args[0]);
                PlayerState.ViewPoint = GetViewPointFromStrings(&Args[1],Args.Num() - 1);
                Viewpoints.Add(ViewName,PlayerState.ViewPoint);
                bShouldLockView = true;
            }
        }
        // Anything else: unrecognized. Print help.
        else
        {
            bPrintHelp = true;
        }
 
        if (bShouldLockView)
        {
            PlayerState.bLocked = true;
            PlayerStates.AddAnnotation(Player, PlayerState);
 
            // Also copy to the clipboard.
            FString ViewPointString = ViewPointToString(PlayerState.ViewPoint);
            FPlatformMisc::ClipboardCopy(*ViewPointString);
        }
 
        if (bPrintHelp)
        {
            UE_LOG(LogConsoleResponse,Display,
                TEXT("Locks the player view and rendering time.\n")
                TEXT("r.LockView ?\n")
                TEXT("   Displays this message.\n")
                TEXT("r.LockView\n")
                TEXT("   Toggles whether the view is currently locked.\n")
                TEXT("r.LockView <name>\n")
                TEXT("   Locks the view at the named location. If there is no stored view with that name the current view is stored with that name.\n")
                TEXT("r.LockView x y z pitch yaw roll\n")
                TEXT("   Locks the view at the specified location and rotation.\n")
                TEXT("r.LockView <name> x y z pitch yaw roll\n")
                TEXT("   Locks the view at the specified location and rotation and stores it with the specified name.\n")
                );
        }
    }
 
private:
    /**
     * Information stored for a given viewpoint.
     */
    struct FViewPoint
    {
        FVector Location;
        float FOV;
        FRotator Rotation;
    };
 
    /** Viewpoints stored by name. */
    TMap<FName,FViewPoint> Viewpoints;
 
    /**
     * Per-player state attached to ULocalPlayer objects via a sparse UObject
     * annotation.
     */
    struct FPlayerState
    {
        FViewPoint ViewPoint;
        bool bLocked;
 
        FPlayerState()
        {
            ViewPoint.Location = FVector::ZeroVector;
            ViewPoint.FOV = 90.0f;
            ViewPoint.Rotation = FRotator::ZeroRotator;
            bLocked = false;
        }
 
        bool IsDefault() const
        {
            return bLocked == false
                && ViewPoint.Location == FVector::ZeroVector
                && ViewPoint.FOV == 90.0f
                && ViewPoint.Rotation == FRotator::ZeroRotator;
        }
    };
    FUObjectAnnotationSparse<FPlayerState,/*bAutoRemove=*/true> PlayerStates;
 
    /** Default constructor. */
    FLockedViewState()
    {
    }
 
    /**
     * Parses a viewpoint from an array of strings.
     *   WARNING: It is expected that the array has six entries!
     */
    static FViewPoint GetViewPointFromStrings(const FString* Strings, int32 NumStrings)
    {
        FViewPoint ViewPoint;
        if (NumStrings == 6)
        {
            ViewPoint.Location.X = FCString::Atof(*Strings[0]);
            ViewPoint.Location.Y = FCString::Atof(*Strings[1]);
            ViewPoint.Location.Z = FCString::Atof(*Strings[2]);
            ViewPoint.Rotation.Pitch = FCString::Atof(*Strings[3]);
            ViewPoint.Rotation.Yaw = FCString::Atof(*Strings[4]);
            ViewPoint.Rotation.Roll = FCString::Atof(*Strings[5]);
            ViewPoint.FOV = 90.0f;
        }
        return ViewPoint;
    }
 
    /**
     * Constructs a string from the view point.
     */
    static FString ViewPointToString(const FViewPoint& ViewPoint)
    {
        return FString::Printf(TEXT("%f %f %f %f %f %f"),
            ViewPoint.Location.X,
            ViewPoint.Location.Y,
            ViewPoint.Location.Z,
            ViewPoint.Rotation.Pitch,
            ViewPoint.Rotation.Yaw,
            ViewPoint.Rotation.Roll
            );
    }
 
    /**
     * Constructs a string representing all locked views and copies it to the
     * clipboard. Passing this string to r.LockViews will restore the state of
     * those locked views.
     */
    static void CopyLockedViews()
    {
        FString LockedViewsStr;
        FLockedViewState& This = FLockedViewState::Get();
        bool bFirst = true;
 
        for (TMap<FName,FViewPoint>::TConstIterator It(This.Viewpoints); It; ++It)
        {
            LockedViewsStr += FString::Printf(
                TEXT("%s%s %s"),
                bFirst ? TEXT("") : TEXT(";\n"),
                *It.Key().ToString(),
                *ViewPointToString(It.Value())
                );
            bFirst = false;
        }
        FPlatformMisc::ClipboardCopy(*LockedViewsStr);
        UE_LOG(LogConsoleResponse,Display,TEXT("%s"),*LockedViewsStr);
    }
 
    static FAutoConsoleCommand CmdCopyLockedViews;
};
 
/** Console command to copy all named locked views to the clipboard. */
FAutoConsoleCommand FLockedViewState::CmdCopyLockedViews(
    TEXT("r.CopyLockedViews"),
    TEXT("Copies all locked views in to a string that r.LockView will accept to reload them."),
    FConsoleCommandDelegate::CreateStatic(FLockedViewState::CopyLockedViews)
    );
 
void ULocalPlayer::GetViewPoint(FMinimalViewInfo& OutViewInfo, EStereoscopicPass StereoPass)
{
    if (FLockedViewState::Get().GetViewPoint(this, OutViewInfo.Location, OutViewInfo.Rotation, OutViewInfo.FOV) == false
        && PlayerController != NULL)
    {
        if (PlayerController->PlayerCameraManager != NULL)
        {
            OutViewInfo = PlayerController->PlayerCameraManager->CameraCache.POV;
            OutViewInfo.FOV = PlayerController->PlayerCameraManager->GetFOVAngle();
            PlayerController->GetPlayerViewPoint(/*out*/ OutViewInfo.Location, /*out*/ OutViewInfo.Rotation);
        }
        else
        {
            PlayerController->GetPlayerViewPoint(/*out*/ OutViewInfo.Location, /*out*/ OutViewInfo.Rotation);
        }
    }
 
    // allow HMDs to override fov
    if ((StereoPass != eSSP_FULL) && GEngine->HMDDevice.IsValid() && GEngine->IsStereoscopic3D())
    {
        float HFOV = OutViewInfo.FOV, VFOV = OutViewInfo.FOV;
        GEngine->HMDDevice->GetFieldOfView(HFOV, VFOV);
        if (VFOV > 0 && HFOV > 0)
        {
            OutViewInfo.FOV = FMath::Max(HFOV, VFOV);
            // AspectRatio won't be used until bConstrainAspectRatio is set to true,
            // but it doesn't really matter since HMD calcs its own projection matrix.
            //OutViewInfo.AspectRatio = HFOV / VFOV;
            //OutViewInfo.bConstrainAspectRatio = true;
        }
    }
}
 
#if ENABLE_BICUBIC_UPSCALING //@Virtuos [Zhangheng_a]: use Bicubic Upscaling in X dir on screen in XboxOne.
float GBicubicUpscalingPercentageX = 100.0f;
static FAutoConsoleVariableRef CVarBicubicUpscalingPercentageX(
    TEXT("r.BicubicUpscalingPercentageX"),
    GBicubicUpscalingPercentageX,
    TEXT("Enable Bicubic interpolation for upscaling in XboxOne.(only in width)\n")
    TEXT("in percent, >0 and <=100"),
    ECVF_RenderThreadSafe);
#endif
 
FSceneView* ULocalPlayer::CalcSceneView( class FSceneViewFamily* ViewFamily,
    FVector& OutViewLocation,
    FRotator& OutViewRotation,
    FViewport* Viewport,
    class FViewElementDrawer* ViewDrawer,
    EStereoscopicPass StereoPass)
{
    if ((PlayerController == NULL) || (Size.X <= 0.f) || (Size.Y <= 0.f) || (Viewport == NULL))
    {
        return NULL;
    }
 
    FSceneViewInitOptions ViewInitOptions;
 
    // get the projection data
    if (GetProjectionData(Viewport, StereoPass, /*inout*/ ViewInitOptions) == false)
    {
        // Return NULL if this we didn't get back the info we needed
        return NULL;
    }
     
    // return if we have an invalid view rect
    if (!ViewInitOptions.IsValidViewRectangle())
    {
        return NULL;
    }
 
    // Get the viewpoint...technically doing this twice
    // but it makes GetProjectionData better
    FMinimalViewInfo ViewInfo;
    GetViewPoint(ViewInfo, StereoPass);
     
    OutViewLocation = ViewInfo.Location;
    OutViewRotation = ViewInfo.Rotation;
 
    if (PlayerController->PlayerCameraManager != NULL)
    {
        // Apply screen fade effect to screen.
        if (PlayerController->PlayerCameraManager->bEnableFading)
        {
            ViewInitOptions.OverlayColor = PlayerController->PlayerCameraManager->FadeColor;
            ViewInitOptions.OverlayColor.A = FMath::Clamp(PlayerController->PlayerCameraManager->FadeAmount,0.0f,1.0f);
        }
 
        // Do color scaling if desired.
        if (PlayerController->PlayerCameraManager->bEnableColorScaling)
        {
            ViewInitOptions.ColorScale = FLinearColor(
                PlayerController->PlayerCameraManager->ColorScale.X,
                PlayerController->PlayerCameraManager->ColorScale.Y,
                PlayerController->PlayerCameraManager->ColorScale.Z
                );
        }
 
        // Was there a camera cut this frame?
        ViewInitOptions.bInCameraCut = PlayerController->PlayerCameraManager->bGameCameraCutThisFrame;
    }
     
    check(PlayerController && PlayerController->GetWorld());
 
    // Fill out the rest of the view init options
    ViewInitOptions.ViewFamily = ViewFamily;
    ViewInitOptions.SceneViewStateInterface = ((StereoPass != eSSP_RIGHT_EYE) ? ViewState.GetReference() : StereoViewState.GetReference());
    ViewInitOptions.ViewActor = PlayerController->GetViewTarget();
    ViewInitOptions.ViewElementDrawer = ViewDrawer;
    ViewInitOptions.BackgroundColor = FLinearColor::Black;
    ViewInitOptions.LODDistanceFactor = PlayerController->LocalPlayerCachedLODDistanceFactor;
    ViewInitOptions.StereoPass = StereoPass;
    ViewInitOptions.WorldToMetersScale = PlayerController->GetWorldSettings()->WorldToMeters;
    ViewInitOptions.CursorPos = Viewport->HasMouseCapture() ? FIntPoint(-1, -1) : FIntPoint(Viewport->GetMouseX(), Viewport->GetMouseY());
    ViewInitOptions.bOriginOffsetThisFrame = PlayerController->GetWorld()->bOriginOffsetThisFrame;
    ViewInitOptions.bUseFieldOfViewForLOD = ViewInfo.bUseFieldOfViewForLOD;
    PlayerController->BuildHiddenComponentList(OutViewLocation, /*out*/ ViewInitOptions.HiddenPrimitives);
 
    FSceneView* const View = new FSceneView(ViewInitOptions);
     
    View->ViewLocation = OutViewLocation;
    View->ViewRotation = OutViewRotation;
 
    //@TODO: SPLITSCREEN: This call will have an issue with splitscreen, as the show flags are shared across the view family
    EngineShowFlagOrthographicOverride(View->IsPerspectiveProjection(), ViewFamily->EngineShowFlags);
         
    ViewFamily->Views.Add(View);
 
    {
        View->StartFinalPostprocessSettings(OutViewLocation);
 
        // CameraAnim override
        if (PlayerController->PlayerCameraManager)
        {
            TArray<FPostProcessSettings> const* CameraAnimPPSettings;
            TArray<float> const* CameraAnimPPBlendWeights;
            PlayerController->PlayerCameraManager->GetCachedPostProcessBlends(CameraAnimPPSettings, CameraAnimPPBlendWeights);
             
            for (int32 PPIdx = 0; PPIdx < CameraAnimPPBlendWeights->Num(); ++PPIdx)
            {
                View->OverridePostProcessSettings( (*CameraAnimPPSettings)[PPIdx], (*CameraAnimPPBlendWeights)[PPIdx]);
            }
        }
 
        //  CAMERA OVERRIDE
        //  NOTE: Matinee works through this channel
        View->OverridePostProcessSettings(ViewInfo.PostProcessSettings, ViewInfo.PostProcessBlendWeight);
 
        View->EndFinalPostprocessSettings(ViewInitOptions);
    }
 
    // Upscaling or Super sampling
    {
        float LocalScreenPercentage = View->FinalPostProcessSettings.ScreenPercentage;
 
        float Fraction = 1.0f;
 
        // apply ScreenPercentage
        if (LocalScreenPercentage != 100.f)
        {
            Fraction = FMath::Clamp(LocalScreenPercentage / 100.0f, 0.1f, 4.0f);
        }
 
        // Upscale if needed
        if (Fraction != 1.0f)
        {
            // compute the view rectangle with the ScreenPercentage applied
            FIntRect ScreenPercentageAffectedViewRect = ViewInitOptions.GetConstrainedViewRect().Scale(Fraction);
            View->SetScaledViewRect(ScreenPercentageAffectedViewRect);
        }
#if ENABLE_BICUBIC_UPSCALING //@Virtuos [Zhangheng_a]: use Bicubic Upscaling in X dir on screen in XboxOne
        else if (GBicubicUpscalingPercentageX > 0 && GBicubicUpscalingPercentageX <= 100 && GAllocatedRenderTargets)
        {
            FIntRect ScreenPercentageAffectedViewRect = ViewInitOptions.GetViewRect();
            ScreenPercentageAffectedViewRect.Max.X *= (GBicubicUpscalingPercentageX / 100);
            View->SetScaledViewRect(ScreenPercentageAffectedViewRect);
        }
#endif // PLATFORM_XBOXONE
    }
 
    for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
    {
        ViewFamily->ViewExtensions[ViewExt]->SetupView(*ViewFamily, *View);
    }
     
    // @VIRTUOS [RS] Special visibility rules for detective mode
    View->bIsXray = bIsXrayEnabled;
    View->bIsThermal = bIsThermalEnabled;
    View->bIsScanMode = bIsScanModeEnabled;
 
    return View;
}
 
bool ULocalPlayer::GetPixelBoundingBox(const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, const FVector2D* OptionalAllotedSize)
{
        //@TODO: CAMERA: This has issues with aspect-ratio constrained cameras
    if ((ViewportClient != NULL) && (ViewportClient->Viewport != NULL) && (PlayerController != NULL))
    {
        // get the projection data
        FSceneViewProjectionData ProjectionData;
        if (GetProjectionData(ViewportClient->Viewport, eSSP_FULL, /*out*/ ProjectionData) == false)
        {
            return false;
        }
 
        // if we passed in an optional size, use it for the viewrect
        FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
        if (OptionalAllotedSize != NULL)
        {
            ViewRect.Min = FIntPoint(0,0);
            ViewRect.Max = FIntPoint(OptionalAllotedSize->X, OptionalAllotedSize->Y);
        }
 
        // transform the box
        const int32 NumOfVerts = 8;
        FVector Vertices[NumOfVerts] =
        {
            FVector(ActorBox.Min),
            FVector(ActorBox.Min.X, ActorBox.Min.Y, ActorBox.Max.Z),
            FVector(ActorBox.Min.X, ActorBox.Max.Y, ActorBox.Min.Z),
            FVector(ActorBox.Max.X, ActorBox.Min.Y, ActorBox.Min.Z),
            FVector(ActorBox.Max.X, ActorBox.Max.Y, ActorBox.Min.Z),
            FVector(ActorBox.Max.X, ActorBox.Min.Y, ActorBox.Max.Z),
            FVector(ActorBox.Min.X, ActorBox.Max.Y, ActorBox.Max.Z),
            FVector(ActorBox.Max)
        };
 
        // create the view projection matrix
        const FMatrix ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
 
        int SuccessCount = 0;
        OutLowerLeft = FVector2D(FLT_MAX, FLT_MAX);
        OutUpperRight = FVector2D(FLT_MIN, FLT_MIN);
        for (int i = 0; i < NumOfVerts; ++i)
        {
            //grab the point in screen space
            const FVector4 ScreenPoint = ViewProjectionMatrix.TransformFVector4( FVector4( Vertices[i], 1.0f) );
 
            if (ScreenPoint.W > 0.0f)
            {
                float InvW = 1.0f / ScreenPoint.W;
                FVector2D PixelPoint = FVector2D( ViewRect.Min.X + (0.5f + ScreenPoint.X * 0.5f * InvW) * ViewRect.Width(),
                                                  ViewRect.Min.Y + (0.5f - ScreenPoint.Y * 0.5f * InvW) * ViewRect.Height());
             
                PixelPoint.X = FMath::Clamp<float>(PixelPoint.X, 0, ViewRect.Width());
                PixelPoint.Y = FMath::Clamp<float>(PixelPoint.Y, 0, ViewRect.Height());
 
                OutLowerLeft.X = FMath::Min(OutLowerLeft.X, PixelPoint.X);
                OutLowerLeft.Y = FMath::Min(OutLowerLeft.Y, PixelPoint.Y);
 
                OutUpperRight.X = FMath::Max(OutUpperRight.X, PixelPoint.X);
                OutUpperRight.Y = FMath::Max(OutUpperRight.Y, PixelPoint.Y);
 
                ++SuccessCount;
            }
        }
 
        // make sure we are calculating with more than one point;
        return SuccessCount >= 2;
    }
    else
    {
        return false;
    }
}
 
bool ULocalPlayer::GetPixelPoint(const FVector& InPoint, FVector2D& OutPoint, const FVector2D* OptionalAllotedSize)
{
    //@TODO: CAMERA: This has issues with aspect-ratio constrained cameras
    bool bInFrontOfCamera = true;
    if ((ViewportClient != NULL) && (ViewportClient->Viewport != NULL) && (PlayerController != NULL))
    {
        // get the projection data
        FSceneViewProjectionData ProjectionData;
        if (GetProjectionData(ViewportClient->Viewport, eSSP_FULL, /*inout*/ ProjectionData) == false)
        {
            return false;
        }
 
        // if we passed in an optional size, use it for the viewrect
        FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
        if (OptionalAllotedSize != NULL)
        {
            ViewRect.Min = FIntPoint(0,0);
            ViewRect.Max = FIntPoint(OptionalAllotedSize->X, OptionalAllotedSize->Y);
        }
 
        // create the view projection matrix
        const FMatrix ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
 
        //@TODO: CAMERA: Validate this code!
        // grab the point in screen space
        FVector4 ScreenPoint = ViewProjectionMatrix.TransformFVector4( FVector4( InPoint, 1.0f) );
 
        ScreenPoint.W = (ScreenPoint.W == 0) ? KINDA_SMALL_NUMBER : ScreenPoint.W;
 
        float InvW = 1.0f / ScreenPoint.W;
        OutPoint = FVector2D(ViewRect.Min.X + (0.5f + ScreenPoint.X * 0.5f * InvW) * ViewRect.Width(),
                             ViewRect.Min.Y + (0.5f - ScreenPoint.Y * 0.5f * InvW) * ViewRect.Height());
 
        if (ScreenPoint.W < 0.0f)
        {
            bInFrontOfCamera = false;
            OutPoint = FVector2D(ViewRect.Max) - OutPoint;
        }
    }
    return bInFrontOfCamera;
}
 
bool ULocalPlayer::GetProjectionData(FViewport* Viewport, EStereoscopicPass StereoPass, FSceneViewProjectionData& ProjectionData)
{
    // If the actor
    if ((Viewport == NULL) || (PlayerController == NULL) || (Viewport->GetSizeXY().X == 0) || (Viewport->GetSizeXY().Y == 0))
    {
        return false;
    }
 
    int32 X = FMath::TruncToInt(Origin.X * Viewport->GetSizeXY().X);
    int32 Y = FMath::TruncToInt(Origin.Y * Viewport->GetSizeXY().Y);
    uint32 SizeX = FMath::TruncToInt(Size.X * Viewport->GetSizeXY().X);
    uint32 SizeY = FMath::TruncToInt(Size.Y * Viewport->GetSizeXY().Y);
 
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 
    // We expect some size to avoid problems with the view rect manipulation
    if(SizeX > 50 && SizeY > 50)
    {
        int32 Value = CVarViewportTest.GetValueOnGameThread();
 
        if(Value)
        {
            int InsetX = SizeX / 4;
            int InsetY = SizeY / 4;
 
            // this allows to test various typical view port situations (todo: split screen)
            switch(Value)
            {
                case 1: X += InsetX; Y += InsetY; SizeX -= InsetX * 2; SizeY -= InsetY * 2;break;
                case 2: Y += InsetY; SizeY -= InsetY * 2; break;
                case 3: X += InsetX; SizeX -= InsetX * 2; break;
                case 4: SizeX /= 2; SizeY /= 2; break;
                case 5: SizeX /= 2; SizeY /= 2; X += SizeX; break;
                case 6: SizeX /= 2; SizeY /= 2; Y += SizeY; break;
                case 7: SizeX /= 2; SizeY /= 2; X += SizeX; Y += SizeY; break;
            }
        }
    }
#endif
 
    FIntRect UnconstrainedRectangle = FIntRect(X, Y, X+SizeX, Y+SizeY);
 
    ProjectionData.SetViewRectangle(UnconstrainedRectangle);
 
    // Get the viewpoint.
    FMinimalViewInfo ViewInfo;
    GetViewPoint(/*out*/ ViewInfo, StereoPass);
 
    // If stereo rendering is enabled, update the size and offset appropriately for this pass
    const bool bNeedStereo = (StereoPass != eSSP_FULL) && GEngine->IsStereoscopic3D();
    if (bNeedStereo)
    {
        GEngine->StereoRenderingDevice->AdjustViewRect(StereoPass, X, Y, SizeX, SizeY);
    }
 
    // scale distances for cull distance purposes by the ratio of our current FOV to the default FOV
    PlayerController->LocalPlayerCachedLODDistanceFactor = ViewInfo.FOV / FMath::Max<float>(0.01f, (PlayerController->PlayerCameraManager != NULL) ? PlayerController->PlayerCameraManager->DefaultFOV : 90.f);
     
    FVector StereoViewLocation = ViewInfo.Location;
    if (bNeedStereo || (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed()))
    {
        GEngine->StereoRenderingDevice->CalculateStereoViewOffset(StereoPass, ViewInfo.Rotation, GetWorld()->GetWorldSettings()->WorldToMeters, StereoViewLocation);
    }
 
    // Create the view matrix
    ProjectionData.ViewOrigin = StereoViewLocation;
    ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) * FMatrix(
        FPlane(0,   0,  1,  0),
        FPlane(1,   0,  0,  0),
        FPlane(0,   1,  0,  0),
        FPlane(0,   0,  0,  1));
 
    if (!bNeedStereo)
    {
        // Create the projection matrix (and possibly constrain the view rectangle)
        FMinimalViewInfo::CalculateProjectionMatrixGivenView(ViewInfo, AspectRatioAxisConstraint, ViewportClient->Viewport, /*inout*/ ProjectionData);
    }
    else
    {
        // Let the stereoscopic rendering device handle creating its own projection matrix, as needed
        ProjectionData.ProjectionMatrix = GEngine->StereoRenderingDevice->GetStereoProjectionMatrix(StereoPass, ViewInfo.FOV);
 
        // calculate the out rect
        ProjectionData.SetViewRectangle(FIntRect(X, Y, X + SizeX, Y + SizeY));
    }
     
     
    return true;
}
 
bool ULocalPlayer::HandleDNCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    // Create a pending Note actor (only in PIE)
    if( PlayerController )
    {
        FString Comment = FString(Cmd);
        int32 NewNoteIndex = GEngine->PendingDroppedNotes.AddZeroed();
        FDropNoteInfo& NewNote = GEngine->PendingDroppedNotes[NewNoteIndex];
 
        // Use the pawn's location if we have one
        if( PlayerController->GetPawnOrSpectator() != NULL )
        {
            NewNote.Location = PlayerController->GetPawnOrSpectator()->GetActorLocation();
        }
        else
        {
            // No pawn, so just use the camera's location
            FRotator CameraRotation;
            PlayerController->GetPlayerViewPoint(NewNote.Location, CameraRotation);
        }
 
        NewNote.Rotation = PlayerController->GetControlRotation();
        NewNote.Comment = Comment;
        UE_LOG(LogPlayerManagement, Log, TEXT("Note Dropped: (%3.2f,%3.2f,%3.2f) - '%s'"), NewNote.Location.X, NewNote.Location.Y, NewNote.Location.Z, *NewNote.Comment);
    }
    return true;
}
 
 
bool ULocalPlayer::HandleExitCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    // If there is no viewport it was already closed.
    if ( ViewportClient && ViewportClient->Viewport )
    {
        ViewportClient->CloseRequested(ViewportClient->Viewport);
    }
 
    FGameDelegates::Get().GetExitCommandDelegate().Broadcast();
 
    return true;
}
 
bool ULocalPlayer::HandlePauseCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
    Super::Exec(InWorld, TEXT("Pause"),Ar);
 
    if (!InWorld->IsPaused())
    {
        if (ViewportClient && ViewportClient->Viewport)
        {
            ViewportClient->Viewport->SetUserFocus(true);
            ViewportClient->Viewport->CaptureMouse(true);
        }
    }
    else
    {
        FSlateApplication::Get().ResetToDefaultInputSettings();
    }
     
 
    return true;
}
 
bool ULocalPlayer::HandleListMoveBodyCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    GShouldLogOutAFrameOfSetBodyTransform = true;
    return true;
}
 
bool ULocalPlayer::HandleListAwakeBodiesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    ListAwakeRigidBodies(true, GetWorld());
    return true;
}
 
 
bool ULocalPlayer::HandleListSimBodiesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    ListAwakeRigidBodies(false, GetWorld());
    return true;
}
 
bool ULocalPlayer::HandleMoveComponentTimesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    GShouldLogOutAFrameOfMoveComponent = true;
    return true;
}
 
bool ULocalPlayer::HandleListSkelMeshesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    // Iterate over all skeletal mesh components and create mapping from skeletal mesh to instance.
    TMultiMap<USkeletalMesh*,USkeletalMeshComponent*> SkeletalMeshToInstancesMultiMap;
    for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
    {
        USkeletalMeshComponent* SkeletalMeshComponent = *It;
        USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
 
        if( !SkeletalMeshComponent->IsTemplate() )
        {
            SkeletalMeshToInstancesMultiMap.Add( SkeletalMesh, SkeletalMeshComponent );
        }
    }
 
    // Retrieve player location for distance checks.
    FVector PlayerLocation = FVector::ZeroVector;
    if( PlayerController && PlayerController->GetPawn() )
    {
        PlayerLocation = PlayerController->GetPawn()->GetActorLocation();
    }
 
    // Iterate over multi-map and dump information sorted by skeletal mesh.
    for( TObjectIterator<USkeletalMesh> It; It; ++It )
    {
        // Look up array of instances associated with this key/ skeletal mesh.
        USkeletalMesh* SkeletalMesh = *It;
        TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
        SkeletalMeshToInstancesMultiMap.MultiFind( SkeletalMesh, SkeletalMeshComponents );
 
        if( SkeletalMesh && SkeletalMeshComponents.Num() )
        {
            // Dump information about skeletal mesh.
            FSkeletalMeshResource* SkelMeshResource = SkeletalMesh->GetResourceForRendering();
            check(SkelMeshResource->LODModels.Num());
            UE_LOG(LogPlayerManagement, Log, TEXT("%5i Vertices for LOD 0 of %s"),SkelMeshResource->LODModels[0].NumVertices,*SkeletalMesh->GetFullName());
 
            // Dump all instances.
            for( int32 InstanceIndex=0; InstanceIndex<SkeletalMeshComponents.Num(); InstanceIndex++ )
            {
                USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshComponents[InstanceIndex];                 
                check(SkeletalMeshComponent);
                UWorld* World = SkeletalMeshComponent->GetWorld();
                check(World);
                float TimeSinceLastRender = World->GetTimeSeconds() - SkeletalMeshComponent->LastRenderTime;
 
                UE_LOG(LogPlayerManagement, Log, TEXT("%s%2i  Component    : %s"),
                    (TimeSinceLastRender > 0.5) ? TEXT(" ") : TEXT("*"),
                    InstanceIndex,
                    *SkeletalMeshComponent->GetFullName() );
                if( SkeletalMeshComponent->GetOwner() )
                {
                    UE_LOG(LogPlayerManagement, Log, TEXT("     Owner        : %s"),*SkeletalMeshComponent->GetOwner()->GetFullName());
                }
                UE_LOG(LogPlayerManagement, Log, TEXT("     LastRender   : %f"), TimeSinceLastRender);
                UE_LOG(LogPlayerManagement, Log, TEXT("     CullDistance : %f   Distance: %f   Location: (%7.1f,%7.1f,%7.1f)"),
                    SkeletalMeshComponent->CachedMaxDrawDistance,   
                    FVector::Dist( PlayerLocation, SkeletalMeshComponent->Bounds.Origin ),
                    SkeletalMeshComponent->Bounds.Origin.X,
                    SkeletalMeshComponent->Bounds.Origin.Y,
                    SkeletalMeshComponent->Bounds.Origin.Z );
            }
        }
    }
    return true;
}
 
bool ULocalPlayer::HandleListPawnComponentsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    for( TObjectIterator<APawn> It; It; ++It )
    {
        APawn *Pawn = *It; 
        UE_LOG(LogPlayerManagement, Log, TEXT("Components for pawn: %s (collision component: %s)"),*Pawn->GetName(),*Pawn->GetRootComponent()->GetName());
 
        TInlineComponentArray<UActorComponent*> Components;
        Pawn->GetComponents(Components);
 
        for (int32 CompIdx = 0; CompIdx < Components.Num(); CompIdx++)
        {
            UActorComponent *Comp = Components[CompIdx];
            if (Comp->IsRegistered())
            {
                UE_LOG(LogPlayerManagement, Log, TEXT("  %d: %s"),CompIdx,*Comp->GetName());
            }
        }
    }
    return true;
}
 
 
bool ULocalPlayer::HandleExecCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    TCHAR Filename[512];
    if( FParse::Token( Cmd, Filename, ARRAY_COUNT(Filename), 0 ) )
    {
        ExecMacro( Filename, Ar );
    }
    return true;
}
 
bool ULocalPlayer::HandleToggleDrawEventsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if( GEmitDrawEvents )
    {
        GEmitDrawEvents = false;
        UE_LOG(LogEngine, Warning, TEXT("Draw events are now DISABLED"));
    }
    else
    {
        GEmitDrawEvents = true;
        UE_LOG(LogEngine, Warning, TEXT("Draw events are now ENABLED"));
    }
#endif
    return true;
}
 
bool ULocalPlayer::HandleToggleStreamingVolumesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    if (FParse::Command(&Cmd, TEXT("ON")))
    {
        GetWorld()->DelayStreamingVolumeUpdates( 0 );
    }
    else if (FParse::Command(&Cmd, TEXT("OFF")))
    {
        GetWorld()->DelayStreamingVolumeUpdates( INDEX_NONE );
    }
    else
    {
        if( GetWorld()->StreamingVolumeUpdateDelay == INDEX_NONE )
        {
            GetWorld()->DelayStreamingVolumeUpdates( 0 );
        }
        else
        {
            GetWorld()->DelayStreamingVolumeUpdates( INDEX_NONE );
        }
    }
    return true;
}
 
bool ULocalPlayer::HandleCancelMatineeCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
    // allow optional parameter for initial time in the matinee that this won't work (ie,
    // 'cancelmatinee 5' won't do anything in the first 5 seconds of the matinee)
    float InitialNoSkipTime = FCString::Atof(Cmd);
 
    // is the player in cinematic mode?
    if (PlayerController->bCinematicMode)
    {
        TArray<UWorld*> MatineeActorWorldsThatSkipped;
        // if so, look for all active matinees that has this Player in a director group
        for (TActorIterator<AMatineeActor> It(GetWorld()); It; ++It)
        {
            AMatineeActor* MatineeActor = *It;
 
            // is it currently playing (and skippable)?
            if (MatineeActor->bIsPlaying && MatineeActor->bIsSkippable && (MatineeActor->bClientSideOnly || MatineeActor->GetWorld()->IsServer()))
            {
                for (int32 GroupIndex = 0; GroupIndex < MatineeActor->GroupInst.Num(); GroupIndex++)
                {
                    // is the PC the group actor?
                    if (MatineeActor->GroupInst[GroupIndex]->GetGroupActor() == PlayerController)
                    {
                        const float RightBeforeEndTime = 0.1f;
                        // make sure we aren';t already at the end (or before the allowed skip time)
                        if ((MatineeActor->InterpPosition < MatineeActor->MatineeData->InterpLength - RightBeforeEndTime) &&
                            (MatineeActor->InterpPosition >= InitialNoSkipTime))
                        {
                            // skip to end
                            MatineeActor->SetPosition(MatineeActor->MatineeData->InterpLength - RightBeforeEndTime, true);
                            MatineeActorWorldsThatSkipped.AddUnique( MatineeActor->GetWorld() );
                        }
                    }
                }
            }
        }
 
        if (MatineeActorWorldsThatSkipped.Num() != 0 )
        {
            for (int iActor = 0; iActor < MatineeActorWorldsThatSkipped.Num() ; iActor++)
            {
                AGameMode* const GameMode = MatineeActorWorldsThatSkipped[ iActor ]->GetAuthGameMode();
                if (GameMode)
                {
                    GameMode->MatineeCancelled();
                }
            }
        }
    }
    return true;
}
 
 
bool ULocalPlayer::Exec(UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar)
{
#if WITH_EDITOR
    if (GIsEditor)
    {
        // Override a few commands in PIE
        if( FParse::Command(&Cmd,TEXT("DN")) )
        {
            return HandleDNCommand( Cmd, Ar );
        }
 
        if( FParse::Command(&Cmd,TEXT("CloseEditorViewport"))
        ||  FParse::Command(&Cmd,TEXT("Exit"))
        ||  FParse::Command(&Cmd,TEXT("Quit")))
        {
            return HandleExitCommand( Cmd, Ar );
        }
 
        if( FParse::Command(&Cmd,TEXT("FocusNextPIEWindow")))
        {
            GEngine->FocusNextPIEWorld(InWorld);
            return true;
        }
        if( FParse::Command(&Cmd,TEXT("FocusLastPIEWindow")))
        {
            GEngine->FocusNextPIEWorld(InWorld, true);
            return true;
        }
 
        if( FParse::Command(&Cmd,TEXT("Pause") ))
        {
            return HandlePauseCommand( Cmd, Ar, InWorld );
        }
    }
#endif // WITH_EDITOR
 
// NOTE: all of these can probably be #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) out
 
    if( FParse::Command(&Cmd,TEXT("LISTMOVEBODY")) )
    {
        return HandleListMoveBodyCommand( Cmd, Ar );
    }
    else if( FParse::Command(&Cmd,TEXT("r.LockView")) )
    {
        TArray<FString> Lines;
        FString CmdString(Cmd);
        int32 NewLineIndex;
        if (CmdString.FindChar(TEXT(';'),NewLineIndex))
        {
            CmdString.ParseIntoArray(Lines,TEXT(";"),true);
        }
        else
        {
            Lines.Add(CmdString);
        }
 
        for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
        {
            TArray<FString> Args;
            Lines[LineIndex].ParseIntoArrayWS(Args);
            FLockedViewState::Get().LockView(this,Args);
        }
        if (Lines.Num() > 1)
        {
            FLockedViewState::Get().UnlockView(this);
        }
        return true;
    }
#if WITH_PHYSX
    // This will list all awake rigid bodies
    else if( FParse::Command(&Cmd,TEXT("LISTAWAKEBODIES")) )
    {
        return HandleListAwakeBodiesCommand( Cmd, Ar );
    }
    // This will list all simulating rigid bodies
    else if( FParse::Command(&Cmd,TEXT("LISTSIMBODIES")) )
    {
        return HandleListSimBodiesCommand( Cmd, Ar );      
    }
#endif
    else if( FParse::Command(&Cmd, TEXT("MOVECOMPTIMES")) )
    {
        return HandleMoveComponentTimesCommand( Cmd, Ar ); 
    }
    else if( FParse::Command(&Cmd,TEXT("LISTSKELMESHES")) )
    {
        return HandleListSkelMeshesCommand( Cmd, Ar );
    }
    else if ( FParse::Command(&Cmd,TEXT("LISTPAWNCOMPONENTS")) )
    {
        return HandleListPawnComponentsCommand( Cmd, Ar ); 
    }
    else if( FParse::Command(&Cmd,TEXT("EXEC")) )
    {
        return HandleExecCommand( Cmd, Ar );
    }
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    else if( FParse::Command(&Cmd,TEXT("TOGGLEDRAWEVENTS")) )
    {
        return HandleToggleDrawEventsCommand( Cmd, Ar );
    }
#endif
    else if( FParse::Command(&Cmd,TEXT("TOGGLESTREAMINGVOLUMES")) )
    {
        return HandleToggleStreamingVolumesCommand( Cmd, Ar );
    }
    // @hack: This is a test matinee skipping function, quick and dirty to see if it's good enough for
    // gameplay. Will fix up better when we have some testing done!
    else if (FParse::Command(&Cmd, TEXT("CANCELMATINEE")))
    {
        return HandleCancelMatineeCommand( Cmd, Ar );  
    }
    //@VIRTUOS [wuzhenhai] BEGIN
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    else if (FParse::Command(&Cmd, TEXT("SetSceneCaptureViewScale")))
    {
        UTextureRenderTarget2D_SceneCaptureViewScale = FCString::Atof(Cmd);
        if(UTextureRenderTarget2D_SceneCaptureViewScale <=0)
        {
            UTextureRenderTarget2D_SceneCaptureViewScale =1.0f;
            return false;
        }
        else
        {
            return true;
        }
    }
    else if (FParse::Command(&Cmd, TEXT("SetMirrorSkip")))
    {
        mirror_skip_update = FCString::Atof(Cmd);
        return true;
    }
#endif
 
    //VIRTUOS END
    //@VIRTUOS [WYF] BEGIN
    else if (FParse::Command(&Cmd, TEXT("V.VERSION")))
    {
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
        if (FParse::Command(&Cmd, TEXT("OFF")))
        {
            if (m_VBP_Details_Instance && m_VBP_Details_Instance_tmp_actor)
            {
                InWorld->DestroyActor(m_VBP_Details_Instance_tmp_actor);
                m_VBP_Details_Instance_tmp_actor = NULL;
            }
        }
        else if (m_VBP_Details_Instance && !m_VBP_Details_Instance_tmp_actor)
        {
            m_VBP_Details_Instance_tmp_actor = InWorld->SpawnActor(m_VBP_Details_Instance);
            FString filter_str(Cmd);
            if (filter_str.Len() > 2 && m_VBP_Details_Instance_tmp_actor)
            {
                GEngine->m_InfoFilterString = filter_str;
            }
            else
            {
                GEngine->m_InfoFilterString = TEXT("STM+SKM+TEX+MAT");
            }
                 
        }
        else if (m_VBP_Details_Instance && m_VBP_Details_Instance_tmp_actor)
        {
            FString filter_str(Cmd);
            if (filter_str.Len() > 2 && m_VBP_Details_Instance_tmp_actor)
            {
                GEngine->m_InfoFilterString=filter_str;
            }
            else
            {
                GEngine->m_InfoFilterString = TEXT("STM+SKM+TEX+MAT");
            }
        }
         
#endif
        return true;
    }
    //VIRTUOS END
    //@VIRTUOS [lizimian] BEGIN
#if (!UE_BUILD_SHIPPING && defined(VIRTUOS_PAIR_UE3UE4))
    else if (FParse::Command(&Cmd, TEXT("TOGGLEFARLOD")))
    {
        GShowFarLOD = !GShowFarLOD;
 
        for (auto streamingLevel : InWorld->StreamingLevels)
        {
            if (streamingLevel->GetWorldAssetPackageName().Contains(TEXT("_FAR")))
            {
                streamingLevel->bShouldBeVisible = GShowFarLOD;
            }
        }
 
        return true;
    }
    else if (FParse::Command(&Cmd, TEXT("TOGGLELOD")))
    {
        GShowLOD = !GShowLOD;
 
        for (auto streamingLevel : InWorld->StreamingLevels)
        {
            if (streamingLevel->GetWorldAssetPackageName().Contains(TEXT("_LOD")))
            {
                streamingLevel->bShouldBeVisible = GShowLOD;
            }
        }
 
        return true;
    }
#endif
    //@VIRTUOS END
    else if(ViewportClient && ViewportClient->Exec( InWorld, Cmd,Ar))
    {
        return true;
    }
    else if ( Super::Exec( InWorld, Cmd, Ar ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}
 
void ULocalPlayer::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
    // make sure Binaries is specified in the filename
    FString FixedFilename;
    if (!FCString::Stristr(Filename, TEXT("Binaries")))
    {
        FixedFilename = FString(TEXT("../../Binaries/")) + Filename;
        Filename = *FixedFilename;
    }
 
    FString Text;
    if (FFileHelper::LoadFileToString(Text, Filename))
    {
        UE_LOG(LogPlayerManagement, Log, TEXT("Execing %s"), Filename);
        const TCHAR* Data = *Text;
        FString Line;
        while( FParse::Line(&Data, Line) )
        {
            Exec(GetWorld(), *Line, Ar);
        }
    }
    else
    {
        UE_SUPPRESS(LogExec, Warning, Ar.Logf(*FString::Printf( TEXT("Can't find file '%s'"), Filename) ));
    }
}
 
void ULocalPlayer::HandleControllerConnectionChange(bool bConnected, int32 InUserId, int32 InControllerId)
{
    // if this is an event for this LocalPlayer
    if (InControllerId == ControllerId)
    {
        // if we lost the connection we need to flush all the keys on the PlayerInput to avoid the PC spinning in place, or firing forever, etc.
        if (!bConnected)
        {      
            if (PlayerController && PlayerController->PlayerInput)
            {
                PlayerController->PlayerInput->FlushPressedKeys();   
            }
        }
    }
}
 
void ULocalPlayer::SetControllerId( int32 NewControllerId )
{
    if ( ControllerId != NewControllerId )
    {
        UE_LOG(LogPlayerManagement, Log, TEXT("%s changing ControllerId from %i to %i"), *GetFName().ToString(), ControllerId, NewControllerId);
 
        int32 CurrentControllerId = ControllerId;
 
        // set this player's ControllerId to -1 so that if we need to swap controllerIds with another player we don't
        // re-enter the function for this player.
        ControllerId = -1;
 
        // see if another player is already using this ControllerId; if so, swap controllerIds with them
        GEngine->SwapControllerId(this, CurrentControllerId, NewControllerId);
        ControllerId = NewControllerId;
    }
}
 
FString ULocalPlayer::GetNickname() const
{
    UWorld* World = GetWorld();
    if (World != NULL)
    {
        IOnlineIdentityPtr OnlineIdentityInt = Online::GetIdentityInterface(World);
        if (OnlineIdentityInt.IsValid())
        {
            auto UniqueId = GetPreferredUniqueNetId();
            if (UniqueId.IsValid())
            {
                return OnlineIdentityInt->GetPlayerNickname(*UniqueId);
            }
        }
    }
 
    return TEXT("");
}
 
TSharedPtr<FUniqueNetId> ULocalPlayer::GetUniqueNetIdFromCachedControllerId() const
{
    UWorld* World = GetWorld();
    if (World != NULL)
    {
        IOnlineIdentityPtr OnlineIdentityInt = Online::GetIdentityInterface(World);
        if (OnlineIdentityInt.IsValid())
        {
            TSharedPtr<FUniqueNetId> UniqueId = OnlineIdentityInt->GetUniquePlayerId(ControllerId);
            if (UniqueId.IsValid())
            {
                return UniqueId;
            }
        }
    }
 
    return NULL;
}
 
TSharedPtr<FUniqueNetId> ULocalPlayer::GetCachedUniqueNetId() const
{
    return CachedUniqueNetId;
}
 
void ULocalPlayer::SetCachedUniqueNetId( TSharedPtr<class FUniqueNetId> NewUniqueNetId )
{
    CachedUniqueNetId = NewUniqueNetId;
}
 
TSharedPtr<FUniqueNetId> ULocalPlayer::GetPreferredUniqueNetId() const
{
    // Prefer the cached unique net id (only if it's valid)
    // This is for backwards compatibility for games that don't yet cache the unique id properly
    if (GetCachedUniqueNetId().IsValid() && GetCachedUniqueNetId()->IsValid())
    {
        return GetCachedUniqueNetId();
    }
 
    // If the cached unique net id is not valid, then get the one paired with the controller
    return GetUniqueNetIdFromCachedControllerId();
}
 
bool ULocalPlayer::IsCachedUniqueNetIdPairedWithControllerId() const
{
    // Get the UniqueNetId that is paired with the controller
    TSharedPtr<FUniqueNetId> UniqueIdFromController = GetUniqueNetIdFromCachedControllerId();
 
    if (CachedUniqueNetId.IsValid() != UniqueIdFromController.IsValid())
    {
        // Definitely can't match if one is valid and not the other
        return false;
    }
 
    if (!CachedUniqueNetId.IsValid())
    {
        // Both are invalid, technically they match
        check(!UniqueIdFromController.IsValid());
        return true;
    }
 
    // Both are valid, ask them if they match
    return *CachedUniqueNetId == *UniqueIdFromController;
}
 
UWorld* ULocalPlayer::GetWorld() const
{
    return ViewportClient ? ViewportClient->GetWorld() : nullptr;
}
 
UGameInstance* ULocalPlayer::GetGameInstance() const
{
    return ViewportClient ? ViewportClient->GetGameInstance() : nullptr;
}
 
void ULocalPlayer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
    ULocalPlayer* This = CastChecked<ULocalPlayer>(InThis);
 
    FSceneViewStateInterface* Ref = This->ViewState.GetReference();
    if(Ref)
    {
        Ref->AddReferencedObjects(Collector);
    }
 
    FSceneViewStateInterface* StereoRef = This->StereoViewState.GetReference();
    if (StereoRef)
    {
        StereoRef->AddReferencedObjects(Collector);
    }
 
    UPlayer::AddReferencedObjects(This, Collector);
}
 
bool ULocalPlayer::IsPrimaryPlayer() const
{
    ULocalPlayer* const PrimaryPlayer = GetOuterUEngine()->GetFirstGamePlayer(GetWorld());
    return (this == PrimaryPlayer);
}