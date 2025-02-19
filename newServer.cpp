#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <csignal>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

class MyServer {
private:
    static constexpr int MAX_CLIENTS = 6;
    static constexpr int PORT = 12345;
    static constexpr int BUFFER_SIZE = 1024;
    static constexpr const char* ID_FILE = "./last_id";

    int server_fd;
    std::vector<std::thread> client_threads;
    std::vector<int> client_sockets;
    std::mutex clients_mutex;
    std::atomic<bool> running{true};
    std::atomic<uint32_t> last_id{0};
    std::mutex id_mutex;

    static void signal_handler(int signal) 
    {
        if (signal == SIGINT) 
        {
            getInstance().shutdown();
        }
    }

    // load last ID from persistent file
    void loadLastId() 
    {
        std::ifstream file(ID_FILE);
        if (file) 
        {
            uint32_t stored_id;
            file >> stored_id;
            last_id.store(stored_id);
        }
    }

    // save last ID to file
    void saveLastId() 
    {
        std::ofstream file(ID_FILE);
        if (file) 
        {
            file << last_id.load();
        }
    }

    uint32_t generateUniqueId() 
    {
        std::lock_guard<std::mutex> lock(id_mutex);
        
        // get current
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&now_c);
        
        // calc secs since midnight
        uint32_t seconds_today = tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
        
        // calc a unique id based on time and last id
        uint32_t new_id = (seconds_today << 16) | ((last_id.load() + 1) & 0xFFFF);
        last_id.store(new_id & 0xFFFF);
        
        saveLastId();
        return new_id;
    }

    MyServer() 
    {
        loadLastId();
        
        struct sockaddr_in address;
        int opt = 1;
        
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
        {
            throw std::runtime_error("socket creation failed");
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) 
        {
            throw std::runtime_error("setsockopt failed");
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) 
        {
            throw std::runtime_error("bind failed");
        }

        if (listen(server_fd, MAX_CLIENTS) < 0) 
        {
            throw std::runtime_error("listen failed");
        }

        std::signal(SIGINT, signal_handler);
    }

public:
    static MyServer& getInstance() 
    {
        static MyServer instance;
        return instance;
    }

    void broadcast(const std::string& message) 
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int socket : client_sockets) 
        {
            send(socket, message.c_str(), message.length(), 0);
        }
    }

    void handleClient(int client_socket) 
    {
        char buffer[BUFFER_SIZE];
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets.push_back(client_socket);
        }

        while (running) 
        {
            uint32_t id = generateUniqueId();
            std::string id_str = std::to_string(id) + "\n";
            
            if (send(client_socket, id_str.c_str(), id_str.length(), 0) <= 0) {
                break;
            }

            fd_set readfds;
            struct timeval tv;
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            if (select(client_socket + 1, &readfds, NULL, NULL, &tv) > 0) {
                int valread = recv(client_socket, buffer, BUFFER_SIZE, 0);
                if (valread <= 0)
                    break;

                // check for newline
                for (int i = 0; i < valread; i++) {
                    if (buffer[i] == '\n') 
                    {
                        std::string count_msg = std::to_string(client_sockets.size()) + "\n";
                        broadcast(count_msg);
                    }
                }
            }
        }

        // remove client socket from list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets.erase(
                std::remove(client_sockets.begin(), client_sockets.end(), client_socket),
                client_sockets.end()
            );
        }
        
        close(client_socket);
    }

    void shutdown() 
    {
        running = false;
        broadcast("Thank you\n");
        
        // clean shutdown of all client connections
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (int socket : client_sockets) 
            {
                close(socket);
            }
            client_sockets.clear();
        }
        
        // wait for all client threads to finish
        for (auto& thread : client_threads) 
        {
            if (thread.joinable()) 
                thread.join();
        }
        
        close(server_fd);
    }

    void run() 
    {
        std::cout << "used server port " << PORT << std::endl;
        
        while (running) 
        {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            
            fd_set readfds;
            struct timeval tv;
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            if (select(server_fd + 1, &readfds, NULL, NULL, &tv) > 0) 
            {
                int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
                
                if (client_socket >= 0) 
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    if (client_sockets.size() < MAX_CLIENTS) 
                    {
                        client_threads.emplace_back(&MyServer::handleClient, this, client_socket);
                    } else 
                    {
                        send(client_socket, "server full\n", 12, 0);
                        close(client_socket);
                    }
                }
            }
        }
    }
};

int main() {
    try {
        MyServer::getInstance().run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}