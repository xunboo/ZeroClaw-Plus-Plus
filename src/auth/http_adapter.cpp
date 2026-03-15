/// auth/http_adapter.cpp — bridges auth::HttpClient interface with http::HttpClient
///
/// This file provides the create_http_client() factory that returns
/// an adapter implementing auth::HttpClient backed by http::HttpClient.

#include "oauth_provider.hpp"
#include "../http/http_client.hpp"

namespace zeroclaw {
    namespace auth {

        /// Adapter that implements auth::HttpClient using http::HttpClient
        class HttpClientAdapter : public HttpClient {
        public:
            HttpClientAdapter() = default;
            ~HttpClientAdapter() override = default;

            std::string post_form(const std::string& url,
                                  const std::map<std::string, std::string>& form_data) override {
                auto [status, body] = post_form_with_status(url, form_data);
                if (status < 200 || status >= 300) {
                    throw std::runtime_error("HTTP POST failed (" +
                        std::to_string(status) + "): " + body);
                }
                return body;
            }

            std::pair<int, std::string> post_form_with_status(
                const std::string& url,
                const std::map<std::string, std::string>& form_data) override {

                auto resp = client_.post_form(url, form_data);
                return {resp.status, resp.body};
            }

        private:
            http::HttpClient client_;
        };

        std::unique_ptr<HttpClient> create_http_client() {
            return std::make_unique<HttpClientAdapter>();
        }

    }
}
