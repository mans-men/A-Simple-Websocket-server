#ifndef CLIENT_WSS_HPP
#define CLIENT_WSS_HPP

#include "client_ws.hpp"

#ifdef USE_STANDALONE_ASIO
#include <asio/ssl.hpp>
#else
#include <boost/asio/ssl.hpp>
#endif

namespace SimpleWeb {
  typedef asio::ssl::stream<asio::ip::tcp::socket> WSS;

  template <>
  class SocketClient<WSS> : public SocketClientBase<WSS> {
  public:
    SocketClient(const std::string &server_port_path, bool verify_certificate = true,
                 const std::string &cert_file = std::string(), const std::string &private_key_file = std::string(),
                 const std::string &verify_file = std::string())
        : SocketClientBase<WSS>::SocketClientBase(server_port_path, 443), context(asio::ssl::context::tlsv12) {
      if(cert_file.size() > 0 && private_key_file.size() > 0) {
        context.use_certificate_chain_file(cert_file);
        context.use_private_key_file(private_key_file, asio::ssl::context::pem);
      }

      if(verify_certificate)
        context.set_verify_callback(asio::ssl::rfc2818_verification(host));

      if(verify_file.size() > 0)
        context.load_verify_file(verify_file);
      else
        context.set_default_verify_paths();

      if(verify_file.size() > 0 || verify_certificate)
        context.set_verify_mode(asio::ssl::verify_peer);
      else
        context.set_verify_mode(asio::ssl::verify_none);
    };

  protected:
    asio::ssl::context context;

    void connect() override {
      std::unique_lock<std::mutex> connection_lock(connection_mutex);
      auto connection = this->connection = std::shared_ptr<Connection>(new Connection(this->handler_runner, *io_service, context));
      connection_lock.unlock();
      asio::ip::tcp::resolver::query query(host, std::to_string(port));
      auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_service);
      resolver->async_resolve(query, [this, connection, resolver](const error_code &ec, asio::ip::tcp::resolver::iterator it) {
        auto lock = connection->handler_runner->continue_lock();
        if(!lock)
          return;
        if(!ec) {
          asio::async_connect(connection->socket->lowest_layer(), it, [this, connection, resolver](const error_code &ec, asio::ip::tcp::resolver::iterator /*it*/) {
            auto lock = connection->handler_runner->continue_lock();
            if(!lock)
              return;
            if(!ec) {
              asio::ip::tcp::no_delay option(true);
              connection->socket->lowest_layer().set_option(option);

              connection->socket->async_handshake(asio::ssl::stream_base::client, [this, connection](const error_code &ec) {
                auto lock = connection->handler_runner->continue_lock();
                if(!lock)
                  return;
                if(!ec)
                  handshake(connection);
                else if(on_error)
                  on_error(connection, ec);
              });
            }
            else if(on_error)
              on_error(connection, ec);
          });
        }
        else if(on_error)
          on_error(connection, ec);
      });
    }
  };
} // namespace SimpleWeb

#endif /* CLIENT_WSS_HPP */