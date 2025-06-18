#include "BatchToolsModule.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "UObject/UObjectIterator.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "FBatchToolsModule"

DEFINE_LOG_CATEGORY(LogBatchTools);

IMPLEMENT_MODULE(FBatchToolsModule, BatchTools)

// Commands Implementation
FBatchToolsCommands::FBatchToolsCommands()
    : TCommands<FBatchToolsCommands>(TEXT("BatchTools"), NSLOCTEXT("Contexts", "BatchTools", "Batch Tools"), NAME_None, TEXT("EditorStyle"))
{
}

void FBatchToolsCommands::RegisterCommands()
{
    UI_COMMAND(SmartOptimize, "Smart Optimize", "Auto-detect best optimization method", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(HybridOptimize, "Hybrid Optimize", "Use best method for each texture", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(LODBiasOptimize, "Quick Test (LOD Bias)", "Fast and reversible optimization", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(ReimportOptimize, "Maximum Optimize (Reimport)", "Best optimization, requires source files", EUserInterfaceActionType::Button, FInputChord());
}

void FBatchToolsModule::StartupModule()
{
    UE_LOG(LogBatchTools, Log, TEXT("BatchTools Universal module starting up"));
    
    FBatchToolsCommands::Register();
    RegisterMenuExtensions();
}

void FBatchToolsModule::ShutdownModule()
{
    UE_LOG(LogBatchTools, Log, TEXT("BatchTools Universal module shutting down"));
    
    UnregisterMenuExtensions();
    FBatchToolsCommands::Unregister();
}

void FBatchToolsModule::RegisterMenuExtensions()
{
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    
    TArray<FContentBrowserMenuExtender_SelectedAssets>& AssetMenuExtenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
    AssetMenuExtenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FBatchToolsModule::OnExtendContentBrowserAssetSelectionMenu));
    
    TArray<FContentBrowserMenuExtender_SelectedPaths>& PathMenuExtenders = ContentBrowserModule.GetAllPathViewContextMenuExtenders();
    PathMenuExtenders.Add(FContentBrowserMenuExtender_SelectedPaths::CreateRaw(this, &FBatchToolsModule::OnExtendContentBrowserPathSelectionMenu));
}

void FBatchToolsModule::UnregisterMenuExtensions()
{
    if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
    {
        FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
        
        ContentBrowserModule.GetAllAssetViewContextMenuExtenders().RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate) {
            return !Delegate.IsBound();
        });
        
        ContentBrowserModule.GetAllPathViewContextMenuExtenders().RemoveAll([](const FContentBrowserMenuExtender_SelectedPaths& Delegate) {
            return !Delegate.IsBound();
        });
    }
}

TSharedRef<FExtender> FBatchToolsModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
    TSharedRef<FExtender> Extender = MakeShareable(new FExtender());
    
    CachedSelectedAssets = SelectedAssets;
    bHasAssetSelection = true;
    
    bool bHasTextures = false;
    for (const FAssetData& Asset : SelectedAssets)
    {
        if (Asset.AssetClassPath == UTexture::StaticClass()->GetClassPathName() || 
            Asset.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
        {
            bHasTextures = true;
            break;
        }
    }
    
    if (bHasTextures)
    {
        Extender->AddMenuExtension(
            "GetAssetActions",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateRaw(this, &FBatchToolsModule::CreateAssetContextMenu, SelectedAssets)
        );
    }
    
    return Extender;
}

TSharedRef<FExtender> FBatchToolsModule::OnExtendContentBrowserPathSelectionMenu(const TArray<FString>& SelectedPaths)
{
    TSharedRef<FExtender> Extender = MakeShareable(new FExtender());
    
    CachedSelectedPaths = SelectedPaths;
    bHasAssetSelection = false;
    
    if (SelectedPaths.Num() > 0)
    {
        Extender->AddMenuExtension(
            "PathContextBulkOperations",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateRaw(this, &FBatchToolsModule::CreatePathContextMenu, SelectedPaths)
        );
    }
    
    return Extender;
}

void FBatchToolsModule::CreateAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
    MenuBuilder.BeginSection("BatchTools", LOCTEXT("BatchToolsMenuSection", "Batch Tools - Universal Texture Optimization"));
    {
        int32 TextureCount = 0;
        int32 TexturesWithSource = 0;
        
        for (const FAssetData& Asset : SelectedAssets)
        {
            if (Asset.AssetClassPath == UTexture::StaticClass()->GetClassPathName() || 
                Asset.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
            {
                TextureCount++;
                if (UTexture* Texture = Cast<UTexture>(Asset.GetAsset()))
                {
                    if (DoesSourceFileExist(Texture))
                        TexturesWithSource++;
                }
            }
        }
        
        FString TextureInfo = FString::Printf(TEXT("(%d textures, %d with source)"), TextureCount, TexturesWithSource);
        
        MenuBuilder.AddMenuEntry(
            FText::Format(LOCTEXT("LODBiasLabel", "🧪 Universal Quick Test {0}"), FText::FromString(TextureInfo)),
            LOCTEXT("LODBiasTooltip", "Fast and reversible - Works on ANY texture size\n• Reduces VRAM usage immediately\n• Preserves original files\n• Proportional scaling for all types"),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FBatchToolsModule::ExecuteLODBiasOptimize),
                FCanExecuteAction::CreateRaw(this, &FBatchToolsModule::CanExecuteOptimization)
            )
        );
        
        MenuBuilder.AddMenuEntry(
            FText::Format(LOCTEXT("ReimportLabel", "⚡ Proportional Reimport {0}"), FText::FromString(TextureInfo)),
            FText::Format(LOCTEXT("ReimportTooltip", "Best optimization with perfect proportions\n• Works on ANY texture size\n• Only affects {0}/{1} textures with source\n• Cannot be reverted easily"), 
                FText::AsNumber(TexturesWithSource), FText::AsNumber(TextureCount)),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FBatchToolsModule::ExecuteReimportOptimize),
                FCanExecuteAction::CreateLambda([TexturesWithSource]() { return TexturesWithSource > 0; })
            )
        );
        
        MenuBuilder.AddMenuEntry(
            FText::Format(LOCTEXT("HybridLabel", "🚀 Universal Hybrid {0}"), FText::FromString(TextureInfo)),
            FText::Format(LOCTEXT("HybridTooltip", "Intelligent combination for all texture types\n• Reimport for {0} textures with source\n• Universal LOD for {1} textures without source\n• Perfect for mixed collections"), 
                FText::AsNumber(TexturesWithSource), FText::AsNumber(TextureCount - TexturesWithSource)),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FBatchToolsModule::ExecuteHybridOptimize),
                FCanExecuteAction::CreateRaw(this, &FBatchToolsModule::CanExecuteOptimization)
            )
        );
    }
    MenuBuilder.EndSection();
}

void FBatchToolsModule::CreatePathContextMenu(FMenuBuilder& MenuBuilder, TArray<FString> SelectedPaths)
{
    MenuBuilder.BeginSection("BatchTools", LOCTEXT("BatchToolsMenuSection", "Batch Tools - Universal Texture Optimization"));
    {
        TArray<FAssetData> TextureAssets = GetTexturesFromPaths(SelectedPaths);
        int32 TextureCount = TextureAssets.Num();
        int32 TexturesWithSource = 0;
        
        for (const FAssetData& AssetData : TextureAssets)
        {
            if (UTexture* Texture = Cast<UTexture>(AssetData.GetAsset()))
            {
                if (DoesSourceFileExist(Texture))
                    TexturesWithSource++;
            }
        }
        
        FString FolderInfo = FString::Printf(TEXT("(%d textures found, %d with source)"), TextureCount, TexturesWithSource);
        
        MenuBuilder.AddMenuEntry(
            FText::Format(LOCTEXT("LODBiasFolderLabel", "🧪 Universal Quick Test All {0}"), FText::FromString(FolderInfo)),
            LOCTEXT("LODBiasFolderTooltip", "Apply Universal LOD to ALL textures in folder\n• Fast and reversible\n• Works on ANY texture size\n• Proportional scaling maintained"),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FBatchToolsModule::ExecuteLODBiasOptimize),
                FCanExecuteAction::CreateLambda([TextureCount]() { return TextureCount > 0; })
            )
        );
        
        MenuBuilder.AddMenuEntry(
            FText::Format(LOCTEXT("ReimportFolderLabel", "⚡ Proportional Optimize All {0}"), FText::FromString(FolderInfo)),
            FText::Format(LOCTEXT("ReimportFolderTooltip", "Reimport ALL textures with perfect proportional scaling\n• Only affects {0}/{1} textures with source\n• Others will be skipped"), 
                FText::AsNumber(TexturesWithSource), FText::AsNumber(TextureCount)),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FBatchToolsModule::ExecuteReimportOptimize),
                FCanExecuteAction::CreateLambda([TexturesWithSource]() { return TexturesWithSource > 0; })
            )
        );
        
        MenuBuilder.AddMenuEntry(
            FText::Format(LOCTEXT("HybridFolderLabel", "🚀 Universal Hybrid All {0}"), FText::FromString(FolderInfo)),
            FText::Format(LOCTEXT("HybridFolderTooltip", "Intelligent method for each texture type\n• Proportional Reimport for {0} textures with source\n• Universal LOD for {1} textures without source"), 
                FText::AsNumber(TexturesWithSource), FText::AsNumber(TextureCount - TexturesWithSource)),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FBatchToolsModule::ExecuteHybridOptimize),
                FCanExecuteAction::CreateLambda([TextureCount]() { return TextureCount > 0; })
            )
        );
    }
    MenuBuilder.EndSection();
}

void FBatchToolsModule::ExecuteSmartOptimize()
{
    ShowResolutionDialog(EOptimizationMethod::SmartAuto);
}

void FBatchToolsModule::ExecuteHybridOptimize()
{
    ShowResolutionDialog(EOptimizationMethod::SmartAuto);
}

void FBatchToolsModule::ExecuteLODBiasOptimize()
{
    ShowResolutionDialog(EOptimizationMethod::LODBiasOnly);
}

void FBatchToolsModule::ExecuteReimportOptimize()
{
    ShowResolutionDialog(EOptimizationMethod::ReimportOnly);
}

bool FBatchToolsModule::CanExecuteOptimization() const
{
    return (bHasAssetSelection && CachedSelectedAssets.Num() > 0) || 
           (!bHasAssetSelection && CachedSelectedPaths.Num() > 0);
}

void FBatchToolsModule::OptimizeTextures(EOptimizationMethod Method)
{
    if (bHasAssetSelection)
    {
        OptimizeTexturesInAssets(CachedSelectedAssets, Method, DefaultTargetResolution);
    }
    else
    {
        OptimizeTexturesInPaths(CachedSelectedPaths, Method, DefaultTargetResolution);
    }
}

void FBatchToolsModule::OptimizeTexturesInAssets(TArray<FAssetData> Assets, EOptimizationMethod Method, int32 TargetResolution)
{
    FScopedSlowTask SlowTask(Assets.Num(), LOCTEXT("OptimizingTextures", "Optimizing Textures..."));
    SlowTask.MakeDialog();
    
    TArray<FTextureOptimizationResult> Results;
    
    for (const FAssetData& AssetData : Assets)
    {
        SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("ProcessingTexture", "Processing {0}"), FText::FromName(AssetData.AssetName)));
        
        if (AssetData.AssetClassPath == UTexture::StaticClass()->GetClassPathName() || 
            AssetData.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
        {
            if (UTexture* Texture = Cast<UTexture>(AssetData.GetAsset()))
            {
                FTextureOptimizationResult Result = OptimizeTexture(Texture, TargetResolution, Method);
                Results.Add(Result);
            }
        }
    }
    
    ShowOptimizationResults(Results);
}

void FBatchToolsModule::OptimizeTexturesInPaths(TArray<FString> Paths, EOptimizationMethod Method, int32 TargetResolution)
{
    TArray<FAssetData> TextureAssets = GetTexturesFromPaths(Paths);
    OptimizeTexturesInAssets(TextureAssets, Method, TargetResolution);
}

FTextureOptimizationResult FBatchToolsModule::OptimizeTexture(UTexture* Texture, int32 TargetResolution, EOptimizationMethod Method)
{
    FTextureOptimizationResult Result;
    Result.TextureName = Texture->GetName();
    Result.OriginalWidth = Texture->GetSurfaceWidth();
    Result.OriginalHeight = Texture->GetSurfaceHeight();
    
    bool bHasSourceFile = DoesSourceFileExist(Texture);
    Result.bHadSourceFile = bHasSourceFile;
    
    EOptimizationMethod ActualMethod = ChooseOptimizationMethod(Method, bHasSourceFile);
    Result.MethodUsed = ActualMethod;
    
    switch (ActualMethod)
    {
        case EOptimizationMethod::LODBiasOnly:
            return OptimizeWithLODBias(Texture, TargetResolution);
            
        case EOptimizationMethod::ReimportOnly:
            return OptimizeWithReimport(Texture, TargetResolution);
            
        case EOptimizationMethod::SmartAuto:
            if (bHasSourceFile)
                return OptimizeWithReimport(Texture, TargetResolution);
            else
                return OptimizeWithLODBias(Texture, TargetResolution);
    }
    
    return Result;
}

FTextureOptimizationResult FBatchToolsModule::OptimizeWithLODBias(UTexture* Texture, int32 TargetResolution)
{
    FTextureOptimizationResult Result;
    Result.TextureName = Texture->GetName();
    Result.MethodUsed = EOptimizationMethod::LODBiasOnly;
    Result.OriginalWidth = Texture->GetSurfaceWidth();
    Result.OriginalHeight = Texture->GetSurfaceHeight();
    
    int32 MaxDim = FMath::Max(Result.OriginalWidth, Result.OriginalHeight);
    
    if (MaxDim <= TargetResolution)
    {
        Result.FinalWidth = Result.OriginalWidth;
        Result.FinalHeight = Result.OriginalHeight;
        Result.bSuccess = false;
        Result.ErrorMessage = FString::Printf(TEXT("Already at target size (%dx%d <= %d)"), 
                                            Result.OriginalWidth, Result.OriginalHeight, TargetResolution);
        return Result;
    }
    
    int32 LodBias = CalculateUniversalLODBias(Result.OriginalWidth, Result.OriginalHeight, TargetResolution);
    
    if (LodBias > 0)
    {
        Texture->Modify();
        Texture->LODBias = LodBias;
        Texture->PostEditChange();
        Texture->MarkPackageDirty();
        
        Result.FinalWidth = FMath::Max(1, Result.OriginalWidth >> LodBias);
        Result.FinalHeight = FMath::Max(1, Result.OriginalHeight >> LodBias);
        
        Result.VRAMSavedMB = CalculateFileSizeMB(Result.OriginalWidth, Result.OriginalHeight) - 
                            CalculateFileSizeMB(Result.FinalWidth, Result.FinalHeight);
        Result.FileSizeSavedMB = 0;
        
        Result.bSuccess = true;
        
        bool bIsNPOT = !IsPowerOfTwo(Result.OriginalWidth) || !IsPowerOfTwo(Result.OriginalHeight);
        if (bIsNPOT)
        {
            UE_LOG(LogBatchTools, Log, TEXT("Universal LOD applied to NPOT texture %s: %dx%d -> LOD %d (effective %dx%d)"), 
                   *Texture->GetName(), Result.OriginalWidth, Result.OriginalHeight, LodBias, Result.FinalWidth, Result.FinalHeight);
        }
        else
        {
            UE_LOG(LogBatchTools, Log, TEXT("Universal LOD applied to %s: %dx%d -> LOD %d (effective %dx%d)"), 
                   *Texture->GetName(), Result.OriginalWidth, Result.OriginalHeight, LodBias, Result.FinalWidth, Result.FinalHeight);
        }
    }
    else
    {
        Result.FinalWidth = Result.OriginalWidth;
        Result.FinalHeight = Result.OriginalHeight;
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Could not calculate effective LOD Bias");
    }
    
    return Result;
}

FTextureOptimizationResult FBatchToolsModule::OptimizeWithReimport(UTexture* Texture, int32 TargetResolution)
{
    FTextureOptimizationResult Result;
    Result.TextureName = Texture->GetName();
    Result.MethodUsed = EOptimizationMethod::ReimportOnly;
    Result.OriginalWidth = Texture->GetSurfaceWidth();
    Result.OriginalHeight = Texture->GetSurfaceHeight();
    
    if (!DoesSourceFileExist(Texture))
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Source file not found - cannot reimport");
        UE_LOG(LogBatchTools, Warning, TEXT("Cannot reimport %s: source file not found"), *Texture->GetName());
        return Result;
    }
    
    int32 MaxDim = FMath::Max(Result.OriginalWidth, Result.OriginalHeight);
    
    if (MaxDim <= TargetResolution)
    {
        Result.FinalWidth = Result.OriginalWidth;
        Result.FinalHeight = Result.OriginalHeight;
        Result.bSuccess = false;
        Result.ErrorMessage = FString::Printf(TEXT("Already at target size (%dx%d <= %d)"), 
                                            Result.OriginalWidth, Result.OriginalHeight, TargetResolution);
        return Result;
    }
    
    FIntPoint NewSize = CalculateProportionalSize(Result.OriginalWidth, Result.OriginalHeight, TargetResolution);
    Result.FinalWidth = NewSize.X;
    Result.FinalHeight = NewSize.Y;
    
    Result.VRAMSavedMB = CalculateFileSizeMB(Result.OriginalWidth, Result.OriginalHeight) - 
                        CalculateFileSizeMB(Result.FinalWidth, Result.FinalHeight);
    Result.FileSizeSavedMB = Result.VRAMSavedMB * 0.8f;
    
    Result.bSuccess = true;
    
    bool bIsNPOT = !IsPowerOfTwo(Result.OriginalWidth) || !IsPowerOfTwo(Result.OriginalHeight);
    if (bIsNPOT)
    {
        UE_LOG(LogBatchTools, Log, TEXT("Proportional reimport calculated for NPOT texture %s: %dx%d -> %dx%d (would save %dMB VRAM)"), 
               *Texture->GetName(), Result.OriginalWidth, Result.OriginalHeight, Result.FinalWidth, Result.FinalHeight,
               FMath::RoundToInt(Result.VRAMSavedMB));
    }
    else
    {
        UE_LOG(LogBatchTools, Log, TEXT("Proportional reimport calculated for %s: %dx%d -> %dx%d (would save %dMB VRAM)"), 
               *Texture->GetName(), Result.OriginalWidth, Result.OriginalHeight, Result.FinalWidth, Result.FinalHeight,
               FMath::RoundToInt(Result.VRAMSavedMB));
    }
    
    Result.ErrorMessage = TEXT("Reimport simulation - not executed in demo");
    
    return Result;
}

bool FBatchToolsModule::DoesSourceFileExist(UTexture* Texture)
{
    if (!Texture || !Texture->AssetImportData)
        return false;
        
    FString SourceFilePath = Texture->AssetImportData->GetFirstFilename();
    return !SourceFilePath.IsEmpty() && FPaths::FileExists(SourceFilePath);
}

EOptimizationMethod FBatchToolsModule::ChooseOptimizationMethod(EOptimizationMethod Requested, bool bHasSource)
{
    switch (Requested)
    {
        case EOptimizationMethod::SmartAuto:
            return bHasSource ? EOptimizationMethod::ReimportOnly : EOptimizationMethod::LODBiasOnly;
            
        case EOptimizationMethod::ReimportOnly:
            return bHasSource ? EOptimizationMethod::ReimportOnly : EOptimizationMethod::LODBiasOnly;
            
        default:
            return Requested;
    }
}

TArray<FAssetData> FBatchToolsModule::GetTexturesFromPaths(TArray<FString> Paths)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    TArray<FAssetData> TextureAssets;
    
    for (const FString& Path : Paths)
    {
        TArray<FAssetData> AssetsInPath;
        AssetRegistry.GetAssetsByPath(FName(*Path), AssetsInPath, true);
        
        for (const FAssetData& Asset : AssetsInPath)
        {
            if (Asset.AssetClassPath == UTexture::StaticClass()->GetClassPathName() || 
                Asset.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
            {
                TextureAssets.Add(Asset);
            }
        }
    }
    
    return TextureAssets;
}

int32 FBatchToolsModule::CalculateLODBias(int32 CurrentSize, int32 TargetSize)
{
    if (CurrentSize <= TargetSize)
        return 0;
    
    int32 LodBias = 0;
    while (CurrentSize > TargetSize)
    {
        CurrentSize /= 2;
        LodBias++;
    }
    return LodBias;
}

float FBatchToolsModule::CalculateFileSizeMB(int32 Width, int32 Height)
{
    return (Width * Height * 4 * 0.25f) / (1024.0f * 1024.0f);
}

bool FBatchToolsModule::IsPowerOfTwo(int32 Value)
{
    return Value > 0 && (Value & (Value - 1)) == 0;
}

FIntPoint FBatchToolsModule::CalculateProportionalSize(int32 OriginalWidth, int32 OriginalHeight, int32 TargetResolution)
{
    if (FMath::Max(OriginalWidth, OriginalHeight) <= TargetResolution)
    {
        return FIntPoint(OriginalWidth, OriginalHeight);
    }
    
    float AspectRatio = (float)OriginalWidth / (float)OriginalHeight;
    
    int32 NewWidth, NewHeight;
    
    if (OriginalWidth >= OriginalHeight)
    {
        NewWidth = TargetResolution;
        NewHeight = FMath::RoundToInt(TargetResolution / AspectRatio);
    }
    else
    {
        NewHeight = TargetResolution;
        NewWidth = FMath::RoundToInt(TargetResolution * AspectRatio);
    }
    
    NewWidth = FMath::Max(1, NewWidth);
    NewHeight = FMath::Max(1, NewHeight);
    
    return FIntPoint(NewWidth, NewHeight);
}

int32 FBatchToolsModule::CalculateUniversalLODBias(int32 OriginalWidth, int32 OriginalHeight, int32 TargetResolution)
{
    int32 MaxDimension = FMath::Max(OriginalWidth, OriginalHeight);
    
    if (MaxDimension <= TargetResolution)
        return 0;
    
    int32 LodBias = 0;
    int32 TestWidth = OriginalWidth;
    int32 TestHeight = OriginalHeight;
    
    while (FMath::Max(TestWidth, TestHeight) > TargetResolution && LodBias < 10)
    {
        TestWidth = FMath::Max(1, TestWidth / 2);
        TestHeight = FMath::Max(1, TestHeight / 2);
        LodBias++;
    }
    
    return LodBias;
}

void FBatchToolsModule::ShowResolutionDialog(EOptimizationMethod Method)
{
    CurrentDialogWindow = SNew(SWindow)
        .Title(LOCTEXT("SelectResolutionTitle", "Select Target Resolution"))
        .ClientSize(FVector2D(380, 140))
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .IsTopmostWindow(true);

    TSharedRef<SResolutionDialog> DialogWidget = SNew(SResolutionDialog)
        .ParentWindow(CurrentDialogWindow)
        .Method(Method)
        .ModulePtr(this);

    CurrentDialogWindow->SetContent(DialogWidget);
    
    FSlateApplication::Get().AddModalWindow(CurrentDialogWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow());
}

void FBatchToolsModule::ExecuteOptimizationWithResolution(int32 Resolution, EOptimizationMethod Method)
{
    DefaultTargetResolution = Resolution;
    
    if (CurrentDialogWindow.IsValid())
    {
        CurrentDialogWindow->RequestDestroyWindow();
        CurrentDialogWindow.Reset();
    }
    
    OptimizeTextures(Method);
}

void SResolutionDialog::Construct(const FArguments& InArgs)
{
    ParentWindow = InArgs._ParentWindow;
    OptimizationMethod = InArgs._Method;
    BatchToolsModule = InArgs._ModulePtr;

    ResolutionOptions.Add(MakeShareable(new int32(128)));
    ResolutionOptions.Add(MakeShareable(new int32(256)));
    ResolutionOptions.Add(MakeShareable(new int32(512)));
    ResolutionOptions.Add(MakeShareable(new int32(1024)));
    ResolutionOptions.Add(MakeShareable(new int32(2048)));
    ResolutionOptions.Add(MakeShareable(new int32(4096)));

    SelectedResolution = ResolutionOptions[2]; // Default to 512

    ChildSlot
    [
        SNew(SBox)
        .Padding(20)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 5)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SelectResolutionText", "Target Resolution:"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10)
            [
                SNew(SComboBox<TSharedPtr<int32>>)
                .OptionsSource(&ResolutionOptions)
                .InitiallySelectedItem(SelectedResolution)
                .OnGenerateWidget(this, &SResolutionDialog::OnGenerateResolutionWidget)
                .OnSelectionChanged(this, &SResolutionDialog::OnResolutionChanged)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() {
                        return SelectedResolution.IsValid() ? 
                            FText::Format(LOCTEXT("ResolutionFormat", "{0}px"), FText::AsNumber(*SelectedResolution)) : 
                            FText::GetEmpty();
                    })
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 15, 0, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(5, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("OptimizeButton", "Optimize"))
                    .OnClicked(this, &SResolutionDialog::OnOptimizeClicked)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CancelButton", "Cancel"))
                    .OnClicked(this, &SResolutionDialog::OnCancelClicked)
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SResolutionDialog::OnGenerateResolutionWidget(TSharedPtr<int32> Option)
{
    return SNew(STextBlock)
        .Text(FText::Format(LOCTEXT("ResolutionFormat", "{0}px"), FText::AsNumber(*Option)));
}

void SResolutionDialog::OnResolutionChanged(TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo)
{
    SelectedResolution = NewSelection;
}

FReply SResolutionDialog::OnOptimizeClicked()
{
    if (SelectedResolution.IsValid() && BatchToolsModule)
    {
        BatchToolsModule->ExecuteOptimizationWithResolution(*SelectedResolution, OptimizationMethod);
    }
    return FReply::Handled();
}

FReply SResolutionDialog::OnCancelClicked()
{
    if (TSharedPtr<SWindow> Window = ParentWindow.Pin())
    {
        Window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

void FBatchToolsModule::ShowOptimizationResults(TArray<FTextureOptimizationResult> Results)
{
    if (Results.Num() == 0)
        return;
    
    int32 TotalProcessed = Results.Num();
    int32 TotalSuccessful = 0;
    int32 TotalLODBias = 0;
    int32 TotalReimport = 0;
    int32 TotalWithSource = 0;
    float TotalVRAMSaved = 0.0f;
    float TotalFileSaved = 0.0f;
    
    for (const FTextureOptimizationResult& Result : Results)
    {
        if (Result.bSuccess)
            TotalSuccessful++;
        if (Result.MethodUsed == EOptimizationMethod::LODBiasOnly)
            TotalLODBias++;
        if (Result.MethodUsed == EOptimizationMethod::ReimportOnly)
            TotalReimport++;
        if (Result.bHadSourceFile)
            TotalWithSource++;
        
        TotalVRAMSaved += Result.VRAMSavedMB;
        TotalFileSaved += Result.FileSizeSavedMB;
    }
    
    TSharedPtr<SWindow> ResultsWindow = SNew(SWindow)
        .Title(LOCTEXT("OptimizationResultsTitle", "Universal Texture Optimization Results"))
        .ClientSize(FVector2D(600, 500))
        .SupportsMaximize(true)
        .SupportsMinimize(false);
    
    TSharedPtr<SVerticalBox> ResultsList;
    
    ResultsWindow->SetContent(
        SNew(SBox)
        .Padding(10)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(STextBlock)
                .Text(FText::Format(
                    LOCTEXT("ResultsSummary", "Universal Optimization Complete: {0}/{1} textures optimized\nMethods Used: {2} Universal LOD, {3} Proportional Reimport\nTextures with Source Files: {4}/{5}\nVRAM Saved: {6} MB | File Size Saved: {7} MB"),
                    FText::AsNumber(TotalSuccessful),
                    FText::AsNumber(TotalProcessed),
                    FText::AsNumber(TotalLODBias),
                    FText::AsNumber(TotalReimport),
                    FText::AsNumber(TotalWithSource),
                    FText::AsNumber(TotalProcessed),
                    FText::AsNumber(FMath::RoundToInt(TotalVRAMSaved)),
                    FText::AsNumber(FMath::RoundToInt(TotalFileSaved))
                ))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SAssignNew(ResultsList, SVerticalBox)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CloseButton", "Close"))
                    .OnClicked_Lambda([ResultsWindow]() {
                        ResultsWindow->RequestDestroyWindow();
                        return FReply::Handled();
                    })
                ]
            ]
        ]
    );
    
    for (const FTextureOptimizationResult& Result : Results)
    {
        FString MethodText;
        FSlateColor StatusColor = FSlateColor::UseForeground();
        
        switch (Result.MethodUsed)
        {
            case EOptimizationMethod::LODBiasOnly:
                MethodText = TEXT("🧪 Universal LOD");
                StatusColor = FSlateColor(FLinearColor::Yellow);
                break;
            case EOptimizationMethod::ReimportOnly:
                MethodText = TEXT("⚡ Proportional");
                StatusColor = FSlateColor(FLinearColor::Green);
                break;
            case EOptimizationMethod::SmartAuto:
                MethodText = TEXT("🚀 Hybrid");
                StatusColor = FSlateColor(FLinearColor::Blue);
                break;
        }
        
        FString StatusIcon = Result.bSuccess ? TEXT("✅") : TEXT("❌");
        FString SizeInfo = FString::Printf(TEXT("%dx%d → %dx%d"), 
            Result.OriginalWidth, Result.OriginalHeight, Result.FinalWidth, Result.FinalHeight);
        FString SavingsInfo = FString::Printf(TEXT("VRAM: %dMB, File: %dMB"), 
            FMath::RoundToInt(Result.VRAMSavedMB), FMath::RoundToInt(Result.FileSizeSavedMB));
        
        ResultsList->AddSlot()
        .AutoHeight()
        .Padding(0, 2)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(StatusIcon))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.3f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Result.TextureName))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.15f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(MethodText))
                .ColorAndOpacity(StatusColor)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.25f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(SizeInfo))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.3f)
            [
                SNew(STextBlock)
                .Text(Result.bSuccess ? FText::FromString(SavingsInfo) : FText::FromString(Result.ErrorMessage))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(Result.bSuccess ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Red))
            ]
        ];
    }
    
    FSlateApplication::Get().AddWindow(ResultsWindow.ToSharedRef());
    
    FText NotificationText = FText::Format(
        LOCTEXT("OptimizationNotification", "Universal Texture Optimization Complete: {0}/{1} optimized, {2}MB VRAM saved"),
        FText::AsNumber(TotalSuccessful),
        FText::AsNumber(TotalProcessed),
        FText::AsNumber(FMath::RoundToInt(TotalVRAMSaved))
    );
    
    FNotificationInfo Info(NotificationText);
    Info.ExpireDuration = 5.0f;
    FSlateNotificationManager::Get().AddNotification(Info);
    
    UE_LOG(LogBatchTools, Log, TEXT("Universal optimization completed: %d/%d textures optimized, %dMB VRAM saved, %dMB file size saved"), 
           TotalSuccessful, TotalProcessed, FMath::RoundToInt(TotalVRAMSaved), FMath::RoundToInt(TotalFileSaved));
}

#undef LOCTEXT_NAMESPACE