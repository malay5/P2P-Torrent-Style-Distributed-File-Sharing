#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <pthread.h> // Include pthread header
#include <mutex>
#include <unordered_set>

using namespace std;

struct ConnectionInfo {
    std::string ipAddress;
    int portNumber;
};

struct FileChunk {
    string fileName; // The original file name provided by the user
    string fileHash; // SHA256 hash of the entire file
    int totalChunks; // Total number of chunks
    unordered_map<string, vector<int>> clientChunks; // Mapping of client IPs to chunk indices
    vector<string> chunkHashes; // Hashes for each chunk
    std::unordered_map<int, ConnectionInfo> peers; // Mapping of client sockets to their peer info
    string group_name;
    vector<string> owners;
};

struct group_details {
    vector<string> members;
    string owner;
    unordered_set<string> pending_requests;
    unordered_map<string,vector<string>> shareable_files;
};

std::mutex db_mutex; // Mutex for thread safety

unordered_map<string, string> userDatabase;
unordered_map<int, string> loggedInUsersBySocket;
unordered_map<string, string> loggedInUsersByIP;
unordered_map<string, FileChunk> fileStorage; // Global storage for files and their chunks

unordered_map<string, group_details> group_data;





void* handleClient(void* arg) {
    int new_socket = *((int*)arg);
    delete (int*)arg; // Free the allocated memory for socket
    char buffer[1024*1024];

    struct sockaddr_in client_address;
    socklen_t client_addrlen = sizeof(client_address);

    getpeername(new_socket, (struct sockaddr*)&client_address, &client_addrlen);
    std::string client_ip = inet_ntoa(client_address.sin_addr);
    int client_port = ntohs(client_address.sin_port);

    while (true) {
        memset(buffer, 0, sizeof(buffer)); // Clear the buffer for each read
        int valread = read(new_socket, buffer, sizeof(buffer));

        if (valread <= 0) {
            std::cout << "Client disconnected or error occurred." << std::endl;
            close(new_socket);
            break; // Exit thread
        }

        std::string receivedMessage(buffer, valread);
        // std::cout << "Received: " << receivedMessage << std::endl;
                std::cout << "Received from " << client_ip << ":"<<client_port<<"  -> " << receivedMessage << std::endl;
        string ipAdd = client_ip + ":"+to_string(client_port);
        if (receivedMessage == "quit") {
            std::cout << "Client requested to quit. Closing connection." << std::endl;
            close(new_socket);
            break; // Exit thread
        }

        // Handle commands
        else if (receivedMessage.rfind("create_user", 0) == 0) {
            std::istringstream iss(receivedMessage);
            std::string command, user_id, passwd;
            iss >> command >> user_id >> passwd;

            if (user_id.empty() || passwd.empty()) {
                std::string errorMsg = "Invalid command format. Use: create_user <user_id> <passwd>\n";
                send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
            } else {
                // Lock mutex while modifying shared database
                std::lock_guard<std::mutex> lock(db_mutex);
                userDatabase[user_id] = passwd;
                std::string successMsg = "User Created Successfully\n";
                send(new_socket, successMsg.c_str(), successMsg.size(), 0);
            }
        } else if (receivedMessage.rfind("login", 0) == 0) {
            std::istringstream iss(receivedMessage);
            std::string command, user_id, passwd;
            iss >> command >> user_id >> passwd;

            std::lock_guard<std::mutex> lock(db_mutex);
            if (userDatabase.find(user_id) == userDatabase.end() || userDatabase[user_id] != passwd) {
                std::string errorMsg = "Invalid Credentials\n";
                send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
            } else if (loggedInUsersByIP.count(ipAdd) > 0 && loggedInUsersByIP[ipAdd]==user_id) {
                std::string errorMsg = "You are already logged in\n";
                send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
            } else if (loggedInUsersByIP.count(ipAdd) > 0) {
                std::string errorMsg = "Another user is already logged in from this client\n";
                send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
            } else {
                loggedInUsersByIP[ipAdd] = user_id;
                loggedInUsersBySocket[new_socket]=user_id;
                std::string successMsg = "Logged in!\n";
                send(new_socket, successMsg.c_str(), successMsg.size(), 0);
            }
        }
        else if (receivedMessage.rfind("logout", 0) == 0) {
    std::lock_guard<std::mutex> lock(db_mutex);
    
    // Check if the user is logged in from this socket
    if (loggedInUsersBySocket.count(new_socket) > 0) {
        std::string user_id = loggedInUsersBySocket[new_socket];
        
        // Remove the user from the logged-in user maps
        loggedInUsersByIP.erase(ipAdd);
        loggedInUsersBySocket.erase(new_socket);

        std::string successMsg = "Logged out successfully!\n";
        send(new_socket, successMsg.c_str(), successMsg.size(), 0);
    } else {
        std::string errorMsg = "You are not logged in!\n";
        send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
    }
}
       
       else if(receivedMessage.rfind("create_group",0)==0){
            std::istringstream iss(receivedMessage);
            std::string command, group_id;
            iss >> command >> group_id;

            if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
                // Send message back indicating no user is logged in
                std::string response = "No user is logged in.";
                send(new_socket, response.c_str(), response.size(), 0);
                // return; // Exit early since no user is logged in
            }

            else if (group_data.count(group_id) > 0) {
                // Send message back indicating no user is logged in
                std::string response = "Group with that id already exists.";
                send(new_socket, response.c_str(), response.size(), 0);
                // return; // Exit early since no user is logged in
            }
            else{

            string curr_uname=loggedInUsersBySocket[new_socket];
            group_details gdata;
            gdata.members={};
            gdata.owner=curr_uname;
            cout<<"Curr username"<<curr_uname<<endl;
            gdata.pending_requests={};

            group_data[group_id]=gdata;

            std::string successMsg = "Group created successfully.\n";
            send(new_socket, successMsg.c_str(), successMsg.size(), 0);
       }
       }
else if (receivedMessage.rfind("join_group", 0) == 0) {
    // Check if the socket is valid and corresponds to a logged-in user
    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
        std::string response = "No user is logged in.";
        send(new_socket, response.c_str(), response.size(), 0);
    } else {
        // Extract the group_id from the received message
        std::istringstream iss(receivedMessage);
        std::string command, group_id;
        iss >> command >> group_id;

        // Check if the group exists
        if (group_data.find(group_id) == group_data.end()) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
        } else {
            // Get the current user
            std::string curr_uname = loggedInUsersBySocket[new_socket];
            group_details& gdata = group_data[group_id];

            // Check if the user is already a member or the owner
            bool isMember = false;
            for (const auto& member : gdata.members) {
                if (member == curr_uname) {
                    isMember = true;
                    break;
                }
            }

            if (isMember || gdata.owner == curr_uname) {
                std::string response = "You are already a member or the owner of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
            } else {
                // Add the user to pending_requests
                gdata.pending_requests.insert(curr_uname);
                std::string response = "Your request to join the group has been sent.";
                send(new_socket, response.c_str(), response.size(), 0);
            }
        }
    }
}

       else if (receivedMessage.rfind("leave_group", 0) == 0) {
    // Check if the socket is valid and corresponds to a logged-in user
    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
        std::string response = "No user is logged in.";
        send(new_socket, response.c_str(), response.size(), 0);
    } else {
        // Extract the group_id from the received message
        std::istringstream iss(receivedMessage);
        std::string command, group_id;
        iss >> command >> group_id;

        // Check if the group exists
        if (group_data.find(group_id) == group_data.end()) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
        } else {
            // Get the current user
            std::string curr_uname = loggedInUsersBySocket[new_socket];
            group_details& gdata = group_data[group_id];

            // Check if the user is part of the group
            bool isMember = false;
            for (const auto& member : gdata.members) {
                if (member == curr_uname) {
                    isMember = true;
                    break;
                }
            }

            if (!isMember && (gdata.owner!=curr_uname) ) {
                std::string response = "You are not a member of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
            } else {
                // Manual removal of the user from members
                std::vector<std::string> new_members;
                    bool isOwner = (gdata.owner == curr_uname);
                for (const auto& member : gdata.members) {
                    if (member != curr_uname) {
                        new_members.push_back(member);
                    }
                }
                gdata.members = std::move(new_members); // Update the members list

                // Check if the group is empty after leaving
                if (gdata.members.empty() && !isOwner) {
                    group_data.erase(group_id); // Remove the group if no members left and the owner has left
                    std::string response = "You left the group and the group has been deleted.";
                    send(new_socket, response.c_str(), response.size(), 0);
                } else if (gdata.members.empty() && isOwner) {
                    // If the owner leaves and no members left, delete the group
                    group_data.erase(group_id);
                    std::string response = "You left the group and the group has been deleted.";
                    send(new_socket, response.c_str(), response.size(), 0);
                } else if (isOwner) {
                    // If the owner is still present, promote the next member to owner
                    gdata.owner = gdata.members.front(); // Promote the first member as the new owner
                    std::string response = "You left the group. The new owner is " + gdata.owner + ".";
                    send(new_socket, response.c_str(), response.size(), 0);
                } else {
                    std::string response = "You left the group.";
                    send(new_socket, response.c_str(), response.size(), 0);
                }
            }
        }
    }
}

       else if (receivedMessage.rfind("list_requests", 0) == 0) {
    // Check if the socket is valid and corresponds to a logged-in user
    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
        std::string response = "No user is logged in.";
        send(new_socket, response.c_str(), response.size(), 0);
    } else {
        // Extract the group_id from the received message
        std::istringstream iss(receivedMessage);
        std::string command, group_id;
        iss >> command >> group_id;

        // Check if the group exists
        if (group_data.find(group_id) == group_data.end()) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
        } else {
            // Get the current user
            std::string curr_uname = loggedInUsersBySocket[new_socket];
            group_details& gdata = group_data[group_id];

            // Check if the current user is the owner of the group
            if (gdata.owner != curr_uname) {
                std::string response = "You are not the owner of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
            } else {
                // Prepare to list the pending requests
                if (gdata.pending_requests.empty()) {
                    std::string response = "There are no pending requests to join this group.";
                    send(new_socket, response.c_str(), response.size(), 0);
                } else {
                    std::string response = "Pending requests to join the group:\n";
                    for (const auto& request : gdata.pending_requests) {
                        response += request + "\n"; // Append each request to the response
                    }
                    send(new_socket, response.c_str(), response.size(), 0);
                }
            }
        }
    }
}

       else if (receivedMessage.rfind("accept_request", 0) == 0) {
    // Check if the socket is valid and corresponds to a logged-in user
    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
        std::string response = "No user is logged in.";
        send(new_socket, response.c_str(), response.size(), 0);
    } else {
        // Extract the group_id and username from the received message
        std::istringstream iss(receivedMessage);
        std::string command, group_id, user_to_accept;
        iss >> command >> group_id >> user_to_accept;

        // Check if the group exists
        if (group_data.find(group_id) == group_data.end()) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
        } else {
            // Get the current user
            std::string curr_uname = loggedInUsersBySocket[new_socket];
            group_details& gdata = group_data[group_id];

            // Check if the current user is the owner of the group
            if (gdata.owner != curr_uname) {
                std::string response = "You are not the owner of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
            } else {
                // Check if the user_to_accept is in pending_requests
                if (gdata.pending_requests.find(user_to_accept) == gdata.pending_requests.end()) {
                    std::string response = "No such request to accept.";
                    send(new_socket, response.c_str(), response.size(), 0);
                } else {
                    // Add the user to members and remove from pending_requests
                    gdata.members.push_back(user_to_accept);
                    gdata.pending_requests.erase(user_to_accept);
                    
                    std::string response = user_to_accept + " has been accepted into the group.";
                    send(new_socket, response.c_str(), response.size(), 0);
                }
            }
        }
    }
}

       else if (receivedMessage.rfind("list_groups", 0) == 0) {
            // Check if the socket is valid and corresponds to a logged-in user
            if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
                // Send message back indicating no user is logged in
                std::string response = "No user is logged in.";
                send(new_socket, response.c_str(), response.size(), 0);
                // return; // Exit early since no user is logged in
            }
            else{

            // Prepare a response with all group_ids
            std::string response = "Groups:\n";
            for (const auto& entry : group_data) {
                response += entry.first + "\n"; // entry.first is the group_id
            }

            // If there are no groups, inform the user
            if (group_data.empty()) {
                response = "No groups available.";
            }

            // Send the list of group_ids to the user
            send(new_socket, response.c_str(), response.size(), 0);}
    }

       else if (receivedMessage.rfind("upload_file ", 0) == 0) {

        if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
            std::string response = "No user is logged in.";
            send(new_socket, response.c_str(), response.size(), 0);
        }
        else{
            std::istringstream iss(receivedMessage);
            std::string command, groupName, fileName, fileHash, userIP;
            iss >> command >> groupName >> fileName >> fileHash >> userIP;


                    std::string curr_uname = loggedInUsersBySocket[new_socket];
                    if (group_data.find(groupName) == group_data.end()) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
        }
        else{
            group_details& gdata = group_data[groupName];

            // Check if the user is a member of the group
            bool isMember = false;
            for (const auto& member : gdata.members) {
                if (member == curr_uname) {
                    isMember = true;
                    break;
                }
            }

            if (!isMember && (gdata.owner!=curr_uname)) {
                std::string response = "You are not a member of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
            }
            else{
                std::vector<std::string> chunkHashes;
            std::string chunkHash;
            while (iss >> chunkHash) { // Read the chunk hashes from the message
                chunkHashes.push_back(chunkHash);
            }

            std::lock_guard<std::mutex> lock(db_mutex); // Lock for thread safety

            // Store or update the file information in fileStorage
            FileChunk& fileChunk = fileStorage[fileName];
            fileChunk.fileName = fileName;
            fileChunk.fileHash = fileHash;
            fileChunk.totalChunks = chunkHashes.size();
            fileChunk.chunkHashes = chunkHashes;
            // fileChunk.peers[new_socket] = {client_ip, client_port};
            // fileChunk.username=loggedInUsersBySocket[new_socket];
            fileChunk.clientChunks[userIP] = {}; // Initialize entry for this client

            // Here, you would also need to specify which chunks this client has.
            // For simplicity, we'll assume this client has all chunks for now.
            for (int i = 0; i < chunkHashes.size(); ++i) {
                fileChunk.clientChunks[userIP].push_back(i); // Assume this client has all chunks
            }

            std::string successMsg = "File uploaded successfully with " + std::to_string(chunkHashes.size()) + " chunks.\n";
            gdata.shareable_files[fileName].push_back(curr_uname);
            send(new_socket, successMsg.c_str(), successMsg.size(), 0);
            }
        }   
        }
            

            
        }
        
        else if (receivedMessage.rfind("get_data ", 0) == 0) {
    std::istringstream iss(receivedMessage);
    std::string command, fileName;
    iss >> command >> fileName;

    std::lock_guard<std::mutex> lock(db_mutex); // Lock for thread safety

    if (fileStorage.count(fileName) > 0) {
        FileChunk& fileChunk = fileStorage[fileName];
        std::string response = "Username: " + loggedInUsersByIP[ipAdd] + "\n"; // Assuming the socket is mapped to a user
        response += "File: " + fileName + "\n";
        response += "Total Chunks: " + std::to_string(fileChunk.totalChunks) + "\n";
        response += "Clients with chunks:\n";

        for (const auto& client : fileChunk.clientChunks) {
                string client_socket = client.first;
                    // auto peer_info = fileChunk.peers[client_socket];
                        // response += "Client IP: " + peer_info.ipAddress + ":" + std::to_string(peer_info.portNumber) + ", Chunks: ";
            // response += "Client IP: " + to_string(client.first) + ", Chunks: ";

            for (int idx : client.second) {
                response += std::to_string(idx) + " (SHA: " + fileChunk.chunkHashes[idx] + ") "; // Add SHA value for each chunk
            }
            response += "\n";
        }
        send(new_socket, response.c_str(), response.size(), 0);
    } else {
        std::string errorMsg = "File not found.\n";
        send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
    }
}

else if (receivedMessage.rfind("download_file ", 0) == 0) {
    std::istringstream iss(receivedMessage);
    std::string command, fileName, group_id;
    iss >> command >> group_id >> fileName;

    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
        std::string response = "No user is logged in.";
        send(new_socket, response.c_str(), response.size(), 0);
        continue;
    }
    else{
        if (group_data.count(group_id)>0) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
            continue;
        }
    }
                group_details& gdata = group_data[group_id];
            std::string curr_uname = loggedInUsersBySocket[new_socket];
            bool isMember = false;

    for (const auto& member : gdata.members) {
                if (member == curr_uname) {
                    isMember = true;
                    break;
                }
            }
    if (!isMember && gdata.owner != curr_uname) {
                std::string response = "You are not part of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
                continue;
            }

    std::lock_guard<std::mutex> lock(db_mutex); // Lock for thread safety

    if (fileStorage.count(fileName) > 0) {
        FileChunk& fileChunk = fileStorage[fileName];
        std::string response = "File: " + fileName + "\n";
        response += "File Hash: " + fileChunk.fileHash + "\n";
        response += "Total Chunks: " + std::to_string(fileChunk.totalChunks) + "\n";
        response += "Clients with this file:\n";

        for (const auto& client : fileChunk.clientChunks) {
                string client_socket = client.first;
                cout<<client_socket<<endl;
                    // auto peer_info = fileChunk.peers[client_socket];
                    response += "  Client IP: " + client_socket;// + ":" + std::to_string(peer_info.portNumber) + " (Chunks: ";
            // response += "  Client Socket: " + std::to_string(client.first) + " (Chunks: ";
            for (int idx : client.second) {
                response += std::to_string(idx) + " ";
            }
            response += ")\n";
        }
        send(new_socket, response.c_str(), response.size(), 0);
    } else {
        std::string errorMsg = "File not found.\n";
        send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
    }
}



else if(receivedMessage.rfind("list_files",0)==0){
    std::istringstream iss(receivedMessage);
    std::string command, group_id;

    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
                // Send message back indicating no user is logged in
                std::string response = "No user is logged in.";
                send(new_socket, response.c_str(), response.size(), 0);
                // return; // Exit early since no user is logged in
            }
            else{
                iss >> command >> group_id;

                if (group_data.find(group_id) == group_data.end()) {
                    std::string response = "Group does not exist.";
                    send(new_socket, response.c_str(), response.size(), 0);
                }
                else{
                    std::string curr_uname = loggedInUsersBySocket[new_socket];
            group_details& gdata = group_data[group_id];

            // Check if the user is part of the group
            bool isMember = false;
            for (const auto& member : gdata.members) {
                if (member == curr_uname) {
                    isMember = true;
                    break;
                }
            }

            if (!isMember) {
                std::string response = "You are not a member of this group.";
                send(new_socket, response.c_str(), response.size(), 0);
            }
            else{
                 if (gdata.shareable_files.empty()) {
                    std::string response = "There are no files shared in this group.";
                    send(new_socket, response.c_str(), response.size(), 0);
                }
                else {
                    std::string response = "Users have shared files in the group:\n";
                    for (const auto& pair : gdata.shareable_files) {
                        response += pair.first + "\n"; 
                    }
                    send(new_socket, response.c_str(), response.size(), 0);
                }
            }
                } 

            }
}


else if(receivedMessage.rfind("stop_share",0)==0){
    if (loggedInUsersBySocket.find(new_socket) == loggedInUsersBySocket.end()) {
        std::string response = "No user is logged in.";
        send(new_socket, response.c_str(), response.size(), 0);
    }
    else{
        std::istringstream iss(receivedMessage);
        std::string command, group_id, file_name;
        iss >> command >> group_id>>file_name;

        if (group_data.find(group_id) == group_data.end()) {
            std::string response = "Group does not exist.";
            send(new_socket, response.c_str(), response.size(), 0);
        } 
        else{
            std::string curr_uname = loggedInUsersBySocket[new_socket];
            group_details& gdata = group_data[group_id];

            if(gdata.owner == curr_uname){

                if (gdata.shareable_files.find(file_name) != gdata.shareable_files.end()) {
                    // Remove the file from shareable_files
                    gdata.shareable_files.erase(file_name);
                    
                    std::string response = "File sharing stopped permanently. Upload the file again if you want it back in the group.";
                    send(new_socket, response.c_str(), response.size(), 0);
                }
                else{
                    std::string response = "File does not exist in the group.";
                    send(new_socket, response.c_str(), response.size(), 0);
                }

                // std::string response = "File sharing stopped permanently. Upload the file again, if you want it back in the group.";
                // send(new_socket, response.c_str(), response.size(), 0);
            }
            else{
                std::string response = "You don't have enough permissions to stop sharing the file. Contact the group owner.";
                send(new_socket, response.c_str(), response.size(), 0);
            }
        }
    }
}

// else if (receivedMessage == "list_files") {
//     std::lock_guard<std::mutex> lock(db_mutex); // Lock for thread safety

//     if (!fileStorage.empty()) {
//         std::string response = "Available files:\n";

//         for (const auto& entry : fileStorage) {
//             const std::string& fileName = entry.first;
//             const FileChunk& fileChunk = entry.second;

//             response += "File: " + fileName + "\n";
//             response += "Clients with this file:\n";

//             for (const auto& client : fileChunk.clientChunks) {
//                 response += "  Client IP: " + std::to_string(client.first) + " (Chunks: ";
//                 for (int idx : client.second) {
//                     response += std::to_string(idx) + " ";
//                 }
//                 response += ")\n";
//             }
//             response += "\n"; // Separate entries with a blank line
//         }

//         send(new_socket, response.c_str(), response.size(), 0);
//     } else {
//         std::string errorMsg = "No files available for download.\n";
//         send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
//     }
// }
   
    else if (receivedMessage.rfind("all_data", 0) == 0) {
        std::lock_guard<std::mutex> lock(db_mutex); // Lock for thread safety

    // Print details of all files in fileStorage
        if (!fileStorage.empty()) {
        // std::cout << "Files in storage:" << std::endl;

        for (const auto& entry : fileStorage) {
            const std::string& fileName = entry.first;
            const FileChunk& fileChunk = entry.second;

            // std::cout << "File: " << fileName << std::endl;
            // std::cout << "Total Chunks: " << fileChunk.totalChunks << std::endl;
            // std::cout << "File Hash: " << fileChunk.fileHash << std::endl; // Print the file hash
            // std::cout << "Clients with chunks:" << std::endl;

            // for (const auto& client : fileChunk.clientChunks) {
            //     std::cout << "  Client IP: " << to_string(client.first) << ", Chunks: ";
            //     for (int idx : client.second) {
            //         std::cout << idx << " (SHA: " << fileChunk.chunkHashes[idx] << ") "; // Add SHA value for each chunk
            //     }
            //     std::cout << std::endl;
            // }
            // std::cout << std::endl; // Add a blank line between files for better readability
        }
        string successMsg="File info printed successfully on tracker";
        send(new_socket, successMsg.c_str(), successMsg.size(), 0);
    } else {
        std::cout << "No files found in storage." << std::endl;
    }
}

        else {
            cout<<"UNKNKAJSFIKOHJAFISHGOIAHGOVIADHOGAID\n";
            std::string errorMsg = "Unknown command "+receivedMessage+"\n";
            send(new_socket, errorMsg.c_str(), errorMsg.size(), 0);
        }
    }

    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " tracker_info.txt tracker_number" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string filename = argv[1];
    int tracker_no = stoi(argv[2]) - 1;

    std::ifstream file(filename);
    std::vector<ConnectionInfo> connections;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return EXIT_FAILURE;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        ConnectionInfo conn;
        if (iss >> conn.ipAddress >> conn.portNumber) {
            connections.push_back(conn);
        } else {
            std::cerr << "Error: Invalid format in line: " << line << std::endl;
            return EXIT_FAILURE;
        }
    }

    file.close();

    if (tracker_no < 0 || tracker_no >= connections.size()) {
        std::cout << "No tracker with associated number found\n";
        return EXIT_FAILURE;
    }

    std::cout << "Trying to connect to:\n";
    std::cout << "IP Address: " << connections[tracker_no].ipAddress << " and Port Number: " << connections[tracker_no].portNumber << std::endl;

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(connections[tracker_no].ipAddress.c_str());
    address.sin_port = htons(connections[tracker_no].portNumber);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }

    std::cout << "Listening for connections on " 
              << connections[tracker_no].ipAddress 
              << ":" << connections[tracker_no].portNumber << "...\n";

    while (true) {
        int new_socket;
        struct sockaddr_in client_address;
        int client_addrlen = sizeof(client_address);

        // Accept a new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t*)&client_addrlen)) < 0) {
            perror("accept");
            continue; // Continue to accept the next connection
        }

        // Create a new thread to handle the client
        int* socketPtr = new int(new_socket); // Allocate memory for the socket
        pthread_t thread_id;
        if (pthread_create(&thread_id, nullptr, handleClient, socketPtr) != 0) {
            perror("Failed to create thread");
            close(new_socket);
            delete socketPtr; // Clean up on failure
        }
        pthread_detach(thread_id); // Detach the thread to prevent resource leaks
    }

    close(server_fd);
    return 0;
}
