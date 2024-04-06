#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::chrono_literals;

constexpr size_t BUFFER_SIZE = 1024;

std::string read_request(tcp::socket& socket) {
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, "\r\n\r\n");
    std::string request(boost::asio::buffer_cast<const char*>(buf.data()), buf.size());
    return request;
}

void parse_request(const std::string& request, std::string& host, std::string& path) {
    // Extract host and path from the request
    // This is a simplistic implementation, you may need to enhance it
    size_t host_start = request.find("Host: ") + 6;
    size_t host_end = request.find("\r\n", host_start);
    host = request.substr(host_start, host_end - host_start);

    size_t path_start = request.find(" ", request.find("GET")) + 1;
    size_t path_end = request.find(" ", path_start);
    path = request.substr(path_start, path_end - path_start);
}

void handle_client(tcp::socket socket, std::string dest_host, unsigned short dest_port, io_context& io_ctx, yield_context yield) {
    try {
        std::string request = read_request(socket);
        std::cout << "Received request: " << request << std::endl;
        std::string host, path;
        parse_request(request, host, path);

        tcp::resolver resolver(io_ctx);
        tcp::resolver::query query(dest_host, std::to_string(dest_port));
        auto endpoint_iterator = resolver.async_resolve(query, yield);

        tcp::socket dest_socket(io_ctx);
        boost::asio::async_connect(dest_socket, endpoint_iterator, yield);

        boost::asio::async_write(dest_socket, buffer(request), yield);

        char data[BUFFER_SIZE];
        size_t length = dest_socket.async_read_some(buffer(data), yield);
        std::string response(data, length);

        std::cout << "Received response: " << response << std::endl;

        boost::asio::async_write(socket, buffer(response), yield);

    } catch (const std::exception& e) {
        std::cerr << "Exception in handle_client: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: ./a.out <local_port> <destination_host> <destination_port>" << std::endl;
            return 1;
        }

        unsigned short local_port = std::atoi(argv[1]);
        std::string dest_host = argv[2];
        unsigned short dest_port = std::atoi(argv[3]);

        io_context io_ctx;

        tcp::acceptor acceptor(io_ctx, tcp::endpoint(tcp::v4(), local_port));

        for (;;) {
            tcp::socket socket(io_ctx);
            acceptor.async_accept(socket, [&](const boost::system::error_code& error) {
                if (!error) {
                    boost::asio::spawn(io_ctx, [&](yield_context yield) {
                        handle_client(std::move(socket), dest_host, dest_port, io_ctx, yield);
                    });
                }
            });
            io_ctx.run();
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

