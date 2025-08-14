// Copyright 2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cstdlib>   // For std::getenv
#include <iostream>  // For logging/debugging
#include <nlohmann/json.hpp> // JSON library
#include <cstdint>   // For uint64_t

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;


namespace make87 {
std::string sanitize_and_checksum(const std::string& input) {
    const std::string prefix = "ros2_";

    // Sanitize the input string
    std::string sanitized;
    sanitized.reserve(input.size());
    for (char c : input) {
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_') {
            sanitized += c;
        } else {
            sanitized += '_';
        }
    }

    // Compute checksum
    uint64_t sum = 0;
    for (unsigned char b : input) {
        sum = (sum * 31 + b) % 1000000007ULL;
    }
    std::string checksum = std::to_string(sum);

    // Calculate maximum allowed length for the sanitized string
    const size_t max_total_length = 256;
    const size_t prefix_length = prefix.size();
    const size_t checksum_length = checksum.size();
    size_t max_sanitized_length = max_total_length - prefix_length - checksum_length;

    // Truncate sanitized string if necessary
    if (sanitized.size() > max_sanitized_length) {
        sanitized = sanitized.substr(0, max_sanitized_length);
    }

    // Construct the final string
    return prefix + sanitized + checksum;
}

std::string resolve_topic_name(const std::string& search_topic, const std::string& default_value) {
    // Retrieve the TOPICS environment variable
    const char* env_value = std::getenv("TOPICS");
    if (!env_value) {
        std::cerr << "Environment variable TOPICS not set. Using default value.\n";
        return default_value;
    }

    try {
        // Parse the JSON string
        nlohmann::json json_obj = nlohmann::json::parse(env_value);

        // Validate structure and access the topics array
        if (json_obj.contains("topics") && json_obj["topics"].is_array()) {
            for (const auto& topic : json_obj["topics"]) {
                // Check if "topic_name" matches the search topic
                if (topic.contains("topic_name") && topic["topic_name"] == search_topic) {
                    // Return the "topic_key" if it exists
                    if (topic.contains("topic_key") && topic["topic_key"].is_string()) {
                        return sanitize_and_checksum(topic["topic_key"].get<std::string>());
                    }
                }
            }
        }

        std::cerr << "Topic " << search_topic << " not found or missing topic_key. Using default value.\n";
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Error parsing JSON from TOPICS: " << e.what() << ". Using default value.\n";
    } catch (const nlohmann::json::type_error& e) {
        std::cerr << "Error accessing JSON structure in TOPICS: " << e.what() << ". Using default value.\n";
    }

    return default_value;
}
}

/* This example creates a subclass of Node and uses std::bind() to register a
 * member function as a callback from the timer. */

class MinimalPublisher : public rclcpp::Node
{
public:
  MinimalPublisher()
  : Node("minimal_publisher"), count_(0)
  {
    publisher_ = this->create_publisher<std_msgs::msg::String>(make87::resolve_topic_name("OUTGOING_MESSAGE", "topic"), 10);
    timer_ = this->create_wall_timer(
      500ms, std::bind(&MinimalPublisher::timer_callback, this));
  }

private:
  void timer_callback()
  {
    auto message = std_msgs::msg::String();
    message.data = "Hello, world! " + std::to_string(count_++);
    RCLCPP_INFO(this->get_logger(), "Publishing: '%s'", message.data.c_str());
    publisher_->publish(message);
  }
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  size_t count_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MinimalPublisher>());
  rclcpp::shutdown();
  return 0;
}
