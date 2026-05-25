#include <windows.h>
#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <winioctl.h> // Required for getting disk size

const int SECTOR_SIZE = 4096;
const int CHUNK_SIZE = SECTOR_SIZE * 256; // 1MB test chunk
const int TOTAL_BLOCKS = 64; // We will test 64 zones across the drive
const int BLOCKS_PER_ROW = 8; // 8x8 grid

// ANSI Color Codes for the grid
const std::string COLOR_GREEN = "\033[42m  \033[0m"; // Green background, two spaces
const std::string COLOR_RED = "\033[41m  \033[0m";   // Red background, two spaces
const std::string COLOR_GRAY = "\033[100m  \033[0m"; // Gray background (untested)

// Enable Windows console to show ANSI colors
void enableColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

bool isRemovableUSB(const std::string& driveLetter) {
    std::string rootPath = driveLetter + ":\\";
    UINT driveType = GetDriveTypeA(rootPath.c_str());
    return (driveType == DRIVE_REMOVABLE);
}

void printGrid(const std::vector<int>& gridStatus, int currentBlock) {
    // Move cursor up to overwrite the previous grid map
    std::cout << "\033[" << (TOTAL_BLOCKS / BLOCKS_PER_ROW) + 2 << "A"; 
    
    std::cout << "\n--- DRIVE HEALTH MAP ---\n";
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (gridStatus[i] == 1) std::cout << COLOR_GREEN;
        else if (gridStatus[i] == -1) std::cout << COLOR_RED;
        else if (i == currentBlock) std::cout << "\033[43m  \033[0m"; // Yellow (Testing currently)
        else std::cout << COLOR_GRAY;

        if ((i + 1) % BLOCKS_PER_ROW == 0) std::cout << "\n"; // Next row
    }
}

void fullDriveTest(const std::string& driveLetter) {
    std::string path = "\\\\.\\" + driveLetter + ":";
    HANDLE hDevice = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Could not open drive. Run as Administrator!\n";
        return;
    }

    // --- 1. GET TOTAL DRIVE CAPACITY ---
    GET_LENGTH_INFORMATION lengthInfo;
    DWORD bytesReturned;
    if (!DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL)) {
        std::cerr << "Error: Could not determine drive size.\n";
        CloseHandle(hDevice);
        return;
    }

    long long totalBytes = lengthInfo.Length.QuadPart;
    double totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
    std::cout << "Claimed Drive Capacity: " << totalGB << " GB\n\n";

    std::cout << "Locking volume...\n";
    DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);

    void* writeBuffer = _aligned_malloc(CHUNK_SIZE, SECTOR_SIZE);
    void* readBuffer = _aligned_malloc(CHUNK_SIZE, SECTOR_SIZE);
    memset(writeBuffer, 0xAA, CHUNK_SIZE);

    // Initialize Grid (0 = untested, 1 = pass, -1 = fail)
    std::vector<int> gridStatus(TOTAL_BLOCKS, 0);
    long long zoneSize = totalBytes / TOTAL_BLOCKS;

    // Print initial empty grid space
    for (int i = 0; i <= (TOTAL_BLOCKS / BLOCKS_PER_ROW) + 1; i++) std::cout << "\n";

    // --- 2. SPOT CHECK ENTIRE DRIVE ---
    int passedBlocks = 0;
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        printGrid(gridStatus, i); // Update visuals

        // Calculate offset for this specific zone
        LARGE_INTEGER offset;
        offset.QuadPart = i * zoneSize;

        // Write
        SetFilePointerEx(hDevice, offset, NULL, FILE_BEGIN);
        DWORD bytesWritten;
        bool writeOk = WriteFile(hDevice, writeBuffer, CHUNK_SIZE, &bytesWritten, NULL) && (bytesWritten == CHUNK_SIZE);

        // Read
        memset(readBuffer, 0x00, CHUNK_SIZE); // Clear read buffer
        SetFilePointerEx(hDevice, offset, NULL, FILE_BEGIN);
        DWORD bytesRead;
        bool readOk = ReadFile(hDevice, readBuffer, CHUNK_SIZE, &bytesRead, NULL) && (bytesRead == CHUNK_SIZE);

        // Verify
        if (writeOk && readOk && memcmp(writeBuffer, readBuffer, CHUNK_SIZE) == 0) {
            gridStatus[i] = 1; // Green
            passedBlocks++;
        } else {
            gridStatus[i] = -1; // Red
        }
    }

    printGrid(gridStatus, -1); // Final update

    // --- 3. FINAL VERDICT ---
    std::cout << "\nTest Complete.\n";
    std::cout << "Passed: " << passedBlocks << "/" << TOTAL_BLOCKS << " blocks.\n";
    if (passedBlocks == TOTAL_BLOCKS) {
        std::cout << "[+] This drive appears to be GENUINE.\n";
    } else {
        double realGB = (double)passedBlocks / TOTAL_BLOCKS * totalGB;
        std::cout << "[-] FAKE DETECTED! Real capacity is likely only ~" << realGB << " GB.\n";
    }

    DeviceIoControl(hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
    CloseHandle(hDevice);
    _aligned_free(writeBuffer);
    _aligned_free(readBuffer);
}

int main() {
    enableColors(); // Turn on Windows console color support
    std::string driveLetter;
    
    std::cout << "=================================\n";
    std::cout << "  FAKE DRIVE CATCHER (GRID MAP)  \n";
    std::cout << "=================================\n\n";
    std::cout << "Enter the USB drive letter to test: ";
    std::cin >> driveLetter;

    if (!driveLetter.empty()) driveLetter[0] = toupper(driveLetter[0]);

    if (!isRemovableUSB(driveLetter)) {
        std::cerr << "ABORTING: Only testing removable USB drives.\n";
        system("pause");
        return 1;
    }

    fullDriveTest(driveLetter);
    system("pause");
    return 0;
}