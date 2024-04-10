#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <vector>
#include "client.h"

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: client <server> <port> <num_clients>\n";
            return 1;
        }

        std::string server = argv[1];
        std::string port = argv[2];
        int num_clients = std::stoi(argv[3]);

        boost::asio::io_context io_context;
        std::vector<std::shared_ptr<Client>> clients;

        // Prevent io_context from running out of work
        boost::asio::io_context::work work(io_context);
        boost::thread_group threads;

        // Create specified number of clients and store them in a vector
        for (int i = 0; i < num_clients; ++i) {
            auto client = std::make_shared<Client>(io_context, server, port, i);
            client->start();
            clients.push_back(client);
        }

        // Use a thread pool to run the io_context
        for (unsigned i = 0; i < boost::thread::hardware_concurrency(); ++i) {
            threads.create_thread([&io_context]() { io_context.run(); });
        }

        threads.join_all();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
