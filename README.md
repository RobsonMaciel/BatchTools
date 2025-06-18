# BatchTools Universal - Unreal Engine Plugin

> **🚀 Universal Texture Optimization Plugin for Unreal Engine 5.5+**  
> Optimize ANY texture size with perfect proportional scaling - from Power-of-Two to NPOT textures!

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-blue?style=for-the-badge&logo=unrealengine)
![C++](https://img.shields.io/badge/C%2B%2B-17-red?style=for-the-badge&logo=cplusplus)
![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)

## ✨ Features

### 🎯 **Universal Compatibility**
- **Works with ANY texture size** - 3000x2308, 1024x1024, 512x256, etc.
- **Perfect proportional scaling** - Maintains aspect ratio automatically
- **NPOT (Non-Power-of-Two) support** - No more restrictions!
- **Real-time VRAM optimization** - See immediate memory savings

### 🧪 **Three Optimization Methods**

| Method | Description | Use Case | Reversible |
|--------|-------------|----------|------------|
| **🧪 Universal Quick Test** | LOD Bias optimization | Fast testing, any texture size | ✅ Yes |
| **⚡ Proportional Reimport** | Perfect proportional scaling | Best quality, requires source | ❌ No |
| **🚀 Universal Hybrid** | Smart combination of both | Mixed texture collections | Partial |

### 📊 **Smart Analytics**
- **Source file detection** - Automatically detects available source files
- **VRAM calculation** - Real memory savings estimation
- **Batch processing** - Handle hundreds of textures at once
- **Detailed reporting** - Complete optimization results

## 🚀 Quick Start

### Installation

1. **Download** the plugin files
2. **Extract** to your project's `Plugins/BatchTools/` folder
3. **Regenerate** project files
4. **Compile** your project
5. **Enable** the plugin in the Plugins menu

### Basic Usage

#### **Method 1: Right-click Assets**
1. Select textures in Content Browser
2. Right-click → **Batch Tools**
3. Choose your optimization method
4. Select target resolution (128px - 4096px)

#### **Method 2: Folder Processing**
1. Right-click on any folder
2. Select **Batch Tools** → **Universal Hybrid All**
3. Process all textures in folder automatically

## 📋 Detailed Guide

### 🎯 **Target Resolution Examples**

Your texture will be scaled proportionally to fit within the target resolution:

```
Original: 3000x2308 → Target: 512px
Result: 512x394 (perfect proportions!)

Original: 1024x1024 → Target: 512px  
Result: 512x512 (square maintained)

Original: 2048x512 → Target: 512px
Result: 512x128 (ratio preserved)
```

### 🧪 **Universal Quick Test (LOD Bias)**

**Best for:** Testing optimizations quickly

- ✅ **Instant** - No file modification
- ✅ **Reversible** - Set LOD Bias back to 0
- ✅ **Universal** - Works on ANY texture size
- ✅ **Safe** - Never damages original files

**How it works:**
```cpp
// For 3000x2308 → 512px target
LOD Bias 3 = 3000÷8 = 375x288 (effective size)
LOD Bias 2 = 3000÷4 = 750x577 
LOD Bias 1 = 3000÷2 = 1500x1154
```

### ⚡ **Proportional Reimport**

**Best for:** Maximum quality with perfect scaling

- ⚡ **Perfect scaling** - Exact proportional resize
- 📁 **Requires source** - Original image files needed
- 💾 **File size savings** - Reduces both VRAM and disk usage
- ⚠️ **Permanent** - Cannot be easily reverted

**Real Examples:**
```
3456x1024 → 512px = 512x148
3000x2308 → 1024px = 1024x787  
2560x1440 → 512px = 512x288
```

### 🚀 **Universal Hybrid (Recommended)**

**Best for:** Mixed texture collections

**Smart Logic:**
- Has source file? → **Proportional Reimport**
- No source file? → **Universal Quick Test**
- **Perfect for folders** with mixed texture types

## 🔧 Technical Details

### Supported Texture Types
- ✅ Texture2D
- ✅ Any resolution (POT and NPOT)
- ✅ All compression formats
- ✅ Source files optional

### File Structure
```
Plugins/BatchTools/
├── Source/
│   └── BatchTools/
│       ├── Public/
│       │   └── BatchToolsModule.h
│       ├── Private/
│       │   └── BatchToolsModule.cpp
│       └── BatchTools.Build.cs
├── BatchTools.uplugin
└── README.md
```

### Key Functions

```cpp
// Calculate perfect proportional size
FIntPoint CalculateProportionalSize(int32 Width, int32 Height, int32 Target);

// Universal LOD Bias calculation
int32 CalculateUniversalLODBias(int32 Width, int32 Height, int32 Target);

// Smart method selection
EOptimizationMethod ChooseOptimizationMethod(EOptimizationMethod Requested, bool bHasSource);
```

## 📊 Results Window

After optimization, you'll see a detailed report:

```
Universal Optimization Complete: 25/30 textures optimized
Methods Used: 15 Universal LOD, 10 Proportional Reimport
Textures with Source Files: 10/30
VRAM Saved: 156 MB | File Size Saved: 89 MB

✅ Texture_Wall_01     🧪 Universal LOD    2048x2048 → 512x512    VRAM: 12MB
✅ Texture_Floor_NPOT  ⚡ Proportional     3000x2308 → 512x394    VRAM: 8MB
❌ Texture_Missing     🧪 Universal LOD    Already at target size
```

## 🎯 Use Cases

### 🎮 **Game Development**
- **Mobile optimization** - Reduce VRAM for mobile devices
- **Level-of-detail** - Create LOD variants efficiently
- **Platform targeting** - Different resolutions per platform

### 🎨 **Asset Management**
- **Batch processing** - Handle large texture libraries
- **Quality testing** - Quick A/B testing of texture sizes
- **Memory profiling** - Find VRAM bottlenecks

### 🔧 **Technical Artists**
- **Pipeline automation** - Integrate into build processes
- **Quality control** - Standardize texture resolutions
- **Performance optimization** - Target specific memory budgets

## ⚙️ Configuration

### Plugin Settings
```cpp
// Default target resolution (can be changed per operation)
int32 DefaultTargetResolution = 512;

// Supported resolution options
TArray<int32> ResolutionOptions = {128, 256, 512, 1024, 2048, 4096};
```

### Custom Integration

Add to your build pipeline:
```cpp
// Get BatchTools module
FBatchToolsModule* BatchTools = FModuleManager::GetModulePtr<FBatchToolsModule>("BatchTools");

// Optimize specific textures
TArray<FAssetData> Textures = GetTexturesFromProject();
BatchTools->OptimizeTexturesInAssets(Textures, EOptimizationMethod::SmartAuto, 512);
```

## 🐛 Troubleshooting

### Common Issues

**❌ "Source file not found"**
- Solution: Use Universal Quick Test instead of Reimport
- Or: Check if source files are in correct import paths

**❌ "Already at target size"**
- Solution: Choose smaller target resolution
- Or: Texture is already optimized

**❌ "Compilation errors"**
- Solution: Ensure UE 5.5+ and regenerate project files
- Check that all dependencies are included

### Validation Steps

1. **Check LOD Bias:**
   - Open texture properties
   - Look for LOD Bias > 0

2. **Verify VRAM usage:**
   - Console command: `stat texture`
   - Compare before/after values

3. **Test in-game:**
   - Check visual quality
   - Monitor performance impact

## 🤝 Contributing

### Development Setup
1. Fork the repository
2. Create feature branch: `git checkout -b feature/amazing-feature`
3. Commit changes: `git commit -m 'Add amazing feature'`
4. Push to branch: `git push origin feature/amazing-feature`
5. Open Pull Request

### Code Style
- Follow Unreal Engine C++ coding standards
- Use meaningful variable names
- Add comprehensive comments
- Include unit tests for new features

## 📝 Changelog

### v1.0.0 (Latest)
- ✅ Universal texture size support
- ✅ Perfect proportional scaling
- ✅ NPOT texture compatibility  
- ✅ Smart hybrid optimization
- ✅ Comprehensive UI with results
- ✅ Real-time VRAM calculation

### Roadmap
- 🔲 Batch import settings
- 🔲 Custom compression options
- 🔲 Automation tools integration
- 🔲 Performance profiling tools

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Unreal Engine team for the robust plugin architecture
- Community feedback for NPOT texture support
- Beta testers for real-world validation

## 📞 Support

- **Wiki:** [GitHub Wiki](https://github.com/RobsonMaciel/BatchTools/wiki)
- **Issues:** [GitHub Issues](https://github.com/RobsonMaciel/BatchTools/issues)
- **Author:** [Robson Maciel](https://github.com/RobsonMaciel)
---

> **🎯 "Universal texture optimization that just works - for any size, any project, any workflow."**

**Made with ❤️ for the Unreal Engine community**
