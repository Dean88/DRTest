// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
/*=============================================================================
     
=============================================================================*/
 
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "ScreenRendering.h"
#include "PostProcessAmbient.h"
#include "PostProcessing.h"
#include "SceneUtils.h"
 
#include "Components/SceneCaptureReflectComponent2D.h"
 
#ifdef RS_OCEAN //@Virtuos [Ding Yifei]: Disable ported ocean
#include "Components/ROceanComponent.h" //@Virtuos [Ding Yifei]
#endif //Disable ported ocean
 
extern bool GAllocatedRenderTargets;
#if ENABLE_BICUBIC_UPSCALING
extern float GBicubicUpscalingPercentageX;
#endif
 
// @Virtuos [Ding Yifei] BEGIN: For mirror optimization
extern int32 GUseTranslucentLightingVolumes;
 
int32 GUseTiledDeferredReflection = 1;
 
int32 GUseDepthBoundsTest = 1;
// @Virtuos END
 
//// @Virtuos Andy: Temporary hack for optimization.
bool GIsCapturingOW_E8Mirror = false;
// @Virtuos [zsx]: Turn off mirror rendering in specific cutscene.
bool GTurnOffMirrorInCutScene = false;
 
// Copies into render target, optionally flipping it in the Y-axis
static void CopyCaptureToTarget(FRHICommandListImmediate& RHICmdList, const FRenderTarget* Target, const FIntPoint& TargetSize, FViewInfo& View, const FIntRect& ViewRect, FTextureRHIParamRef SourceTextureRHI, bool bNeedsFlippedRenderTarget)
{
    SetRenderTarget(RHICmdList, Target->GetRenderTargetTexture(), NULL);
 
    RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
    RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
    RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
 
    TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
    TShaderMapRef<FScreenPS> PixelShader(View.ShaderMap);
    static FGlobalBoundShaderState BoundShaderState;
    SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
 
    FRenderingCompositePassContext Context(RHICmdList, View);
 
    VertexShader->SetParameters(RHICmdList, View);
    PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTextureRHI);
 
    if (bNeedsFlippedRenderTarget)
    {
        DrawRectangle(
            RHICmdList,
            ViewRect.Min.X, ViewRect.Min.Y,
            ViewRect.Width(), ViewRect.Height(),
            ViewRect.Min.X, ViewRect.Height() - ViewRect.Min.Y,
            ViewRect.Width(), -ViewRect.Height(),
            TargetSize,
            TargetSize,
            *VertexShader,
            EDRF_UseTriangleOptimization);
    }
    else
    {
        DrawRectangle(
            RHICmdList,
            ViewRect.Min.X, ViewRect.Min.Y,
            ViewRect.Width(), ViewRect.Height(),
            ViewRect.Min.X, ViewRect.Min.Y,
            ViewRect.Width(), ViewRect.Height(),
            TargetSize,
            GSceneRenderTargets.GetBufferSizeXY(),
            *VertexShader,
            EDRF_UseTriangleOptimization);
    }
}
 
static void UpdateSceneCaptureContent_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer, FTextureRenderTargetResource* TextureRenderTarget, const FName OwnerName, const FResolveParams& ResolveParams, bool bUseSceneColorTexture, bool bMirror)
{
    //// @Virtuos Andy: Temporary hack for optimization.
    GIsCapturingOW_E8Mirror = (OwnerName.ToString() == "SceneCaptureReflect2D2");
    bool bGlasses = (OwnerName.ToString() == "SceneCapture2DActor_2");
    bool bRiddlerLightFunction = (OwnerName.ToString() == "SceneCapture2DActor_3");
 
    // @Virtuos [Ding Yifei]: Indicate OW_E8 Mirror for optimization
    bool bIsCapturingOW_E8Mirror = (OwnerName.ToString() == "SceneCaptureReflect2D2");
 
    FMemMark MemStackMark(FMemStack::Get());
 
    // update any resources that needed a deferred update
    FDeferredUpdateResource::UpdateResources(RHICmdList);
 
    {
#if WANTS_DRAW_MESH_EVENTS
        FString EventName;
        OwnerName.ToString(EventName);
        SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("SceneCapture %s"), *EventName);
#else
        SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContent_RenderThread);
#endif
 
        const bool bIsMobileHDR = IsMobileHDR();
        const bool bRHINeedsFlip = RHINeedsToSwitchVerticalAxis(GMaxRHIShaderPlatform);
        const bool bNeedsFlippedRenderTarget = !bIsMobileHDR && bRHINeedsFlip;
 
        // Intermediate render target that will need to be flipped (needed on !IsMobileHDR())
        TRefCountPtr<IPooledRenderTarget> FlippedPooledRenderTarget;
 
        const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;
 
        if (bNeedsFlippedRenderTarget)
        {
            // We need to use an intermediate render target since the result will be flipped
            auto& RenderTarget = Target->GetRenderTargetTexture();
            FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Target->GetSizeXY(),
                RenderTarget.GetReference()->GetFormat(),
                TexCreate_None,
                TexCreate_RenderTargetable,
                false));
            GRenderTargetPool.FindFreeElement(Desc, FlippedPooledRenderTarget, TEXT("SceneCaptureFlipped"));
        }
 
        // Helper class to allow setting render target
        struct FRenderTargetOverride : public FRenderTarget
        {
            FRenderTargetOverride(FRHITexture2D* In)
            {
                RenderTargetTextureRHI = In;
            }
 
            virtual FIntPoint GetSizeXY() const { return FIntPoint(RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY()); }
 
            FTexture2DRHIRef GetTextureParamRef() { return RenderTargetTextureRHI; }
        } FlippedRenderTarget(
            FlippedPooledRenderTarget.GetReference()
            ? FlippedPooledRenderTarget.GetReference()->GetRenderTargetItem().TargetableTexture->GetTexture2D()
            : nullptr);
        FViewInfo& View = SceneRenderer->Views[0];
        FIntRect ViewRect = View.ViewRect;
        FIntRect UnconstrainedViewRect = View.UnconstrainedViewRect;
        SetRenderTarget(RHICmdList, Target->GetRenderTargetTexture(), NULL);
        RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, ViewRect);
 
        // @Virtuos [Ding Yifei] BEGIN: Set tweak for mirror optimization
        //FFXSystemInterface* FXSystemCache = nullptr;
        //if (SceneRenderer->Scene)
        //{
        //  FXSystemCache = SceneRenderer->Scene->FXSystem;
        //}
 
        int32 GUseTranslucentLightingVolumesCache = GUseTranslucentLightingVolumes;
        int32 GUseTiledDeferredReflectionCache = GUseTiledDeferredReflection;
#if PLATFORM_PS4
        int32 GUseDepthBoundsTestCache = GUseDepthBoundsTest;
#endif
 
        if (bIsCapturingOW_E8Mirror || bGlasses || bRiddlerLightFunction)
        {
            //if (SceneRenderer->Scene)
            //{
            //  SceneRenderer->Scene->FXSystem = nullptr; // Disable particle simulation
            //}
            GUseTranslucentLightingVolumes = 0; // Disable Translucent Volume Lighting
 
            GUseTiledDeferredReflection = 0; // Disable calculating reflection environment with compute shader
        }
 
        if (bMirror)
        {
#if PLATFORM_PS4
            GUseDepthBoundsTest = 0; // Disable depth bounds test when rendering lights in mirror on PS4
#endif
        }
        // @Virtuos [Ding Yifei] END
 
        // Render the scene normally
        {
            SCOPED_DRAW_EVENT(RHICmdList, RenderScene);
 
            if (bNeedsFlippedRenderTarget)
            {
                // Hijack the render target
                SceneRenderer->ViewFamily.RenderTarget = &FlippedRenderTarget; //-V506
            }
 
            SceneRenderer->Render(RHICmdList);
 
            if (bNeedsFlippedRenderTarget)
            {
                // And restore it
                SceneRenderer->ViewFamily.RenderTarget = Target;
            }
        }
 
        if (bNeedsFlippedRenderTarget)
        {
            // We need to flip this texture upside down (since we depended on tonemapping to fix this on the hdr path)
            SCOPED_DRAW_EVENT(RHICmdList, FlipCapture);
 
            FIntPoint TargetSize(bMirror ? TextureRenderTarget->GetSizeXY() : UnconstrainedViewRect.Size());
            CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, FlippedRenderTarget.GetTextureParamRef(), true);
        }
        else if (bUseSceneColorTexture && (bIsMobileHDR || SceneRenderer->FeatureLevel >= ERHIFeatureLevel::SM4))
        {
            // Copy the captured scene into the destination texture (only required on HDR or deferred as that implies post-processing)
            SCOPED_DRAW_EVENT(RHICmdList, CaptureSceneColor);
            FIntPoint TargetSize(bMirror ? TextureRenderTarget->GetSizeXY() : UnconstrainedViewRect.Size());
            CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, GSceneRenderTargets.GetSceneColorTexture(), false);
        }
 
        // @Virtuos [Ding Yifei] BEGIN: Restore tweak for mirror optimization
        if (bIsCapturingOW_E8Mirror || bGlasses || bRiddlerLightFunction)
        {
            //if (SceneRenderer->Scene)
            //{
            //  SceneRenderer->Scene->FXSystem = FXSystemCache; // Restore particle simulation
            //}
            GUseTranslucentLightingVolumes = GUseTranslucentLightingVolumesCache; // Restore Translucent Volume Lighting
 
            GUseTiledDeferredReflection = GUseTiledDeferredReflectionCache; // Restore reflection environment calculation with compute shader
 
            GIsCapturingOW_E8Mirror = false;
        }
 
        if (bMirror)
        {
#if PLATFORM_PS4
            GUseDepthBoundsTest = GUseDepthBoundsTestCache; // Restore depth bounds test on PS4
#endif
        }
        // @Virtuos [Ding Yifei] END
 
        RHICmdList.CopyToResolveTarget(TextureRenderTarget->GetRenderTargetTexture(), TextureRenderTarget->TextureRHI, false, ResolveParams);
    }
 
    delete SceneRenderer;
}
 
FSceneRenderer* FScene::CreateSceneRenderer( USceneCaptureComponent* SceneCaptureComponent, UTextureRenderTarget* TextureTarget, const FMatrix& ViewRotationMatrix, const FVector& ViewLocation, float FOV, float MaxViewDistance, bool bCaptureSceneColour, FPostProcessSettings* PostProcessSettings, float PostProcessBlendWeight, FPlane ClipPlane, bool bMirror
#ifdef RS_OCEAN //@Virtuos [Ding Yifei]: Disable ported ocean
                                            , bool IsOceanCase
#endif //Disable ported ocean
                                            )
{
    // @Virtuos [Ding Yifei]: Indicate OW_E8 Mirror for optimization
    bool bIsCapturingOW_E8Mirror = SceneCaptureComponent->GetOwner() && SceneCaptureComponent->GetOwner()->GetName() == "SceneCaptureReflect2D2";
     
    FIntPoint CaptureSize(TextureTarget->GetSurfaceWidth(), TextureTarget->GetSurfaceHeight());
 
    // @Virtuos [Ding Yifei]: Compatible with dynamic resolusion
    int32 ScaledCaptureSizeX = TextureTarget->GetSurfaceWidth();
#if ENABLE_BICUBIC_UPSCALING
    ScaledCaptureSizeX *= GBicubicUpscalingPercentageX / 100.0f;
#endif
 
//  static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage")); //@Virtuos [Zhangheng_a]:fix screen stretch in mirror room
//  float value = CVar->GetValueOnGameThread();
//  if (value > 0.0f)
//  {
//      CaptureSize.X = FMath::CeilToInt(CaptureSize.X * value/100.f);
//      CaptureSize.Y = FMath::CeilToInt(CaptureSize.Y * value/100.f);
//  }
//#if ENABLE_BICUBIC_UPSCALING
//  // @Virtuos [pengshu]: temporary code to fix the mirror when bicubic upscaling is enabled
//  {
//      if (0 < GBicubicUpscalingPercentageX && GBicubicUpscalingPercentageX < 100 && GAllocatedRenderTargets) {
//          CaptureSize.X = (uint32)(CaptureSize.X * GBicubicUpscalingPercentageX / 100);
//          CaptureSize.Y = (uint32)(CaptureSize.Y * GBicubicUpscalingPercentageX / 100);
//      }
//  }
//#endif
 
    FTextureRenderTargetResource* Resource = TextureTarget->GameThread_GetRenderTargetResource();
    FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
        Resource,
        this,
        SceneCaptureComponent->ShowFlags)
        .SetResolveScene(!bCaptureSceneColour));
 
    FSceneViewInitOptions ViewInitOptions;
    ViewInitOptions.SetViewRectangle(FIntRect(0, 0, ScaledCaptureSizeX, CaptureSize.Y));
    ViewInitOptions.ViewFamily = &ViewFamily;
    ViewInitOptions.ViewOrigin = ViewLocation;
    ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
    ViewInitOptions.BackgroundColor = FLinearColor::Black;
    ViewInitOptions.OverrideFarClippingPlaneDistance = MaxViewDistance;
    ViewInitOptions.SceneViewStateInterface = SceneCaptureComponent->GetViewState();
 
    if (bCaptureSceneColour)
    {
        ViewFamily.EngineShowFlags.PostProcessing = 0;
        ViewInitOptions.OverlayColor = FLinearColor::Black;
    }
 
    // Build projection matrix
    {
        float XAxisMultiplier;
        float YAxisMultiplier;
 
        if (CaptureSize.X > CaptureSize.Y)
        {
            // if the viewport is wider than it is tall
            XAxisMultiplier = 1.0f;
            YAxisMultiplier = CaptureSize.X / (float)CaptureSize.Y;
        }
        else
        {
            // if the viewport is taller than it is wide
            XAxisMultiplier = CaptureSize.Y / (float)CaptureSize.X;
            YAxisMultiplier = 1.0f;
        }
 
        if ((int32)ERHIZBuffer::IsInverted != 0)
        {
            ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
                FOV,
                FOV,
                XAxisMultiplier,
                YAxisMultiplier,
                GNearClippingPlane,
                GNearClippingPlane
                );
        }
        else
        {
            ViewInitOptions.ProjectionMatrix = FPerspectiveMatrix(
                FOV,
                FOV,
                XAxisMultiplier,
                YAxisMultiplier,
                GNearClippingPlane,
                GNearClippingPlane
                );
        }
 
//VIRTUOS [shenmo]: Begin overwrite the near clip plane
        if (ClipPlane != FPlane(0,0,0,0))
        {
            FClipProjectionMatrix ClipProjectionMatrix(ViewInitOptions.ProjectionMatrix, ClipPlane);
 
            ////Change projection for reversed matrix.
            ////Near = -1  when point is on the panel,      z=-1
            ////Far  = 0   when point is far away from camera,z=0
            //ClipProjectionMatrix.M[2][2] += 1.0f;
            //ClipProjectionMatrix.M[3][2] += -1.0f;
            //ClipProjectionMatrix.M[3][3] =  -1.0f;
            ViewInitOptions.ProjectionMatrix = ClipProjectionMatrix;
        }
//VIRTUOS [shenmo]: End
    }
 
    FSceneView* View = new FSceneView(ViewInitOptions);
    bool bGlasses = (SceneCaptureComponent->GetOwner()->GetName() == "SceneCapture2DActor_2");
    bool bRiddlerLightFunction = (SceneCaptureComponent->GetOwner()->GetFullName() == "SceneCapture2D /Game/Maps/Riddler/Riddler_01_Design.Riddler_01_Design:PersistentLevel.SceneCapture2DActor_3");
 
    // @Virtuos [Ding Yifei] BEGIN: Disable unnecessary rendering in reflection capture
    if (bIsCapturingOW_E8Mirror || bGlasses || bRiddlerLightFunction)
    {
        ViewFamily.EngineShowFlags.ScreenSpaceReflections = 0; // Disable SSR
        ViewFamily.EngineShowFlags.Fog = 0; // Disable fog
        ViewFamily.EngineShowFlags.Refraction = 0; // Disable distortion
        ViewFamily.EngineShowFlags.LightShafts = 0; // Disable light shaft bloom
        //ViewFamily.EngineShowFlags.SubsurfaceScattering = 0; // Disable composition lighting
    }  
    if (bIsCapturingOW_E8Mirror)
    {
        ViewFamily.EngineShowFlags.ReflectionEnvironment = 0;
    }
    // @Virtuos [Ding Yifei] END
    if (bRiddlerLightFunction)
    {
        ViewFamily.EngineShowFlags.Particles = 0;
        ViewFamily.EngineShowFlags.ReflectionEnvironment = 0;
        ViewFamily.EngineShowFlags.Translucency = 0;
    }
 
    View->bIsSceneCapture = true;
 
    if (bMirror)
    {
        View->bEnableCapeDepthBiasHack = true;
    }
 
#ifdef RS_OCEAN //@Virtuos [Ding Yifei]: Disable ported ocean
    //@Virtuos [Ding Yifei] Begin: Filter primitives without reflection tag in ocean case
    if (SceneCaptureComponent->IsA(USceneCaptureReflectComponent2D::StaticClass()) && IsOceanCase)
    {
        SceneCaptureComponent->HiddenComponents.Empty();
        SceneCaptureComponent->HiddenComponents.Reserve(Primitives.Num());
        for (int32 pidx = 0; pidx < Primitives.Num(); ++pidx)
        {
            if (pidx == Primitives.Num())
                break;
 
            if (Primitives[pidx] && Primitives[pidx]->Proxy && !Primitives[pidx]->Proxy->bOnlyReflectionSee)   //@VIRTUOS [RS]: fixed a crash when Proxy is null
                SceneCaptureComponent->HiddenComponents.Add(Primitives[pidx]->Proxy->Component);
        }
    }
    //@Virtuos [Ding Yifei] End
#endif //Disable ported ocean
 
    check(SceneCaptureComponent);
    for (auto It = SceneCaptureComponent->HiddenComponents.CreateConstIterator(); It; ++It)
    {
        // If the primitive component was destroyed, the weak pointer will return NULL.
        UPrimitiveComponent* PrimitiveComponent = It->Get();
        if (PrimitiveComponent)
        {
            View->HiddenPrimitives.Add(PrimitiveComponent->ComponentId);
        }
    }
 
    ViewFamily.Views.Add(View);
 
    View->StartFinalPostprocessSettings(ViewLocation);
    View->OverridePostProcessSettings(*PostProcessSettings, PostProcessBlendWeight);
    View->EndFinalPostprocessSettings(ViewInitOptions);
 
    return FSceneRenderer::CreateSceneRenderer(&ViewFamily, NULL);
}
 
void FScene::UpdateSceneCaptureContents(USceneCaptureComponent2D* CaptureComponent)
{
    check(CaptureComponent);
 
    bool bMirror = false;
 
    if (CaptureComponent->TextureTarget)
    {
        FTransform Transform = CaptureComponent->GetComponentToWorld();
        FVector ViewLocation = Transform.GetTranslation();
//VIRTUOS [shenmo]: Begin capture when these actor are visible
        if (CaptureComponent->OnlyCaptureIfTheseActorsVisible.Num() > 0)
        {
            bool OneVisible = false;
            for (int i = 0; i < CaptureComponent->OnlyCaptureIfTheseActorsVisible.Num(); i++)
            {
                if (CaptureComponent->OnlyCaptureIfTheseActorsVisible[i] != NULL &&
                    GetWorld()->GetTimeSeconds() - CaptureComponent->OnlyCaptureIfTheseActorsVisible[i]->GetLastRenderTime() < 1.0f)
                {
                    OneVisible = true;
                    break;
                }
            }
            if (!OneVisible)   
                return;
        }
//VIRTUOS [shenmo]: End
 
        // @Virtuos [zsx]: Fix BMBUG-8293. Turn off mirror rendering in specific cutscene.
        if (GTurnOffMirrorInCutScene && CaptureComponent->GetOuter() &&
            CaptureComponent->GetOuter()->GetFullName().Contains(TEXT("SceneCaptureReflect2D2")) && CaptureComponent->GetOuter()->GetFullName().Contains(TEXT("OW_E8")))
        {
            return;
        }
 
        // Remove the translation from Transform because we only need rotation.
        Transform.SetTranslation(FVector::ZeroVector);
        FMatrix ViewRotationMatrix = Transform.ToInverseMatrixWithScale();
 
//VIRTUOS [shenmo] Begin: Implement SceneCaptureReflect
#ifdef RS_OCEAN //@Virtuos [Ding Yifei]: Disable ported ocean
        bool IsOceanCase = CaptureComponent->GetAttachmentRootActor()->GetFullName().Contains(TEXT("SceneOceanReflection")); //@Virtuos [Ding Yifei]: Indicate whether in ocean reflection case
#endif //Disable ported ocean
        FPlane MirrorPlane = FPlane(0,0,0,0);
        FMirrorMatrix MirrorMatrix(MirrorPlane);
        if (CaptureComponent->IsA(USceneCaptureReflectComponent2D::StaticClass()))
        {
            bMirror = true;
             
            USceneCaptureReflectComponent2D* CaptureReflectComponent = Cast<USceneCaptureReflectComponent2D>(CaptureComponent);
            if (CaptureReflectComponent && CaptureReflectComponent->PlaneMesh)
            {
                //Make mirror plane & mirror matrix
                const FTransform& TransformPlane = CaptureReflectComponent->PlaneMesh->GetTransform();
                FVector ViewActorDir(CaptureReflectComponent->PlaneMesh->GetActorForwardVector());
                ViewActorDir.Normalize();
                FVector ViewActorPos(TransformPlane.GetLocation());
                MirrorPlane = FPlane(ViewActorPos, ViewActorDir);
                MirrorMatrix = FMirrorMatrix(MirrorPlane);
 
                // Virtuos SHL: calculate mirror view from the player in mirror
                APlayerController* Controller = World->GetFirstPlayerController();
                if (Controller)
                {
                    // @Virtuos [Ding Yifei] BEGIN: Fix BMBUG-8944, stopping reflection update when player is behind the mirror.
                    FVector PlayerCameraDir(Controller->PlayerCameraManager->GetActorForwardVector());
                    FVector PlayerCameraPos(Controller->PlayerCameraManager->GetCameraLocation());
                    if (MirrorPlane.PlaneDot(PlayerCameraPos) < 0.0f)
                    {
                        return;
                    }
                    // @Virtuos END
                     
                    FMatrix playerTranformMatrix = Controller->PlayerCameraManager->GetTransform().ToMatrixWithScale();                  
                    FTransform mirrorPlayerTransform( playerTranformMatrix * MirrorMatrix );
                    ViewLocation = mirrorPlayerTransform.GetTranslation();                 
                    mirrorPlayerTransform.SetTranslation(FVector::ZeroVector);
                    ViewRotationMatrix = mirrorPlayerTransform.ToInverseMatrixWithScale();
                    CaptureComponent->FOVAngle = Controller->PlayerCameraManager->GetFOVAngle();
                }
                else
                {
                    return;
                }
 
#ifdef RS_OCEAN //@Virtuos [Ding Yifei]: Disable ported ocean
                //@Virtuos [Ding Yifei] Begin: Stop updating reflection capture
                bool IsMirrorNotRendering = false;
                bool IsOceanNotRendering = true;
 
                if (IsOceanCase)
                {
                    for (int32 pidx = 0; pidx < Primitives.Num(); ++pidx)
                    {
                        if (Primitives[pidx]->Proxy->Component->IsA(UROceanComponent::StaticClass()))
                        {
                            //Check whether ocean is in render when in ocean case
                            IsOceanNotRendering = Primitives[pidx]->Proxy->Component->GetAttachmentRootActor()->GetLastRenderTime() < GetWorld()->GetTimeSeconds() - 1;
 
                            break;
                        }
                    }
                }
 
                if (!IsOceanCase) //Check whether related mirror plane mesh is in render when not in ocean case
                    IsMirrorNotRendering = CaptureReflectComponent->PlaneMesh->GetLastRenderTime() < GetWorld()->GetTimeSeconds() - 1;
 
                if ((IsOceanCase && IsOceanNotRendering) || (!IsOceanCase && IsMirrorNotRendering))
                {
                    return;
                }
                //@Virtuos [Ding Yifei] End
#else //Disable ported ocean
                //Stop capture when related mesh not in render.
                //if (CaptureReflectComponent->PlaneMesh->GetLastRenderTime() < GetWorld()->GetTimeSeconds() - 1)
                //{
                //  return;
                //}
#endif //Disable ported ocean
 
            }
        }
//VIRTUOS [shenmo] End: Implement SceneCaptureReflect
 
        // swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
        ViewRotationMatrix = ViewRotationMatrix * FMatrix(
            FPlane(0,   0,  1,  0),
            FPlane(1,   0,  0,  0),
            FPlane(0,   1,  0,  0),
            FPlane(0,   0,  0,  1));
        const float FOV = CaptureComponent->FOVAngle * (float)PI / 360.0f;
        const bool bUseSceneColorTexture = CaptureComponent->CaptureSource == SCS_SceneColorHDR;
 
//VIRTUOS [shenmo] Begin: Calculate mirror clip plane
        FPlane MirrorPlaneViewSpace = FPlane(0,0,0,0);
        if (MirrorPlane != FPlane(0,0,0,0))
        {
            FMatrix viewMatrix = FTranslationMatrix(-ViewLocation) * ViewRotationMatrix;
 
            // transform the clip plane so that it is in view space
            MirrorPlaneViewSpace = MirrorPlane.TransformBy(viewMatrix);
        }
//VIRTUOS [shenmo] END:
 
        FSceneRenderer* SceneRenderer = CreateSceneRenderer(CaptureComponent, CaptureComponent->TextureTarget, ViewRotationMatrix , ViewLocation, FOV, CaptureComponent->MaxViewDistanceOverride, bUseSceneColorTexture, &CaptureComponent->PostProcessSettings, CaptureComponent->PostProcessBlendWeight, MirrorPlaneViewSpace, bMirror
#ifdef RS_OCEAN //@Virtuos [Ding Yifei]: Disable ported ocean
            , IsOceanCase
#endif //Disable ported ocean
            );
 
        FTextureRenderTargetResource* TextureRenderTarget = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
        const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;
 
        ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER(
            CaptureCommand,
            FSceneRenderer*, SceneRenderer, SceneRenderer,
            FTextureRenderTargetResource*, TextureRenderTarget, TextureRenderTarget,
            FName, OwnerName, OwnerName,
            bool, bUseSceneColorTexture, bUseSceneColorTexture,
            bool, bMirror, bMirror,
        {
            UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, OwnerName, FResolveParams(), bUseSceneColorTexture, bMirror);
        });
    }
}
 
void FScene::UpdateSceneCaptureContents(USceneCaptureComponentCube* CaptureComponent)
{
    struct FLocal
    {
        /** Creates a transformation for a cubemap face, following the D3D cubemap layout. */
        static FMatrix CalcCubeFaceTransform(ECubeFace Face)
        {
            static const FVector XAxis(1.f, 0.f, 0.f);
            static const FVector YAxis(0.f, 1.f, 0.f);
            static const FVector ZAxis(0.f, 0.f, 1.f);
 
            // vectors we will need for our basis
            FVector vUp(YAxis);
            FVector vDir;
            switch (Face)
            {
                case CubeFace_PosX:
                    vDir = XAxis;
                    break;
                case CubeFace_NegX:
                    vDir = -XAxis;
                    break;
                case CubeFace_PosY:
                    vUp = -ZAxis;
                    vDir = YAxis;
                    break;
                case CubeFace_NegY:
                    vUp = ZAxis;
                    vDir = -YAxis;
                    break;
                case CubeFace_PosZ:
                    vDir = ZAxis;
                    break;
                case CubeFace_NegZ:
                    vDir = -ZAxis;
                    break;
            }
            // derive right vector
            FVector vRight(vUp ^ vDir);
            // create matrix from the 3 axes
            return FBasisVectorMatrix(vRight, vUp, vDir, FVector::ZeroVector);
        }
    } ;
 
    check(CaptureComponent);
 
    if (GetFeatureLevel() >= ERHIFeatureLevel::SM4 && CaptureComponent->TextureTarget)
    {
        const float FOV = 90 * (float)PI / 360.0f;
        for (int32 faceidx = 0; faceidx < (int32)ECubeFace::CubeFace_MAX; faceidx++)
        {
            const ECubeFace TargetFace = (ECubeFace)faceidx;
            const FVector Location = CaptureComponent->GetComponentToWorld().GetTranslation();
            const FMatrix ViewRotationMatrix = FLocal::CalcCubeFaceTransform(TargetFace);
            FSceneRenderer* SceneRenderer = CreateSceneRenderer(CaptureComponent, CaptureComponent->TextureTarget, ViewRotationMatrix, Location, FOV, CaptureComponent->MaxViewDistanceOverride);
 
            FTextureRenderTargetCubeResource* TextureRenderTarget = static_cast<FTextureRenderTargetCubeResource*>(CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource());
            const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;
 
            ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
                CaptureCommand,
                FSceneRenderer*, SceneRenderer, SceneRenderer,
                FTextureRenderTargetCubeResource*, TextureRenderTarget, TextureRenderTarget,
                FName, OwnerName, OwnerName,
                ECubeFace, TargetFace, TargetFace,
            {
                UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, OwnerName, FResolveParams(FResolveRect(), TargetFace), true, false);
            });
        }
    }
}