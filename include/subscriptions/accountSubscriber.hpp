#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

#include "wssSubscriber.hpp"

namespace mango_v3 {
namespace subscription {

template <class T>
class accountSubscriber {
 public:
  template <typename... Args>
  accountSubscriber(const std::string &name, Args... args)
      : wssConnection(name), account(std::make_shared<T>(args...)) {}

  template <typename func>
  void registerUpdateCallback(func &&callback) {
    notifyCb = std::forward<func>(callback);
  }

  template <typename func>
  void registerCloseCallback(func &&callback) {
    closeCb = std::forward<func>(callback);
    ;
  }

  std::shared_ptr<const T> getAccount() { return account; }

  void subscribe() {
    wssConnection.registerOnMessageCallback(
        std::bind(&accountSubscriber::onMessage, this, std::placeholders::_1));
    wssConnection.registerOnCloseCallback(
        std::bind(&accountSubscriber::onClose, this));
    wssConnection.start();
  }

 private:
  void onMessage(const nlohmann::json &msg) {
    if (account->handleMsg(msg)) {
      if (notifyCb) {
        notifyCb();
      }
    }
  }
  void onClose() {
    if (closeCb) {
      closeCb();
    }
  }

  std::shared_ptr<T> account;
  wssSubscriber wssConnection;
  std::function<void()> notifyCb;
  std::function<void()> closeCb;
};
}  // namespace subscription
}  // namespace mango_v3
