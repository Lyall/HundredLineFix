#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "HundredLineFix";
std::string sFixVersion = "0.0.1";
std::filesystem::path sFixPath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;

// Aspect ratio / FOV / HUD
std::pair DesktopDimensions = { 0,0 };
const float fPi = 3.1415926535f;
const float fNativeAspect = 16.00f / 9.00f;
float fAspectRatio;
float fAspectMultiplier;
float fHUDWidth;
float fHUDWidthOffset;
float fHUDHeight;
float fHUDHeightOffset;

// Ini variables
bool bCustomResolution;
int iCustomResX;
int iCustomResY;
bool bFixAspect;
int iFramerateLimit;

// Variables
int iCurrentResX;
int iCurrentResY;

void CalculateAspectRatio(bool bLog)
{
    if (iCurrentResX <= 0 || iCurrentResY <= 0)
        return;

    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD 
    fHUDWidth = (float)iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2.00f;
    fHUDHeightOffset = 0.00f;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0.00f;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2.00f;
    }

    // Log details about current resolution
    if (bLog) {
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {:d}x{:d}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }
}

void Logging()
{
    // Get path to DLL
    WCHAR dllPath[_MAX_PATH] = {0};
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    sFixPath = dllPath;
    sFixPath = sFixPath.remove_filename();

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = {0};
    GetModuleFileNameW(exeModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // Spdlog initialisation
    try
    {
        // Truncate existing log file
        std::ofstream file(sExePath.string() + sLogFile, std::ios::trunc);
        if (file.is_open()) file.close();

        // Create single log file that's size-limited to 10MB
        logger = std::make_shared<spdlog::logger>(sFixName, std::make_shared<spdlog::sinks::rotating_file_sink_st>(sExePath.string() + sLogFile, 10 * 1024 * 1024, 1));
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::debug);

        #ifdef _DEBUG
        spdlog::set_level(spdlog::level::debug); 
        #endif 

        spdlog::info("----------");
        spdlog::info("{:s} v{:s} loaded.", sFixName, sFixVersion);
        spdlog::info("----------");
        spdlog::info("Log file: {}", sFixPath.string() + sLogFile);
        spdlog::info("----------");
        spdlog::info("Module Name: {:s}", sExeName);
        spdlog::info("Module Path: {:s}", sExePath.string());
        spdlog::info("Module Address: 0x{:x}", (uintptr_t)exeModule);
        spdlog::info("Module Timestamp: {:d}", Memory::ModuleTimestamp(exeModule));
        spdlog::info("----------");
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        AllocConsole();
        FILE *dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "Log initialisation failed: " << ex.what() << std::endl;
        FreeLibraryAndExitThread(thisModule, 1);
    }
}

void Configuration()
{
    // Inipp initialisation
    std::ifstream iniFile(sFixPath / sConfigFile);
    if (!iniFile)
    {
        AllocConsole();
        FILE *dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVersion.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sFixPath.string().c_str() << std::endl;
        spdlog::error("ERROR: Could not locate config file {}", sConfigFile);
        spdlog::shutdown();
        FreeLibraryAndExitThread(thisModule, 1);
    }
    else
    {
        spdlog::info("Config file: {}", sFixPath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();
    spdlog::info("----------");

    // Load settings from ini
    inipp::get_value(ini.sections["Custom Resolution"], "Enabled", bCustomResolution);
    inipp::get_value(ini.sections["Custom Resolution"], "Width", iCustomResX);
    inipp::get_value(ini.sections["Custom Resolution"], "Height", iCustomResY);
    inipp::get_value(ini.sections["Framerate Limit"], "FPS", iFramerateLimit);
    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);

    // Clamp settings
    iFramerateLimit = std::clamp(iFramerateLimit, 0, 1000);

    // Log ini parse
    spdlog_confparse(bCustomResolution);
    spdlog_confparse(iCustomResX);
    spdlog_confparse(iCustomResY);
    spdlog_confparse(iFramerateLimit);
    spdlog_confparse(bFixAspect);

    spdlog::info("----------");
}

void Resolution()
{
    if (bCustomResolution) 
    {
        // Grab desktop resolution
        DesktopDimensions = Util::GetPhysicalDesktopDimensions();

        // Set custom resolution as desktop resolution if set to 0 or invalid
        if (iCustomResX <= 0 || iCustomResY <= 0) {
            iCustomResX = DesktopDimensions.first;
            iCustomResY = DesktopDimensions.second;
        }
    }

    // Resolution
    std::uint8_t* ResolutionScanResult = Memory::PatternScan(exeModule, "45 0F ?? ?? 8B ?? 8B ?? 48 8B ?? ?? ?? 48 8B ?? ?? ?? 48 8B ?? ?? ?? 48 83 ?? ?? 41 ?? E9 ?? ?? ?? ??");
    if (ResolutionScanResult) {
        spdlog::info("Resolution: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        static SafetyHookMid ResolutionMidHook{};
        ResolutionMidHook = safetyhook::create_mid(ResolutionScanResult + 0x4,
            [](SafetyHookContext& ctx) {
                // Apply custom resolution
                if (bCustomResolution) {
                    ctx.rsi = iCustomResX;
                    ctx.rbx = iCustomResY;
                }

                // Get current resolution
                int iResX = static_cast<int>(ctx.rsi);
                int iResY = static_cast<int>(ctx.rbx);
  
                // Log current resolution
                if (iCurrentResX != iResX || iCurrentResY != iResY) {
                    iCurrentResX = iResX;
                    iCurrentResY = iResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else {
        spdlog::error("Resolution: Pattern scan failed.");
    }
}

void AspectRatio() 
{
    if (bFixAspect) {
        // Aspect ratio
        std::uint8_t* AspectRatioScanResult = Memory::PatternScan(exeModule, "0F 28 ?? F3 ?? ?? ?? ?? F3 ?? ?? ?? ?? F3 0F ?? ?? 0F 57 ?? F3 0F ?? ?? 0F ?? ?? F3 ?? ?? ?? ??");
        if (AspectRatioScanResult) {
            spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid AspectRatioMidHook{};
            AspectRatioMidHook = safetyhook::create_mid(AspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm8.f32[0] = ctx.xmm5.f32[0];
                });
        }
        else {
            spdlog::error("Aspect Ratio: Pattern scan failed.");
        }

        // Pillarboxing
        std::uint8_t* UIPlatePolygonScanResult = Memory::PatternScan(exeModule, "C6 ?? ?? 01 E8 ?? ?? ?? ?? 48 8B ?? 48 8B ?? 48 83 ?? ?? ?? 48 FF ?? ??");
        if (UIPlatePolygonScanResult) {
            spdlog::info("Pillarboxing: Address is {:s}+{:x}", sExeName.c_str(), UIPlatePolygonScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            Memory::PatchBytes(UIPlatePolygonScanResult + 0x3, "\x00", 1);
            spdlog::info("Pillarboxing: Patched instruction.");
        }
        else {
            spdlog::error("Pillarboxing: Pattern scan failed.");
        }
    }
}

void Framerate() 
{
    if (iFramerateLimit != 60)
    {
        // Framerate limit
        std::uint8_t* FramerateLimitScanResult = Memory::PatternScan(exeModule, "0F ?? ?? 89 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 88 ?? ?? ?? ?? ?? 48 83 ?? ?? ?? C3");
        if (FramerateLimitScanResult) {
            spdlog::info("Framerate Limit: Address is {:s}+{:x}", sExeName.c_str(), FramerateLimitScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid FramerateLimitMidHook{};
            FramerateLimitMidHook = safetyhook::create_mid(FramerateLimitScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rcx = iFramerateLimit;
                });
        }
        else {
            spdlog::error("Framerate Limit: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    Resolution();
    AspectRatio();
    Framerate();

    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        thisModule = hModule;

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
