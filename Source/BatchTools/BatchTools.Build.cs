using UnrealBuildTool;

public class BatchTools : ModuleRules
{
    public BatchTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "ToolMenus",
            "EditorStyle",
            "EditorWidgets",
            "Slate",
            "SlateCore",
            "InputCore",
            "AssetTools",
            "ContentBrowser",
            "ContentBrowserData",
            "EditorSubsystem",
            "EditorScriptingUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DesktopPlatform",
            "PropertyEditor",
            "SharedSettingsWidgets",
            "SourceControl",
            "ToolWidgets",
            "WorkspaceMenuStructure",
            "AssetRegistry"
        });
    }
}