#pragma once

#include <string>
#include <string_view>

namespace minigit::update {

class UpdateNotifier {
public:
    static UpdateNotifier& instance();

    bool is_notification_enabled() const;

    void check_and_notify(std::string_view command);

    void print_update_banner(std::string_view current_ver, std::string_view latest_ver, std::string_view url = "");

    void set_cache_path(const std::string& path);
    std::string get_cache_path() const;

    void set_check_interval_seconds(int seconds);
    int get_check_interval_seconds() const;

private:
    UpdateNotifier();
    std::string custom_cache_path_;
    int check_interval_seconds_{86400}; // 24 hours
};

} // namespace minigit::update
