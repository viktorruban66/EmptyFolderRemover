// remover.cpp
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <map>
#include <fnmatch.h>

using namespace std;
namespace fs = std::filesystem;

const string RESET = "\033[0m";
const string GREEN = "\033[92m";
const string RED = "\033[91m";
const string YELLOW = "\033[93m";
const string BLUE = "\033[94m";

string colorize(const string& text, const string& color) {
    return color + text + RESET;
}

bool isIgnored(const string& name, const vector<string>& ignoreList) {
    for (const auto& ig : ignoreList) {
        if (name == ig) return true;
    }
    return false;
}

bool isFolderEmpty(const fs::path& dir, const vector<string>& ignoreList, long long minSize) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) return false;
    vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(dir)) {
        entries.push_back(entry);
    }
    vector<fs::directory_entry> filtered;
    for (const auto& e : entries) {
        if (!isIgnored(e.path().filename().string(), ignoreList)) {
            filtered.push_back(e);
        }
    }
    if (filtered.empty()) return true;
    if (minSize > 0) {
        bool allSmall = true;
        for (const auto& e : filtered) {
            if (e.is_directory()) {
                allSmall = false;
                break;
            }
            if (e.file_size() >= (uintmax_t)minSize) {
                allSmall = false;
                break;
            }
        }
        if (allSmall) return true;
    }
    return false;
}

vector<fs::path> getEmptyDirs(const fs::path& root, bool recursive,
                              const vector<string>& exclude,
                              const vector<string>& ignoreList,
                              long long minSize) {
    vector<fs::path> result;
    function<void(const fs::path&)> walk = [&](const fs::path& dir) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                if (recursive) walk(entry.path());
                // Проверяем исключения
                bool skip = false;
                for (const auto& pat : exclude) {
                    if (fnmatch(pat.c_str(), entry.path().filename().c_str(), 0) == 0) {
                        skip = true;
                        break;
                    }
                }
                if (skip) continue;
                if (isFolderEmpty(entry.path(), ignoreList, minSize)) {
                    result.push_back(entry.path());
                }
            }
        }
    };
    walk(root);
    // Сортировка по глубине
    sort(result.begin(), result.end(), [](const fs::path& a, const fs::path& b) {
        return a.string().length() > b.string().length();
    });
    return result;
}

int main(int argc, char* argv[]) {
    string root = ".";
    bool recursive = true, dryRun = false, verbose = false, yes = false;
    string logFile, excludeStr, ignoreStr;
    long long minSize = 0;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-p" && i+1 < argc) root = argv[++i];
        else if (arg == "-r") recursive = true;
        else if (arg == "-n") dryRun = true;
        else if (arg == "-v") verbose = true;
        else if (arg == "-l" && i+1 < argc) logFile = argv[++i];
        else if (arg == "-e" && i+1 < argc) excludeStr = argv[++i];
        else if (arg == "-i" && i+1 < argc) ignoreStr = argv[++i];
        else if (arg == "-m" && i+1 < argc) minSize = stoll(argv[++i]);
        else if (arg == "-y") yes = true;
        else if (arg == "-h" || arg == "--help") {
            cout << "Usage: remover [options]\n"
                 << "  -p <dir>    Path (default .)\n"
                 << "  -r          Recursive (default on)\n"
                 << "  -n          Dry run\n"
                 << "  -v          Verbose\n"
                 << "  -l <file>   Log file\n"
                 << "  -e <pat>    Exclude (glob pattern)\n"
                 << "  -i <list>   Ignore files (comma)\n"
                 << "  -m <bytes>  Min size\n"
                 << "  -y          Yes (no prompt)\n";
            return 0;
        }
    }

    fs::path rootPath = fs::absolute(root);
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        cerr << colorize("Error: '" + root + "' is not a directory", RED) << endl;
        return 1;
    }

    vector<string> exclude;
    if (!excludeStr.empty()) {
        stringstream ss(excludeStr);
        string item;
        while (getline(ss, item, ',')) exclude.push_back(item);
    }
    vector<string> ignoreList;
    if (!ignoreStr.empty()) {
        stringstream ss(ignoreStr);
        string item;
        while (getline(ss, item, ',')) ignoreList.push_back(item);
    }
    // Добавляем системные
    vector<string> defaultIgnore = {".DS_Store", "Thumbs.db", "desktop.ini"};
    ignoreList.insert(ignoreList.end(), defaultIgnore.begin(), defaultIgnore.end());
    sort(ignoreList.begin(), ignoreList.end());
    ignoreList.erase(unique(ignoreList.begin(), ignoreList.end()), ignoreList.end());

    auto emptyDirs = getEmptyDirs(rootPath, recursive, exclude, ignoreList, minSize);
    if (emptyDirs.empty()) {
        cout << colorize("No empty folders found.", YELLOW) << endl;
        return 0;
    }

    cout << colorize("Found " + to_string(emptyDirs.size()) + " empty folders:", BLUE) << endl;
    if (verbose) {
        for (const auto& d : emptyDirs) {
            cout << "  " << d.string() << endl;
        }
    }

    if (dryRun) {
        cout << colorize("Dry run. Nothing deleted.", GREEN) << endl;
        return 0;
    }

    if (!yes) {
        cout << colorize("Delete " + to_string(emptyDirs.size()) + " folders? [y/N] ", YELLOW);
        string ans;
        getline(cin, ans);
        if (ans != "y" && ans != "Y") {
            cout << colorize("Operation cancelled.", RED) << endl;
            return 0;
        }
    }

    vector<string> logLines;
    int deleted = 0;
    for (const auto& d : emptyDirs) {
        if (verbose) cout << colorize("Deleting: " + d.string(), GREEN) << endl;
        try {
            fs::remove_all(d);
            deleted++;
            logLines.push_back(d.string());
        } catch (const exception& e) {
            cout << colorize("Error deleting " + d.string() + ": " + e.what(), RED) << endl;
        }
    }

    if (!logFile.empty()) {
        ofstream lf(logFile);
        if (lf) {
            for (const auto& line : logLines) lf << line << endl;
            cout << colorize("Log saved to " + logFile, GREEN) << endl;
        } else {
            cout << colorize("Error writing log", RED) << endl;
        }
    }

    cout << colorize("Deleted " + to_string(deleted) + " folders.", GREEN) << endl;
    return 0;
}
