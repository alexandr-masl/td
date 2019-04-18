//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2018
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <td/telegram/Client.h>
#include <td/telegram/Log.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <sqlite3.h>

// m 797687104 💎 Получить монетку

// Simple single-threaded example of TDLib usage.
// Real world programs should use separate thread for the user input.
// Example includes user authentication, receiving updates, getting chat list and sending text messages.

// overloaded
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;

class TdExample {
 public:
  TdExample() {
    td::Log::set_verbosity_level(1);
    client_ = std::make_unique<td::Client>();
    sqlite3_open("tg_members.db", &m_db_ptr);
    create();
  }

  bool query_write(std::string sql)
   {
      char* messaggeError; 
      int rez = sqlite3_exec(m_db_ptr, sql.c_str(), NULL, 0, &messaggeError); 
   
      if (rez != SQLITE_OK) { 
         std::cerr << "Error in query: " << messaggeError << std::endl; 
         std::cerr << sql << std::endl;
         sqlite3_free(messaggeError); 
      } 
      else
         // std::cout << "Successfull query !" << std::endl; 

      return rez == SQLITE_OK;
   }

   bool query_read(std::string sql, std::vector<std::string>& rez_list, unsigned int cols)
   {
      sqlite3_stmt*        stmt;
      const unsigned char* text;

      int rez = sqlite3_prepare(m_db_ptr, sql.c_str(), sql.length(), &stmt, NULL);

      if (rez == SQLITE_ERROR)
      {
         printf( std::string(sqlite3_errmsg(m_db_ptr)).c_str() );
         printf( sql.c_str() );
         return false;
      }

      bool done = false;
      while (!done) {
         switch (sqlite3_step (stmt)) {
            case SQLITE_ROW:
               char* ch;
               for (unsigned int i = 0; i < cols; i++)
               {
                  text  = sqlite3_column_text(stmt, i);

                  ch = (char*)text;
                  rez_list.push_back(std::string(ch));
               }
               
               break;

            case SQLITE_DONE:
               done = true;
               break;

            default:
               printf( std::string(sqlite3_errmsg(m_db_ptr)).c_str() );
               return false;
         }
      }

      sqlite3_finalize(stmt);

      return true;
   }

   bool create()
   {
      std::string sql = "CREATE TABLE IF NOT EXISTS TG_USERS("
                           "USER_ID                         BIGINT                                 NOT NULL, "
                           // "CHAT_ID                         BIGINT                                 NOT NULL, "
                           "FIRST_NAME                      VARСHAR(255)                           NOT NULL, "
                           "LAST_NAME                       VARСHAR(255)                           NOT NULL, "
                           "USERNAME                        VARCHAR(255)                           NOT NULL "
                           // "PHONE_NUMBER                    VARCHAR(255)                           NOT NULL "
                        ");";
      return query_write(sql);
   }

  void loop() {
    while (true) {
      if (need_restart_) {
        restart();
      } else if (!are_authorized_) {
        process_response(client_->receive(10));
      } else {
        std::cerr << "Enter action [q] quit [u] check for updates and request results [c] show chats [m <id> <text>] "
                     "send message [l] logout: "
                  << std::endl;
        std::string line;
        std::getline(std::cin, line);
        std::istringstream ss(line);
        std::string action;
        if (!(ss >> action)) {
          continue;
        }
        if (action == "q") {
          return;
        }
        if (action == "u") {
          update();
        } else if (action == "l") {
          std::cerr << "Logging out..." << std::endl;
          send_query(td_api::make_object<td_api::logOut>(), {});
        } else if (action == "x") {
          std::string title;
          ss >> title;
          bool is_channel;
          ss >> is_channel;
          std::string description;
          ss >> description;

          std::cerr << "Creating new supergroup..." << std::endl;
          send_query(td_api::make_object<td_api::createNewSupergroupChat>(title, is_channel, description),[this](Object object) {
                       if (object->get_id() == td_api::error::ID) {
                         return;
                       }
                       auto cht = td::move_tl_object_as<td_api::chat>(object);
                       std::cerr << "[id:" << cht->id_ << "] [title:" << cht->title_ << "] [super_group_id:" << td::move_tl_object_as<td_api::chatTypeSupergroup>(cht->type_)->supergroup_id_ << "]" << std::endl;
                     });
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
          update();
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          update();
        } else if (action == "y") {
          std::int64_t chat_id;
          ss >> chat_id;

          std::cerr << "Adding members to new supergroup..." << std::endl;

              send_query(td_api::make_object<td_api::addChatMember>(chat_id, 510783846, 50), {});

               std::this_thread::sleep_for(std::chrono::milliseconds(500));
               update();

               send_query(td_api::make_object<td_api::addChatMember>(chat_id, 328938922, 50), {});

               std::this_thread::sleep_for(std::chrono::milliseconds(500));
               update();

            std::string sql = "SELECT COUNT(USER_ID) FROM TG_USERS;";
            std::vector<std::string> vec;
            bool flag = query_read(sql, vec, 1);

            if (false == flag) break;

            uint64_t count = strtoull(vec[0].c_str(), NULL, 10);
          
            for (uint64_t i=0; i < count; i++)
            {
               std::vector<std::string> vec;
               bool flag = query_read("SELECT USER_ID FROM TG_USERS LIMIT " + std::to_string(i) + ", 1", vec, 1);

               if (false == flag || 0 == vec.size()) break;

               std::int32_t user_id = strtoull(vec[0].c_str(), NULL, 10);

               std::cerr << user_id << std::endl;

               send_query(td_api::make_object<td_api::addChatMember>(chat_id, user_id, 50), {});

               std::this_thread::sleep_for(std::chrono::milliseconds(500));
               update();
            }
        } else if (action == "s") {
          std::string username;
          ss >> username;

          std::cerr << "Searching public chat..." << std::endl;
          send_query(td_api::make_object<td_api::searchPublicChat>(username),
                     [this](Object object) {
                       if (object->get_id() == td_api::error::ID) {
                         return;
                       }
                       auto cht = td::move_tl_object_as<td_api::chat>(object);
                       std::cerr << "[id:" << cht->id_ << "] [title:" << cht->title_ << "] [super_group_id:" << td::move_tl_object_as<td_api::chatTypeSupergroup>(cht->type_)->supergroup_id_ << "]" << std::endl;
                     });
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
          update();
        } else if (action == "f") {
          std::int32_t group_id;
          ss >> group_id;
          std::int32_t offset;
          ss >> offset;
          std::int32_t limit;
          ss >> limit;
          std::int32_t count;
          ss >> count;

          std::cerr << "Getting basic group full info..." << std::endl;
          for (int32_t j=0; j < count; j++) {
            send_query(td_api::make_object<td_api::getSupergroupMembers>(group_id, td_api::make_object<td_api::supergroupMembersFilterSearch>(), offset + limit * j, limit),
                        [this, limit](Object object) {
                        if (object->get_id() == td_api::error::ID) {
                           return;
                        }
                        auto info = td::move_tl_object_as<td_api::chatMembers>(object);
                        std::cerr << "[total_count:" << info->total_count_ << "]" << std::endl;
                        for (int32_t i=0; i < limit; i++)
                        {
                           if (info->members_.size() < limit)
                                 break;

                           send_query(td_api::make_object<td_api::getUser>(info->members_[i]->user_id_),
                              [this](Object object) {
                                 if (object->get_id() == td_api::error::ID) {
                                    return;
                                 }

                                 auto usr = td::move_tl_object_as<td_api::user>(object);

                                 std::cerr << "[username:" << usr->username_ << "] [user_id:" << usr->id_ << "]" << std::endl;

                                 std::string sql = "INSERT INTO TG_USERS (USER_ID, FIRST_NAME, LAST_NAME, USERNAME) VALUES (" + std::to_string(usr->id_) + ", '" + usr->first_name_ + "', '" + usr->last_name_ + "', '" + usr->username_ + "');";

                                 bool flag = query_write(sql);
                              }
                           );
                        }
                        });
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        update();
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        update();
          }
        } else if (action == "m") {
          std::int64_t chat_id;
          ss >> chat_id;
          ss.get();
          std::string text;
          std::getline(ss, text);

         // for (int i=0; i < 1000; i++) {
          std::cerr << "Sending message to chat " << chat_id << "..." << text << std::endl;
          auto send_message = td_api::make_object<td_api::sendMessage>();
          send_message->chat_id_ = chat_id;
          auto message_content = td_api::make_object<td_api::inputMessageText>();
          message_content->text_ = td_api::make_object<td_api::formattedText>();
          message_content->text_->text_ = text;
          send_message->input_message_content_ = std::move(message_content);

          send_query(std::move(send_message), {});

          std::this_thread::sleep_for(std::chrono::milliseconds(50));
         // }
        } else if (action == "c") {
          std::cerr << "Loading chat list..." << std::endl;
          send_query(td_api::make_object<td_api::getChats>(std::numeric_limits<std::int64_t>::max(), 0, 20),
                     [this](Object object) {
                       if (object->get_id() == td_api::error::ID) {
                         return;
                       }
                       auto chats = td::move_tl_object_as<td_api::chats>(object);
                       for (auto chat_id : chats->chat_ids_) {
                         std::cerr << "[id:" << chat_id << "] [title:" << chat_title_[chat_id] << "]" << std::endl;
                       }
                     });
        }
      }
    }
  }

 private:
   sqlite3 *m_db_ptr;

  using Object = td_api::object_ptr<td_api::Object>;
  std::unique_ptr<td::Client> client_;

  td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
  bool are_authorized_{false};
  bool need_restart_{false};
  std::uint64_t current_query_id_{0};
  std::uint64_t authentication_query_id_{0};

  std::map<std::uint64_t, std::function<void(Object)>> handlers_;

  std::map<std::int32_t, td_api::object_ptr<td_api::user>> users_;

  std::map<std::int64_t, std::string> chat_title_;

  void restart() {
    client_.reset();
    *this = TdExample();
  }

  void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
      handlers_.emplace(query_id, std::move(handler));
    }
    client_->send({query_id, std::move(f)});
  }

  void update()
  {
     std::cerr << "Checking for updates..." << std::endl;
      while (true) {
      auto response = client_->receive(0);
      if (response.object) {
         process_response(std::move(response));
      } else {
         break;
      }
    }
  }

  void process_response(td::Client::Response response) {
    if (!response.object) {
      return;
    }
    //std::cerr << response.id << " " << to_string(response.object) << std::endl;
    if (response.id == 0) {
      return process_update(std::move(response.object));
    }
    auto it = handlers_.find(response.id);
    if (it != handlers_.end()) {
      it->second(std::move(response.object));
    }
  }

  std::string get_user_name(std::int32_t user_id) {
    auto it = users_.find(user_id);
    if (it == users_.end()) {
      return "unknown user";
    }
    return it->second->first_name_ + " " + it->second->last_name_;
  }

  void process_update(td_api::object_ptr<td_api::Object> update) {
    td_api::downcast_call(
        *update, overloaded(
                     [this](td_api::updateAuthorizationState &update_authorization_state) {
                       authorization_state_ = std::move(update_authorization_state.authorization_state_);
                       on_authorization_state_update();
                     },
                     [this](td_api::updateNewChat &update_new_chat) {
                       chat_title_[update_new_chat.chat_->id_] = update_new_chat.chat_->title_;
                     },
                     [this](td_api::updateChatTitle &update_chat_title) {
                       chat_title_[update_chat_title.chat_id_] = update_chat_title.title_;
                     },
                     [this](td_api::updateUser &update_user) {
                       auto user_id = update_user.user_->id_;
                       users_[user_id] = std::move(update_user.user_);
                     },
                     [this](td_api::updateNewMessage &update_new_message) {
                       auto chat_id = update_new_message.message_->chat_id_;
                       auto sender_user_name = get_user_name(update_new_message.message_->sender_user_id_);
                       std::string text;
                       if (update_new_message.message_->content_->get_id() == td_api::messageText::ID) {
                         text = static_cast<td_api::messageText &>(*update_new_message.message_->content_).text_->text_;
                       }
                     //   std::cerr << "Got message: [chat_id:" << chat_id << "] [from:" << sender_user_name << "] ["
                     //             << text << "]" << std::endl;
                     },
                     [](auto &update) {}));
  }

  auto create_authentication_query_handler() {
    return [this, id = authentication_query_id_](Object object) {
      if (id == authentication_query_id_) {
        check_authentication_error(std::move(object));
      }
    };
  }

  void on_authorization_state_update() {
    authentication_query_id_++;
    td_api::downcast_call(
        *authorization_state_,
        overloaded(
            [this](td_api::authorizationStateReady &) {
              are_authorized_ = true;
              std::cerr << "Got authorization" << std::endl;
            },
            [this](td_api::authorizationStateLoggingOut &) {
              are_authorized_ = false;
              std::cerr << "Logging out" << std::endl;
            },
            [this](td_api::authorizationStateClosing &) { std::cerr << "Closing" << std::endl; },
            [this](td_api::authorizationStateClosed &) {
              are_authorized_ = false;
              need_restart_ = true;
              std::cerr << "Terminated" << std::endl;
            },
            [this](td_api::authorizationStateWaitCode &wait_code) {
              std::string first_name;
              std::string last_name;
              if (!wait_code.is_registered_) {
                std::cerr << "Enter your first name: ";
                std::cin >> first_name;
                std::cerr << "Enter your last name: ";
                std::cin >> last_name;
              }
              std::cerr << "Enter authentication code: ";
              std::string code;
              std::cin >> code;
              send_query(td_api::make_object<td_api::checkAuthenticationCode>(code, first_name, last_name),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitPassword &) {
              std::cerr << "Enter authentication password: ";
              std::string password;
              std::cin >> password;
              send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitPhoneNumber &) {
              std::cerr << "Enter phone number: ";
              std::string phone_number;
              std::cin >> phone_number;
              send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                             phone_number, false /*allow_flash_calls*/, false /*is_current_phone_number*/),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitEncryptionKey &) {
              std::cerr << "Enter encryption key or DESTROY: ";
              std::string key;
              std::getline(std::cin, key);
              if (key == "DESTROY") {
                send_query(td_api::make_object<td_api::destroy>(), create_authentication_query_handler());
              } else {
                send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(std::move(key)),
                           create_authentication_query_handler());
              }
            },
            [this](td_api::authorizationStateWaitTdlibParameters &) {
              auto parameters = td_api::make_object<td_api::tdlibParameters>();
              parameters->database_directory_ = "tdlib";
              parameters->use_message_database_ = true;
              parameters->use_secret_chats_ = true;
              parameters->api_id_ = 94575;
              parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
              parameters->system_language_code_ = "en";
              parameters->device_model_ = "Desktop";
              parameters->system_version_ = "Ubuntu 18.04.2 LTS";
              parameters->application_version_ = "1.0";
              parameters->enable_storage_optimizer_ = true;
              send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                         create_authentication_query_handler());
            }));
  }

  void check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
      auto error = td::move_tl_object_as<td_api::error>(object);
      std::cerr << "Error: " << to_string(error);
      on_authorization_state_update();
    }
  }

  std::uint64_t next_query_id() {
    return ++current_query_id_;
  }
};

int main() {
  TdExample example;
  example.loop();
}
