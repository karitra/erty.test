#include <iostream>

#include <atomic>
#include <vector>
#include <thread>
#include <memory>
#include <exception>

#include <pplx/pplxtasks.h>

#include "liberty/liberty.hpp"

namespace conf {
    constexpr auto TEST_URI = "http://localhost:8877/v1/distribution";
}

std::atomic_int deleted;

struct self_ref_t {

    self_ref_t(const int i) : num(i) {}

    ~self_ref_t() {
        std::cerr << "self_ref_t::~dtor(" << this->num << ")\n";
        ++deleted;
    }

    auto boo() -> void {
        std::cerr << "boo " << num << '\n';
    }

    int num;
};

template<class Client>
auto run_iters(Client&& client, const std::string& uri, const int iters) -> std::vector<pplx::task<std::string>> {
    std::vector<pplx::task<std::string>> tasks;

    for (int i = 0; i < iters; ++i) {
        pplx::task_completion_event<std::string> event;
        auto request = liberty::http_request{};

        auto self = std::make_shared<self_ref_t>(i);

        request.set_request_uri(uri);
        request.set_complete_callback([=] (const liberty_http_error* error, const liberty_http_response* response) {
            if (error) {
                std::cerr << "Or, noh! Error!\n";
                event.set_exception(std::runtime_error(std::string(liberty_error_extra(error), liberty_error_extra_size(error))));
                return;
            }

            if (response) {
                std::cerr << "Response\n";
                auto code = liberty_http_response_code(response);
                auto body = std::string(liberty_http_response_body(response), liberty_http_response_body_size(response));

                std::cerr << "Code is " << code << '\n';

                if (code == 200) {
                    event.set(std::move(body));
                    self->boo();
                } else {
                    std::cerr << "Body is " << body << '\n';
                    event.set_exception(std::runtime_error(std::move(body)));
                }
            }
        });

        tasks.emplace_back(event);
        client.perform(std::move(request));

    } // for

    return tasks;
}

auto main(int argc, const char *argv[]) -> int {
    std::cerr << "Starting tests...\n";

    auto client = std::make_shared<liberty::http_client>();

    for(auto&& task: run_iters(*client, conf::TEST_URI, 1000)) {
        task.then([=] (pplx::task<std::string> task) {
            try {
                task.get();
                std::cerr << "Bingo!\n";
            } catch (const std::exception& e) {
                std::cerr << "Exception: " << e.what() << '\n';
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.reset();

    std::cerr << "deleted successfully " << deleted << '\n';
    return EXIT_SUCCESS;
}
