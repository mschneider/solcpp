#include <unistd.h>
#include <chrono>
#include <nlohmann/json.hpp>
#include <vector>
#include "websocket.hpp"
#include <thread>

using json = nlohmann::json;

class WebSocketSubscriber
{
public:
    net::io_context ioc;
    std::shared_ptr<session> sess;
    std::thread read_thread;
    std::thread write_thread;
    int curr_id = 0;
    std::vector<std::string> available_commitment;

    WebSocketSubscriber(std::string host = "api.devnet.solana.com",
                        std::string port = "80")
    {
        // create a new session
        sess = std::make_shared<session>(ioc);
        // function to read
        auto read_fn = [=]()
        {
            //std::cout << host << " " << port << std::endl;
            sess->run(host, port);
            ioc.run();
        };

        // function to write
        auto write_fn = [&]()
        {
            sess->write();
            ioc.run();
        };

        // add commitments to available commitment
        fill_available_commitment();

        // writing using one thread and read using another
        read_thread = std::thread(read_fn);
        write_thread = std::thread(write_fn);
    }
    ~WebSocketSubscriber()
    {
        // disconnect the session and wait for the threads to complete
        sess->disconnect();
        if(read_thread.joinable())
            read_thread.join();
        if(write_thread.joinable())
            write_thread.join();
    }

    /// @brief fill all the available commitments
    void fill_available_commitment()
    {
        available_commitment.push_back("processed");
        available_commitment.push_back("confirmed");
        available_commitment.push_back("finalized");
    }

    /// @brief check if commitment is part of available commitment
    /// @param commitment the commitment you want to check
    /// @return true or false
    bool check_commitment_integrity(std::string commitment)
    {
        for (auto com : available_commitment)
        {
            if (com == commitment)
                return true;
        }
        return false;
    }
    /// @brief callback to call when data in account changes
    /// @param pub_key public key for the account
    /// @param account_change_callback callback to call when the data changes
    /// @param commitment commitment
    /// @return subsccription id (actually the current id)
    int onAccountChange(
        std::string pub_key,
        Callback account_change_callback,
        std::string commitment = "finalized")
    {
        // return -1 if commitment is not part of available commitment
        if (!check_commitment_integrity(commitment))
            return -1;

        // create parameters using the user provided input
        json param = {
            pub_key,
            {{"encoding", "base64"},
             {"commitment", commitment}}};

        // create a new request content
        std::shared_ptr<RequestContent> req(new RequestContent(curr_id, "accountSubscribe", "accountUnsubscribe", account_change_callback, param));

        // subscribe the new request content
        sess->subscribe(req);

        // increase the curr_id so that it can be used for the next request content
        curr_id += 2;

        return req->id_;
    }

    /// @brief remove the account change listener for the given id
    /// @param sub_id the id for which removing subscription is needed
    void removeAccountChangeListener(int sub_id)
    {
        sess->unsubscribe(sub_id);
    }
};

bool subscribe_called = false;
void call_on_subscribe(json &data) { subscribe_called = !subscribe_called; }

int main()
{
    WebSocketSubscriber sub;
    int sub_id = sub.onAccountChange("8vnAsXjHtdRgyuFHVESjfe1CBmWSwRFRgBR3WJCQbMiW", call_on_subscribe);
    // stop execution here and call solana airdrop 1 from terminal
    sleep(10);
    std::cout << "Subscribe Called Value " << subscribe_called << std::endl;
    sub.removeAccountChangeListener(sub_id);
    // call solana airdrop 1
    sleep(10);
    std::cout << "Subscribe Called Value " << subscribe_called << std::endl;
}
