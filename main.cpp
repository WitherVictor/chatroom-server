#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/system/detail/error_code.hpp>
#include <exception>
#include <fstream>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;
namespace asio = boost::asio;

//  用于 socket_list 的 mutex
std::mutex socket_list_mutex{};

//  存储 socket 列表的 socket_list
std::vector<std::shared_ptr<asio::ip::tcp::socket>> socket_list{};

std::mutex message_queue_mutex{};

//  存储消息队列
std::queue<std::string> message_queue{};

void read_message(std::shared_ptr<asio::ip::tcp::socket> socket_shared_ptr)
{
    try
    {
        while (true)
        {
            //  存储数据的缓冲区
            std::array<unsigned char, 4096> buffer{};

            //  读取数据后的状态码
            boost::system::error_code error_code{};

            //  
            auto read_length = socket_shared_ptr->read_some(asio::buffer(buffer), error_code);
            std::string read_message = {buffer.data(), buffer.data() + read_length};
            spdlog::info("Message read : {}, length: {}", read_message, read_length);

            //  如果链接已经断开
            if (error_code == boost::asio::error::eof)
            {
                spdlog::info("Client disconnected");
                socket_shared_ptr->close();
                break;
            }

            std::unique_lock lock{ message_queue_mutex };
            message_queue.push(read_message);
        }
    }
    catch (std::exception& e)
    {
        spdlog::critical("{}", e.what());
    }
}

void broadcast_message()
{
    try
    {
        while (true)
        {
            if (!message_queue.empty())
            {
                std::unique_lock lock{message_queue_mutex};             //  获取消息队列的锁
                auto message = message_queue.front();   //  获得队列中队首的消息
                message_queue.pop();                                        //  弹出队首的元素
                lock.unlock();      //  对消息队列的访问已完成, 解锁队列
                
                std::unique_lock socket_list_lock{socket_list_mutex};
                for (auto iter = socket_list.begin(); iter != socket_list.end(); )
                {
                    if ((*iter)->is_open())
                    {
                        (*iter)->write_some(asio::buffer(message));
                        iter++;
                    }
                    else
                    {
                        iter = socket_list.erase(iter);
                    }
                }
            }
        }
    }
    catch (std::exception& e)
    {
        spdlog::critical("{}", e.what());
    }
}

int main()
{
    //  打开配置文件
    std::ifstream config_json{ "config.json" };
    spdlog::info("Reading config json...");

    //  关联 json 对象到配置文件
    auto json = json::parse(config_json);
    auto port_string = json["port"].get<std::string>();

    if (port_string.empty())
    {
        spdlog::critical("Port cannot be null !");
        throw std::logic_error{"Port read from config file is null"};
    }

    //  将字符串解析为数字
    std::uint_least16_t port{};
    std::from_chars(port_string.data(), port_string.data() + port_string.size(), port);

    asio::io_context io_context{}; 
    asio::ip::tcp::acceptor acceptor{ io_context, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}};

    //  如果 acceptor 已经开放
    if (acceptor.is_open())
        spdlog::info("Successfully opened, listening to port {}", port);
    else
    {
        spdlog::critical("Failed listening to port {}", port);
        throw std::logic_error{ std::format("Cannot listen to the server port {}", port) };
    }

    //  存储读入信息的缓冲区
    constexpr std::size_t buffer_size{ 4096 };
    std::array<unsigned char, buffer_size> buffer{};

    std::jthread{broadcast_message}.detach();

    while (true)
    {
        //  新创建 socket, 保存到 shared_ptr 内供线程使用
        auto socket_shared_ptr = std::make_shared<asio::ip::tcp::socket>(io_context);

        //  等待新链接连入
        acceptor.accept(*socket_shared_ptr);

        //  创建线程监听客户端发送消息
        std::jthread{read_message, socket_shared_ptr}.detach();

        //  对客户端 socket 队列加锁, 将当前链接放入维护队列内
        std::unique_lock guard{socket_list_mutex};
        socket_list.push_back(socket_shared_ptr);
    }
}