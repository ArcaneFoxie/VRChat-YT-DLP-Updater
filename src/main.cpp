#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <windows.h>
#include <fstream>
#include <shlobj.h>
#include <vector>
#include <limits>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Forward declarations
std::string GetErrorMessage(DWORD errorCode);
bool RemoveReadOnlyAttribute(const std::string& filePath);
bool SetReadOnlyAttribute(const std::string& filePath);
void CheckAttributes(const std::string& path, const std::string& label);
std::string ReadVersionFile(const std::string& versionFilePath);
bool WriteVersionFile(const std::string& versionFilePath, const std::string& version);
bool DownloadFile(const std::string& url, const std::string& outputPath);
std::string GetVRChatToolsPath();
bool ConfigureYtDlp(const std::string& vrchatToolsPath);

// Callback function to handle CURL response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Function to get error message from error code
std::string GetErrorMessage(DWORD errorCode) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );
    
    if (size == 0 || messageBuffer == nullptr) {
        return "Unknown error";
    }
    
    std::string message(messageBuffer);
    LocalFree(messageBuffer);
    
    // Trim whitespace and newlines from the end
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) {
        message.pop_back();
    }
    
    return message;
}

// Function to check file attributes
void CheckAttributes(const std::string& path, const std::string& label) {
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Failed to get attributes for " << label << ": " << path << std::endl;
        return;
    }
    
    std::cout << "Attributes for " << label << " (" << path << "): ";
    if (attributes & FILE_ATTRIBUTE_READONLY) std::cout << "READONLY ";
    if (attributes & FILE_ATTRIBUTE_HIDDEN) std::cout << "HIDDEN ";
    if (attributes & FILE_ATTRIBUTE_SYSTEM) std::cout << "SYSTEM ";
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) std::cout << "DIRECTORY ";
    std::cout << std::endl;
}

// Function to read version from version file
std::string ReadVersionFile(const std::string& versionFilePath) {
    if (!fs::exists(versionFilePath)) {
        return "";
    }
    
    std::ifstream file(versionFilePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open version file: " << versionFilePath << std::endl;
        return "";
    }
    
    std::string version;
    std::getline(file, version);
    file.close();
    
    return version;
}

// Function to write version to version file
bool WriteVersionFile(const std::string& versionFilePath, const std::string& version) {
    std::ofstream file(versionFilePath);
    if (!file.is_open()) {
        std::cerr << "Failed to create version file: " << versionFilePath << std::endl;
        return false;
    }
    
    file << version;
    file.close();
    
    return true;
}

// Function to download a file
bool DownloadFile(const std::string& url, const std::string& outputPath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    // Create a temporary file path
    std::string tempPath = outputPath + ".tmp";
    
    FILE* fp = fopen(tempPath.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to open file for writing: " << tempPath << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    // Set up CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Verify SSL certificate
    
    // Add User-Agent header (required by GitHub API)
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: yt-dlp-updater");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Get the response code
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    // Get the content length
    curl_off_t contentLength = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    
    fclose(fp);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        fs::remove(tempPath);
        return false;
    }
    
    if (responseCode != 200) {
        std::cerr << "HTTP request failed with response code: " << responseCode << std::endl;
        fs::remove(tempPath);
        return false;
    }
    
    // Check if the file was downloaded successfully
    if (!fs::exists(tempPath)) {
        std::cerr << "Downloaded file does not exist: " << tempPath << std::endl;
        return false;
    }
    
    // Check file size
    uintmax_t fileSize = fs::file_size(tempPath);
    if (fileSize == 0) {
        std::cerr << "Downloaded file is empty (0 bytes): " << tempPath << std::endl;
        fs::remove(tempPath);
        return false;
    }
    
    std::cout << "Downloaded file size: " << fileSize << " bytes" << std::endl;
    
    // If we have content length info and it doesn't match, something went wrong
    if (contentLength > 0 && static_cast<uintmax_t>(contentLength) != fileSize) {
        std::cerr << "Downloaded file size (" << fileSize << " bytes) does not match expected size (" 
                  << contentLength << " bytes)" << std::endl;
        fs::remove(tempPath);
        return false;
    }
    
    // Rename the temporary file to the final path
    try {
        if (fs::exists(outputPath)) {
            fs::remove(outputPath);
        }
        fs::rename(tempPath, outputPath);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to rename temporary file: " << e.what() << std::endl;
        fs::remove(tempPath);
        return false;
    }
}

// Function to remove read-only attribute from a file
bool RemoveReadOnlyAttribute(const std::string& filePath) {
    // Check if the path is a file (not a directory)
    if (!fs::is_regular_file(filePath)) {
        std::cerr << "Path is not a file: " << filePath << std::endl;
        return false;
    }
    
    DWORD attributes = GetFileAttributesA(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        std::cerr << "Failed to get file attributes: " << filePath << std::endl;
        std::cerr << "Error code: " << error << " - " << GetErrorMessage(error) << std::endl;
        return false;
    }
    
    if (attributes & FILE_ATTRIBUTE_READONLY) {
        attributes &= ~FILE_ATTRIBUTE_READONLY;
        if (!SetFileAttributesA(filePath.c_str(), attributes)) {
            DWORD error = GetLastError();
            std::cerr << "Failed to remove read-only attribute: " << filePath << std::endl;
            std::cerr << "Error code: " << error << " - " << GetErrorMessage(error) << std::endl;
            return false;
        }
        std::cout << "Removed read-only attribute from: " << filePath << std::endl;
    }
    
    return true;
}

// Function to set read-only attribute on a file
bool SetReadOnlyAttribute(const std::string& filePath) {
    // Check if the path is a file (not a directory)
    if (!fs::is_regular_file(filePath)) {
        std::cerr << "Path is not a file: " << filePath << std::endl;
        return false;
    }
    
    DWORD attributes = GetFileAttributesA(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        std::cerr << "Failed to get file attributes: " << filePath << std::endl;
        std::cerr << "Error code: " << error << " - " << GetErrorMessage(error) << std::endl;
        return false;
    }
    
    attributes |= FILE_ATTRIBUTE_READONLY;
    if (!SetFileAttributesA(filePath.c_str(), attributes)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to set read-only attribute: " << filePath << std::endl;
        std::cerr << "Error code: " << error << " - " << GetErrorMessage(error) << std::endl;
        return false;
    }
    std::cout << "Set read-only attribute on: " << filePath << std::endl;
    
    return true;
}

// Function to get the VRChat Tools directory path
std::string GetVRChatToolsPath() {
    PWSTR localAppDataLowPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, NULL, &localAppDataLowPath);
    
    if (FAILED(hr) || !localAppDataLowPath) {
        std::cerr << "Failed to get LocalAppDataLow folder path. Error code: " << hr << std::endl;
        return "";
    }
    
    // Convert wide string to narrow string
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, localAppDataLowPath, -1, NULL, 0, NULL, NULL);
    std::string localAppDataLow(bufferSize, 0);
    int result = WideCharToMultiByte(CP_UTF8, 0, localAppDataLowPath, -1, &localAppDataLow[0], bufferSize, NULL, NULL);
    
    // Ensure the string is properly null-terminated and doesn't have extra characters
    if (result > 0) {
        localAppDataLow.resize(result - 1);
    }
    
    // Free the wide string
    CoTaskMemFree(localAppDataLowPath);
    
    // Construct the full path
    std::string vrchatToolsPath = localAppDataLow + "\\VRChat\\VRChat\\Tools";
    return vrchatToolsPath;
}

// Function to fetch the latest release information from GitHub
bool FetchLatestReleaseInfo(std::string& downloadUrl, std::string& latestVersion) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    // Set up CURL options
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Verify SSL certificate
    
    // Add User-Agent header (required by GitHub API)
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: yt-dlp-updater");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }
    
    // Get the response code
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    if (responseCode != 200) {
        std::cerr << "HTTP request failed with response code: " << responseCode << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    try {
        // Parse JSON response
        json data = json::parse(response);
        
        // Get the assets array
        const auto& assets = data["assets"];
        
        // Find the yt-dlp.exe asset
        for (const auto& asset : assets) {
            std::string name = asset["name"];
            if (name == "yt-dlp.exe") {
                downloadUrl = asset["browser_download_url"];
                latestVersion = data["tag_name"];
                
                std::cout << "Latest version: " << latestVersion << std::endl;
                std::cout << "Download URL: " << downloadUrl << std::endl;
                return true;
            }
        }
        
        std::cerr << "yt-dlp.exe asset not found in the latest release." << std::endl;
        return false;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

// Function to update yt-dlp.exe
bool UpdateYtDlp(const std::string& vrchatToolsPath, const std::string& downloadUrl, const std::string& latestVersion) {
    std::string ytDlpPath = vrchatToolsPath + "\\yt-dlp.exe";
    std::string versionFilePath = vrchatToolsPath + "\\yt-dlp-version.txt";
    
    std::cout << "Using VRChat Tools directory: " << vrchatToolsPath << std::endl;
    
    // Check Tools directory attributes before operations
    CheckAttributes(vrchatToolsPath, "Tools directory (before)");
    
    // Create directory if it doesn't exist
    if (!fs::exists(vrchatToolsPath)) {
        std::cout << "Creating VRChat Tools directory: " << vrchatToolsPath << std::endl;
        fs::create_directories(vrchatToolsPath);
        
        // Check Tools directory attributes after creation
        CheckAttributes(vrchatToolsPath, "Tools directory (after creation)");
    }
    
    // Check if yt-dlp.exe exists
    if (fs::exists(ytDlpPath)) {
        std::cout << "Existing yt-dlp.exe found at: " << ytDlpPath << std::endl;
        
        // Check yt-dlp.exe attributes before operations
        CheckAttributes(ytDlpPath, "yt-dlp.exe (before)");
        
        // Remove read-only attribute if present (only on the file, not the directory)
        if (!RemoveReadOnlyAttribute(ytDlpPath)) {
            std::cerr << "Failed to remove read-only attribute. Aborting." << std::endl;
            return false;
        }
        
        // Check yt-dlp.exe attributes after removing read-only
        CheckAttributes(ytDlpPath, "yt-dlp.exe (after removing read-only)");
        
        // Delete the existing file
        std::cout << "Deleting existing yt-dlp.exe..." << std::endl;
        if (!fs::remove(ytDlpPath)) {
            std::cerr << "Failed to delete existing yt-dlp.exe. Aborting." << std::endl;
            return false;
        }
    }
    
    // Download the latest yt-dlp.exe
    std::cout << "Downloading latest yt-dlp.exe to: " << ytDlpPath << std::endl;
    if (!DownloadFile(downloadUrl, ytDlpPath)) {
        std::cerr << "Failed to download yt-dlp.exe." << std::endl;
        return false;
    }
    
    std::cout << "Successfully downloaded yt-dlp.exe!" << std::endl;
    
    // Set integrity level to medium
    std::string icaclsCommand = "icacls \"" + ytDlpPath + "\" /setintegritylevel medium";
    std::cout << "Setting integrity level to medium: " << icaclsCommand << std::endl;
    
    // Verify ytDlpPath is a valid location to prevent potential exploits
    if (!fs::exists(ytDlpPath) || !fs::is_regular_file(ytDlpPath)) {
        std::cerr << "Invalid file path for integrity level setting: " << ytDlpPath << std::endl;
        return false;
    }
    
    // Verify the path is within the expected directory to prevent directory traversal attacks
    std::string expectedDir = vrchatToolsPath;
    if (ytDlpPath.find(expectedDir) != 0) {
        std::cerr << "Security check failed: yt-dlp.exe path is outside the expected directory" << std::endl;
        return false;
    }
    
    int result = system(icaclsCommand.c_str());
    if (result != 0) {
        std::cerr << "Failed to set integrity level. Error code: " << result << std::endl;
        return false;
    }
    std::cout << "Successfully set integrity level to medium." << std::endl;
    
    // Check yt-dlp.exe attributes before setting read-only
    CheckAttributes(ytDlpPath, "yt-dlp.exe (before setting read-only)");
    
    // Set read-only attribute on the new file (only on the file, not the directory)
    if (!SetReadOnlyAttribute(ytDlpPath)) {
        std::cerr << "Failed to set read-only attribute on the new file." << std::endl;
        return false;
    }
    
    std::cout << "Successfully updated yt-dlp.exe and set read-only attribute!" << std::endl;
    
    // Check yt-dlp.exe attributes after setting read-only
    CheckAttributes(ytDlpPath, "yt-dlp.exe (after setting read-only)");
    
    // Write version to version file
    if (!WriteVersionFile(versionFilePath, latestVersion)) {
        std::cerr << "Failed to update version file." << std::endl;
        return false;
    }
    
    std::cout << "Updated version file with version: " << latestVersion << std::endl;
    
    // Check Tools directory attributes after all operations
    CheckAttributes(vrchatToolsPath, "Tools directory (after all operations)");
    
    return true;
}

// Function to create and configure yt-dlp.conf
bool ConfigureYtDlp(const std::string& vrchatToolsPath) {
    std::string configPath = vrchatToolsPath + "\\yt-dlp.conf";
    
    // Check if config file already exists
    if (fs::exists(configPath)) {
        std::cout << "yt-dlp.conf already exists." << std::endl;
        return true;
    }
    
    // Create config file with default settings
    std::ofstream configFile(configPath);
    if (!configFile.is_open()) {
        std::cerr << "Failed to create yt-dlp.conf" << std::endl;
        return false;
    }
    
    // Write default settings
    configFile << "--no-playlist\n";
    configFile << "--no-warnings\n";
    configFile << "--quiet\n";
    configFile << "--no-progress\n";
    
    // List of available browsers
    std::vector<std::string> browsers = {
        "firefox", "brave", "chrome", "chromium", "edge", 
        "opera", "safari", "vivaldi", "whale"
    };
    
    std::cout << "\nAvailable browsers:" << std::endl;
    for (size_t i = 0; i < browsers.size(); ++i) {
        std::cout << i + 1 << ". " << browsers[i] << std::endl;
    }
    
    std::cout << "\nNote: Firefox is preferred as Chrome-based browsers may fail to work if they are running while loading videos." << std::endl;
    
    int choice;
    while (true) {
        std::cout << "\nSelect a browser (1-9): ";
        if (std::cin >> choice && choice >= 1 && choice <= static_cast<int>(browsers.size())) {
            break;
        }
        std::cout << "Invalid choice. Please try again." << std::endl;
        std::cin.clear();
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    }
    
    // Append browser selection to config file
    configFile << "--cookies-from-browser " << browsers[choice - 1] << std::endl;
    configFile.close();
    
    std::cout << "Created yt-dlp.conf with selected browser: " << browsers[choice - 1] << std::endl;
    
    // Clear any leftover input
    std::cin.clear();
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    
    return true;
}

int main() {
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Get the VRChat Tools directory path
    std::string vrchatToolsPath = GetVRChatToolsPath();
    if (vrchatToolsPath.empty()) {
        std::cerr << "Failed to get VRChat Tools directory path. Aborting." << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Configure yt-dlp if needed
    if (!ConfigureYtDlp(vrchatToolsPath)) {
        std::cerr << "Failed to configure yt-dlp. Aborting." << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Fetch the latest release information
    std::string downloadUrl;
    std::string latestVersion;
    if (!FetchLatestReleaseInfo(downloadUrl, latestVersion)) {
        std::cerr << "Failed to fetch latest release information. Aborting." << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Check current version from version file
    std::string versionFilePath = vrchatToolsPath + "\\yt-dlp-version.txt";
    std::string currentVersion = ReadVersionFile(versionFilePath);
    std::cout << "Current version: " << (currentVersion.empty() ? "Unknown" : currentVersion) << std::endl;
    
    // Check if update is needed
    bool updateNeeded = currentVersion != latestVersion;
    if (!updateNeeded) {
        std::cout << "yt-dlp.exe is already up to date (version " << currentVersion << ")." << std::endl;
    } else {
        std::cout << "Update needed: " << currentVersion << " -> " << latestVersion << std::endl;
        
        // Update yt-dlp.exe
        if (!UpdateYtDlp(vrchatToolsPath, downloadUrl, latestVersion)) {
            std::cerr << "Failed to update yt-dlp.exe. Aborting." << std::endl;
            curl_global_cleanup();
            return 1;
        }
    }
    
    // Clean up
    curl_global_cleanup();
    
    std::cout << "\nPress any key to exit...";
    std::cin.get();
    
    return 0;
} 