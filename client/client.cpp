#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <openssl/sha.h>
#include <iomanip>
#include <unordered_map>
#include <thread>
#include <mutex>

using namespace std;

struct ConnectionInfo {
    string ipAddress;
    int portNumber;
};

struct FileChunk {
    string fileName; // The original file name provided by the user
    string fileHash; // SHA256 hash of the entire file
    vector<string> chunks; // Chunks of the file
};

string sha256(const string& str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(str.c_str()), str.size(), hash);
    
    std::ostringstream oss;
    for (const auto& byte : hash) {
        oss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(byte);
    }
    return oss.str();
}

void uploadFile(int sock, unordered_map<string, FileChunk>& fileStorage,string ipAddress) {
    string filePath, fileName, groupName;
    cout << "Enter the group name: ";
    getline(cin, groupName);
    cout << "Enter the folder path of theipAddr file to upload: ";
    getline(cin, filePath);
    cout << "Enter the file name to upload: ";
    getline(cin, fileName);

    string fullPath = filePath + "/" + fileName;

    ifstream file(fullPath, ios::binary);
    if (!file) {
        cerr << "Unable to open file: " << fullPath << endl;
        return;
    }

    string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    string fileHash = sha256(fileContent);
    
    size_t chunkSize = 512 * 1024;
    size_t totalChunks = (fileContent.size() + chunkSize - 1) / chunkSize;

    vector<string> chunks;
    vector<string> chunkHashes;
    
    for (size_t i = 0; i < totalChunks; ++i) {
        size_t start = i * chunkSize;
        size_t end = min(start + chunkSize, fileContent.size());
        string chunk = fileContent.substr(start, end - start);
        chunks.push_back(chunk);
        string chunkHash = sha256(chunk);
        chunkHashes.push_back(chunkHash);
    }

    FileChunk fileChunk = {fileName, fileHash, chunks};
    fileStorage[fileName] = fileChunk;

    cout << "File uploaded successfully with SHA256: " << fileHash << endl;

    string message = "upload_file " + groupName + " " + fileName + " " + fileHash + " " + ipAddress;
    for (const auto& chunkHash : chunkHashes) {
        message += " " + chunkHash;
    }

    cout << "SENDING " << message.size() << "\n\n\n";
    cin;

    send(sock, message.c_str(), message.size(), 0);
    char buffer[4096] = {0};
    int valread = read(sock, buffer, sizeof(buffer));
    if (valread > 0) {
        string response(buffer, valread);
        cout << "Tracker said: " << response << endl;
    }
}

void downloadChunk(const string& peerIP, int peerPort, const string& fileName, int chunkIndex) {

    
    int peerSock = socket(AF_INET, SOCK_STREAM, 0);
    if (peerSock < 0) {
        cerr << "Failed to create socket for peer" << endl;
        return;
    }

    peerPort=9400;

    struct sockaddr_in peerAddr;
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peerPort);
    if (inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr) <= 0) {
        cerr << "Invalid peer address" << endl;
        close(peerSock);
        return;
    }

    if (connect(peerSock, (struct sockaddr *)&peerAddr, sizeof(peerAddr)) < 0) {
        cerr << "Connection to peer failed" << endl;
        close(peerSock);
        return;
    }

    string request = "get_chunk " + fileName + " " + to_string(chunkIndex);
    cout<<request<<endl;
    send(peerSock, request.c_str(), request.size(), 0);

    cout<<"AFTER SEND";

    char buffer[1024];
    ofstream outFile(fileName, ios::binary | ios::app);
    if (!outFile) {
        cerr << "Failed to open file for writing" << endl;
        close(peerSock);
        return;
    }

    int bytesReceived;
    while ((bytesReceived = read(peerSock, buffer, sizeof(buffer))) > 0) {
        cout<<"WRITING"<<endl;
        
        outFile.write(buffer, bytesReceived);
    }

    outFile.close();
    close(peerSock);
    cout << "Downloaded chunk " << chunkIndex << " of " << fileName << endl;
}

bool readTrackerInfo(const string& filename, ConnectionInfo& conn) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return false;
    }

    string line;
    if (getline(file, line)) {
        istringstream iss(line);
        if (iss >> conn.ipAddress >> conn.portNumber) {
            return true;
        } else {
            cerr << "Error: Invalid format in line: " << line << endl;
            return false;
        }
    }

    cerr << "Error: No data in file." << endl;
    return false;
}

void listenForUploads(int listeningPort) {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        cerr << "Failed to create listening socket" << endl;
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Listen on all interfaces
    serverAddr.sin_port = htons(listeningPort);

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Bind failed" << endl;
        close(serverSock);
        return;
    }

    listen(serverSock, 3); // Listen for up to 3 connections

    cout << "Listening for incoming file uploads on port " << listeningPort << endl;

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
        // int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0) {
            cerr << "Accept failed" << endl;
            continue;
        }

        // Handle file upload in a separate thread
        std::thread([clientSock]() {
            char buffer[1024];
            string receivedMessage;

            // Read incoming message
            int bytesRead;
            while ((bytesRead = read(clientSock, buffer, sizeof(buffer))) > 0) {
                cout<<"LISTENSKAJLDLKASNFL: "<<buffer<<endl;
                receivedMessage.append(buffer, bytesRead);
            }

            cout << "Received from another client: " << receivedMessage << endl;

            // Here you would handle the file writing part
            ofstream outFile("received_file", ios::binary | ios::app); // Change as needed
            if (!outFile) {
                cerr << "Failed to open file for writing" << endl;
                close(clientSock);
                return;
            }

            outFile.write(receivedMessage.c_str(), receivedMessage.size());
            outFile.close();
            close(clientSock);
            cout << "File upload completed." << endl;
        }).detach(); // Detach the thread to handle multiple uploads concurrently
    }

    close(serverSock);
}



int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <IP>:<PORT> tracker_info.txt" << endl;
        return EXIT_FAILURE;
    }

    string clientInput = argv[1];
    size_t colonPos = clientInput.find(':');
    if (colonPos == string::npos) {
        cerr << "Error: Invalid format for client IP and port. Use <IP>:<PORT>" << endl;
        return EXIT_FAILURE;
    }

    string clientIP = clientInput.substr(0, colonPos);
    int clientPort = stoi(clientInput.substr(colonPos + 1));

    string ipAddr = clientIP+":"+to_string(clientPort);

    ConnectionInfo conn;
    if (!readTrackerInfo(argv[2], conn)) {
        return EXIT_FAILURE;
    }

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "Socket creation error" << endl;
        return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conn.portNumber);

    if (inet_pton(AF_INET, conn.ipAddress.c_str(), &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address or Address not supported" << endl;
        return EXIT_FAILURE;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection to tracker failed" << endl;
        return EXIT_FAILURE;
    }

    cout << "Connected to the tracker at " << conn.ipAddress << ":" << conn.portNumber << endl;

    // Start listening for uploads on a specified port (e.g., clientPort)
    std::thread listeningThread(listenForUploads, clientPort);
    listeningThread.detach(); // Detach so it runs independently

    bool isLoggedIn = false;
    unordered_map<string, FileChunk> fileStorage;

    while (true) {
        string message;
        cout << "> ";
        getline(cin, message);

        if (message.empty()) {
            continue;
        } else if (message.rfind("login", 0) == 0 && isLoggedIn) {
            cout << "You are already logged in. Log out to log in with a different account." << endl;
            continue;
        } else if (message.rfind("login", 0) == 0) {
            send(sock, message.c_str(), message.size(), 0);
            char buffer[1024] = {0};
            int valread = read(sock, buffer, sizeof(buffer));
            if (valread > 0) {
                string response(buffer, valread);
                cout << "Tracker said: " << response << endl;
                isLoggedIn = (response.find("Logged in!") != string::npos);
            }
        } else if (message == "logout") {
            send(sock, message.c_str(), message.size(), 0);
            char buffer[1024] = {0};
            int valread = read(sock, buffer, sizeof(buffer));
            if (valread > 0) {
                string response(buffer, valread);
                cout << "Tracker said: " << response << endl;
            isLoggedIn = false;
                // isLoggedIn = (response.find("Logged in!") != string::npos);
            }
            
        } else if (message == "quit") {
            send(sock, message.c_str(), message.size(), 0);
            cout << "Exiting..." << endl;
            break;
        } else if (message.rfind("upload_file", 0) == 0) {
            uploadFile(sock, fileStorage,ipAddr);
        } else if (message.rfind("get_file ", 0) == 0) {
            send(sock, message.c_str(), message.size(), 0);
            
            char buffer[4096] = {0};
            int valread = read(sock, buffer, sizeof(buffer));
            if (valread > 0) {
                string response(buffer, valread);
                cout << "Tracker said: " << response << endl;

                // Initialize variables to store extracted information
                string fileHash;
                int totalChunks = 0;
                vector<string> clientIPs;

                vector<thread> downloadThreads; // Declare the thread vector here

                stringstream ss(response);
                string line;

                // Process the response line by line
                while (getline(ss, line)) {
                    if (line.find("File Hash: ") != string::npos) {
                        fileHash = line.substr(line.find(":") + 2);
                    }
                    else if (line.find("Total Chunks: ") != string::npos) {
                        totalChunks = stoi(line.substr(line.find(":") + 2));
                    }
                    else if (line.find("Client IP: ") != string::npos) {
                        size_t ipStart = line.find("Client IP: ") + 12; // Length of "Client IP: "
                        size_t ipEnd = line.find(" (Chunks:");
                        string peerAddress = line.substr(ipStart-1, ipEnd - ipStart+1);
                        cout<<"PEER ADDRESS "<<peerAddress<<"\n\n";
                        
                        // Extract IP and Port
                        size_t portStart = peerAddress.find(':');
                        string peerIP = peerAddress.substr(0, portStart);
                        int peerPort = stoi(peerAddress.substr(portStart + 1)); // Port after ':'

                        clientIPs.push_back(peerIP);
                        
                        // Download all chunks from this client
                        for (int index = 0; index < totalChunks; ++index) {
                            downloadThreads.emplace_back(downloadChunk, peerIP, clientPort, message.substr(9), index);
                        }
                    }
                }

                // Output the collected information
                cout << "Received file hash: " << fileHash << endl;
                cout << "Total chunks to be downloaded: " << totalChunks << endl;
                cout << "Clients' IP addresses: ";
                for (const auto& ip : clientIPs) {
                    cout << ip << " ";
                }
                
                cout << endl;

                // Join download threads after starting them
                for (auto& t : downloadThreads) {
                    if (t.joinable()) {
                        t.join();
                    }
                }
            }
        }
        // else if(){

        // }
         else {
            send(sock, message.c_str(), message.size(), 0);
            char buffer[1024] = {0};
            int valread = read(sock, buffer, sizeof(buffer));
            if (valread > 0) {
                string response(buffer, valread);
                cout << "Tracker said: " << response << endl;
            }
        }
    }

    close(sock);
    return 0;
}
