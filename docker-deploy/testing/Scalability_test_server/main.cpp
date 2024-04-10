#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <vector>
#include "Client.h"

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
        boost::asio::io_context::work work(io_context);
        boost::thread_group threads;
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < num_clients; ++i) {
            auto client = std::make_shared<Client>(io_context, server, port, i);
            client->start();
            clients.push_back(client);
        }

        for (unsigned i = 0; i < boost::thread::hardware_concurrency(); ++i) {
            threads.create_thread([&io_context]() { io_context.run(); });
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double average_time = duration / (num_clients * 2);
        int throughtput_perSec = 10000000 / average_time;
        std::cout << "Average time per request and response pair in us: " << average_time  / 1000 << " us" << std::endl;
        //std::cout << "Average time per request and response pair in ms: " << average_time / 1000000 << " ms" << std::endl;
        std::cout << "Average throughput: " << throughtput_perSec << std::endl;
        threads.join_all();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

