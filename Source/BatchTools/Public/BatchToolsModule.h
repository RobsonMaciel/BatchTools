#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/Commands.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "AssetRegistry/AssetData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBatchTools, Log, All);

// Forward declarations
class UTexture;
class FMenuBuilder;

// Optimization methods
enum class EOptimizationMethod : uint8
{
    LODBiasOnly,
    ReimportOnly,
    SmartAuto
};

// Result structure
struct FTextureOptimizationResult
{
    FString TextureName;
    int32 OriginalWidth = 0;
    int32 OriginalHeight = 0;
    int32 FinalWidth = 0;
    int32 FinalHeight = 0;
    float VRAMSavedMB = 0.0f;
    float FileSizeSavedMB = 0.0f;
    bool bSuccess = false;
    bool bHadSourceFile = false;
    EOptimizationMethod MethodUsed = EOptimizationMethod::LODBiasOnly;
    FString ErrorMessage;
};

// Custom widget para o dialog de resolução
class SResolutionDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SResolutionDialog) {}
        SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
        SLATE_ARGUMENT(EOptimizationMethod, Method)
        SLATE_ARGUMENT(class FBatchToolsModule*, ModulePtr)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TArray<TSharedPtr<int32>> ResolutionOptions;
    TSharedPtr<int32> SelectedResolution;
    TWeakPtr<SWindow> ParentWindow;
    EOptimizationMethod OptimizationMethod;
    FBatchToolsModule* BatchToolsModule;

    FReply OnOptimizeClicked();
    FReply OnCancelClicked();
    TSharedRef<SWidget> OnGenerateResolutionWidget(TSharedPtr<int32> Option);
    void OnResolutionChanged(TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo);
};

// Commands
class FBatchToolsCommands : public TCommands<FBatchToolsCommands>
{
public:
    FBatchToolsCommands();

    virtual void RegisterCommands() override;

public:
    TSharedPtr<FUICommandInfo> SmartOptimize;
    TSharedPtr<FUICommandInfo> HybridOptimize;
    TSharedPtr<FUICommandInfo> LODBiasOptimize;
    TSharedPtr<FUICommandInfo> ReimportOptimize;
};

// Main module class
class FBatchToolsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    // Menu extension functions
    void RegisterMenuExtensions();
    void UnregisterMenuExtensions();
    TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
    TSharedRef<FExtender> OnExtendContentBrowserPathSelectionMenu(const TArray<FString>& SelectedPaths);
    void CreateAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
    void CreatePathContextMenu(FMenuBuilder& MenuBuilder, TArray<FString> SelectedPaths);

    // Execution functions
    void ExecuteSmartOptimize();
    void ExecuteHybridOptimize();
    void ExecuteLODBiasOptimize();
    void ExecuteReimportOptimize();
    bool CanExecuteOptimization() const;

    // Optimization functions
    void OptimizeTextures(EOptimizationMethod Method);
    void OptimizeTexturesInAssets(TArray<FAssetData> Assets, EOptimizationMethod Method, int32 TargetResolution);
    void OptimizeTexturesInPaths(TArray<FString> Paths, EOptimizationMethod Method, int32 TargetResolution);
    FTextureOptimizationResult OptimizeTexture(UTexture* Texture, int32 TargetResolution, EOptimizationMethod Method);
    FTextureOptimizationResult OptimizeWithLODBias(UTexture* Texture, int32 TargetResolution);
    FTextureOptimizationResult OptimizeWithReimport(UTexture* Texture, int32 TargetResolution);
    
    // Helper functions
    int32 CalculateLODBias(int32 CurrentSize, int32 TargetSize);
    float CalculateFileSizeMB(int32 Width, int32 Height);
    bool IsPowerOfTwo(int32 Value);
    FIntPoint CalculateProportionalSize(int32 OriginalWidth, int32 OriginalHeight, int32 TargetResolution);
    int32 CalculateUniversalLODBias(int32 OriginalWidth, int32 OriginalHeight, int32 TargetResolution);

    // Utility functions
    TArray<FAssetData> GetTexturesFromPaths(TArray<FString> Paths);
    bool DoesSourceFileExist(UTexture* Texture);
    EOptimizationMethod ChooseOptimizationMethod(EOptimizationMethod Requested, bool bHasSource);
    void ShowResolutionDialog(EOptimizationMethod Method);
    void ShowOptimizationResults(TArray<FTextureOptimizationResult> Results);

    // State variables
    TArray<FAssetData> CachedSelectedAssets;
    TArray<FString> CachedSelectedPaths;
    bool bHasAssetSelection = false;
    int32 DefaultTargetResolution = 512;
    
    // Dialog state
    TSharedPtr<SWindow> CurrentDialogWindow;

public:
    // Public para ser chamado pelo widget
    void ExecuteOptimizationWithResolution(int32 Resolution, EOptimizationMethod Method);
};