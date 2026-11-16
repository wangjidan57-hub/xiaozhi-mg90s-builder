#ifndef MG90S_SERVO_CONTROLLER_H
#define MG90S_SERVO_CONTROLLER_H

#include <cstdint>
#include <string>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mcp_server.h"

// MG90S 单舵机控制：50 Hz PWM，13-bit LEDC。
// 将逻辑 0..180 度映射到较保守的 600..2400 us 脉宽，降低撞击机械限位的风险。
class Mg90sServoController {
public:
    explicit Mg90sServoController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        if (gpio_num_ == GPIO_NUM_NC) {
            ESP_LOGE(TAG, "Servo GPIO is not configured");
            return;
        }

        if (!InitializePwm()) {
            ESP_LOGE(TAG, "Failed to initialize MG90S PWM on GPIO%d", gpio_num_);
            return;
        }

        initialized_ = true;
        if (SetAngle(90) != ESP_OK) {
            initialized_ = false;
            ESP_LOGE(TAG, "Failed to set the initial servo position");
            return;
        }

        RegisterMcpTools();
        ESP_LOGI(TAG, "MG90S ready: GPIO%d, initial angle=90", gpio_num_);
    }

    Mg90sServoController(const Mg90sServoController&) = delete;
    Mg90sServoController& operator=(const Mg90sServoController&) = delete;

    esp_err_t SetAngle(int angle) {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }
        if (angle < 0 || angle > 180) {
            return ESP_ERR_INVALID_ARG;
        }

        const uint32_t pulse_us =
            kMinPulseUs + (static_cast<uint32_t>(angle) *
                          (kMaxPulseUs - kMinPulseUs)) / 180U;
        const uint32_t duty = (pulse_us * kDutyMax) / kPwmPeriodUs;

        esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_7, duty);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_set_duty failed: %s", esp_err_to_name(err));
            return err;
        }

        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_7);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_update_duty failed: %s", esp_err_to_name(err));
            return err;
        }

        current_angle_ = angle;
        ESP_LOGI(TAG, "MG90S target angle: %d degrees (%lu us)",
                 angle, static_cast<unsigned long>(pulse_us));
        return ESP_OK;
    }

    int GetAngle() const {
        return current_angle_;
    }

private:
    static constexpr const char* TAG = "MG90SServo";
    static constexpr uint32_t kPwmFrequencyHz = 50;
    static constexpr uint32_t kPwmPeriodUs = 20000;
    static constexpr uint32_t kDutyResolutionBits = 13;
    static constexpr uint32_t kDutyMax = (1U << kDutyResolutionBits) - 1U;
    static constexpr uint32_t kMinPulseUs = 600;
    static constexpr uint32_t kMaxPulseUs = 2400;

    gpio_num_t gpio_num_;
    bool initialized_ = false;
    int current_angle_ = 90;

    bool InitializePwm() {
        ledc_timer_config_t timer_config = {};
        timer_config.speed_mode = LEDC_LOW_SPEED_MODE;
        timer_config.duty_resolution = LEDC_TIMER_13_BIT;
        timer_config.timer_num = LEDC_TIMER_1;
        timer_config.freq_hz = kPwmFrequencyHz;
        timer_config.clk_cfg = LEDC_AUTO_CLK;

        esp_err_t err = ledc_timer_config(&timer_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
            return false;
        }

        ledc_channel_config_t channel_config = {};
        channel_config.gpio_num = gpio_num_;
        channel_config.speed_mode = LEDC_LOW_SPEED_MODE;
        channel_config.channel = LEDC_CHANNEL_7;
        channel_config.intr_type = LEDC_INTR_DISABLE;
        channel_config.timer_sel = LEDC_TIMER_1;
        channel_config.duty = 0;
        channel_config.hpoint = 0;

        err = ledc_channel_config(&channel_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
            return false;
        }

        return true;
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.servo.set_angle",
            "控制 GPIO2 上的 MG90S 舵机转到指定角度。用户明确要求舵机转动或调整角度时调用。angle 为 0 到 180 度，90 度是中间位置。",
            PropertyList({Property("angle", kPropertyTypeInteger, 0, 180)}),
            [this](const PropertyList& properties) -> ToolResult {
                const int angle = properties["angle"].value<int>();
                const esp_err_t err = SetAngle(angle);
                if (err != ESP_OK) {
                    return std::unexpected(
                        std::string("MG90S 舵机控制失败：") + esp_err_to_name(err));
                }
                return ReturnValue(true);
            });

        mcp_server.AddTool(
            "self.servo.center",
            "将 GPIO2 上的 MG90S 舵机回到 90 度中间位置。",
            PropertyList(),
            [this](const PropertyList&) -> ToolResult {
                const esp_err_t err = SetAngle(90);
                if (err != ESP_OK) {
                    return std::unexpected(
                        std::string("MG90S 舵机回中失败：") + esp_err_to_name(err));
                }
                return ReturnValue(true);
            });

        mcp_server.AddTool(
            "self.servo.get_angle",
            "查询 MG90S 舵机当前设定的目标角度。",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return std::string("{\"angle\":") +
                       std::to_string(GetAngle()) + "}";
            });
    }
};

#endif  // MG90S_SERVO_CONTROLLER_H
