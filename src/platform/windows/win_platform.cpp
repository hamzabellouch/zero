#include "../app_scanner.h"
#include "../process_engine.h"
#include "../leftover_cleaner.h"

#if defined(_WIN32)

#include "../../app_manager.h"
#include "../../process_manager.h"
#include "../../cleaner.h"

namespace ZeroCore {

    class WinAppScanner : public IAppScanner {
    public:
        std::vector<AppInfo> EnumerateDesktopApps() override {
            std::vector<AppInfo> result;
            auto winApps = AppManager::EnumerateDesktopApps();
            for (const auto& w : winApps) {
                AppInfo a;
                a.displayName = w.displayName;
                a.publisher = w.publisher;
                a.installDate = w.installDate;
                a.installLocation = w.installLocation;
                a.estimatedSizeBytes = w.estimatedSizeBytes;
                a.sizeFormatted = w.sizeFormatted;
                a.uninstallString = w.uninstallString;
                a.show32BitTag = w.show32BitTag;
                a.type = AppType::Desktop;
                result.push_back(a);
            }
            return result;
        }

        std::vector<AppInfo> EnumerateStorePackages() override {
            std::vector<AppInfo> result;
            auto winApps = AppManager::EnumerateStoreApps();
            for (const auto& w : winApps) {
                AppInfo a;
                a.displayName = w.displayName;
                a.publisher = w.publisher;
                a.installDate = w.installDate;
                a.installLocation = w.installLocation;
                a.estimatedSizeBytes = w.estimatedSizeBytes;
                a.sizeFormatted = w.sizeFormatted;
                a.isStoreApp = true;
                a.type = AppType::SystemPackage;
                result.push_back(a);
            }
            return result;
        }

        bool LaunchUninstall(const AppInfo& app, std::string& errorMsg) override {
            InstalledApp winApp;
            winApp.displayName = app.displayName;
            winApp.uninstallString = app.uninstallString;
            winApp.isWindowsStoreApp = app.isStoreApp;
            std::wstring err;
            bool ok = false;
            if (app.isStoreApp) {
                ok = AppManager::UninstallStoreApp(winApp, err);
            } else {
                HANDLE hP = nullptr;
                ok = AppManager::LaunchStandardUninstall(winApp, nullptr, err, &hP);
                if (hP) CloseHandle(hP);
            }
            if (!ok) {
                errorMsg = std::string(err.begin(), err.end());
            }
            return ok;
        }

        bool ForceRemove(const AppInfo& app, std::string& errorMsg) override {
            InstalledApp winApp;
            winApp.displayName = app.displayName;
            winApp.uninstallString = app.uninstallString;
            std::wstring err;
            bool ok = AppManager::ForceRemoveRegistryEntry(winApp, err);
            if (!ok) {
                errorMsg = std::string(err.begin(), err.end());
            }
            return ok;
        }
    };

    class WinProcessEngine : public IProcessEngine {
    public:
        void UpdateSnapshot() override {
            m_mgr.UpdateSnapshot();
        }

        std::vector<ProcessInfo> GetProcesses() const override {
            std::vector<ProcessInfo> result;
            auto winProcs = m_mgr.GetProcesses();
            for (const auto& wp : winProcs) {
                ProcessInfo pi;
                pi.pid = wp.pid;
                for (DWORD d : wp.pids) pi.pids.push_back(d);
                pi.name = wp.name;
                pi.exeName = wp.exeName;
                pi.fullPath = wp.fullPath;
                pi.instanceCount = wp.instanceCount;
                pi.cpuPercent = wp.cpuPercent;
                pi.memoryBytes = wp.memoryBytes;
                pi.diskBytesPerSec = wp.diskBytesPerSec;
                pi.netBytesPerSec = wp.netBytesPerSec;
                pi.cpuText = wp.cpuText;
                pi.memoryText = wp.memoryText;
                pi.diskText = wp.diskText;
                pi.networkText = wp.networkText;
                result.push_back(pi);
            }
            return result;
        }

        SystemStats GetSystemStats() const override {
            SystemStats ss;
            auto ws = m_mgr.GetSystemStats();
            ss.totalCpuPercent = ws.totalCpuPercent;
            ss.totalMemoryPercent = ws.totalMemoryPercent;
            ss.totalDiskPercent = ws.totalDiskPercent;
            ss.totalNetPercent = ws.totalNetPercent;
            ss.totalPhysMemBytes = ws.totalPhysMemBytes;
            ss.usedPhysMemBytes = ws.usedPhysMemBytes;
            return ss;
        }

        bool TerminateProcessGroup(const ProcessInfo& proc) override {
            ProcessItem item;
            for (uint32_t p : proc.pids) item.pids.push_back(p);
            return m_mgr.TerminateProcessGroup(item);
        }

    private:
        ProcessManager m_mgr;
    };

    class WinLeftoverCleaner : public ILeftoverCleaner {
    public:
        std::vector<LeftoverItem> ScanLeftovers(const AppInfo& app) override {
            std::vector<LeftoverItem> result;
            InstalledApp winApp;
            winApp.displayName = app.displayName;
            winApp.installLocation = app.installLocation;
            auto winLeftovers = Cleaner::ScanLeftovers(winApp);
            for (const auto& w : winLeftovers) {
                LeftoverItem li;
                li.path = w.path;
                li.detail = w.detail;
                li.sizeBytes = w.sizeBytes;
                li.selected = w.selected;
                if (w.type == LeftoverType::File) li.type = ZeroCore::LeftoverType::File;
                else if (w.type == LeftoverType::Folder) li.type = ZeroCore::LeftoverType::Folder;
                else li.type = ZeroCore::LeftoverType::RegistryKey;
                result.push_back(li);
            }
            return result;
        }

        bool CleanItems(const std::vector<LeftoverItem>& items, std::string& errorMsg) override {
            std::vector<::LeftoverItem> winItems;
            for (const auto& i : items) {
                ::LeftoverItem wi;
                wi.path = i.path;
                wi.detail = i.detail;
                wi.sizeBytes = i.sizeBytes;
                wi.selected = i.selected;
                if (i.type == ZeroCore::LeftoverType::File) wi.type = LeftoverType::File;
                else if (i.type == ZeroCore::LeftoverType::Folder) wi.type = LeftoverType::Folder;
                else wi.type = LeftoverType::RegistryKey;
                winItems.push_back(wi);
            }
            int deleted = 0;
            return Cleaner::DeleteLeftovers(winItems, deleted);
        }
    };

    std::unique_ptr<IAppScanner> IAppScanner::CreatePlatformScanner() {
        return std::make_unique<WinAppScanner>();
    }

    std::unique_ptr<IProcessEngine> IProcessEngine::CreatePlatformEngine() {
        return std::make_unique<WinProcessEngine>();
    }

    std::unique_ptr<ILeftoverCleaner> ILeftoverCleaner::CreatePlatformCleaner() {
        return std::make_unique<WinLeftoverCleaner>();
    }

} // namespace ZeroCore

#endif
