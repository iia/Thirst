#ifndef SYSTEM_H
#define SYSTEM_H

// System partition table (required since SDK 3.0.0).
#define SYS_PARTITION_BOOTLOADER_SIZE       0x001000
#define SYS_PARTITION_BOOTLOADER_ADDR       0x000000
#define SYS_PARTITION_OTA_FW_SIZE           0x100000
#define SYS_PARTITION_OTA_FW_1_ADDR         (SYS_PARTITION_BOOTLOADER_ADDR + SYS_PARTITION_BOOTLOADER_SIZE)
#define SYS_PARTITION_OTA_FW_2_ADDR         (SYS_PARTITION_OTA_FW_1_ADDR + SYS_PARTITION_OTA_FW_SIZE)
#define SYS_PARTITION_CONFIG_SIZE           0x001000
#define SYS_PARTITION_CONFIG_ADDR           0x3FA000
#define SYS_PARTITION_RF_CAL_SIZE           0x001000
#define SYS_PARTITION_RF_CAL_ADDR           (SYS_PARTITION_CONFIG_ADDR + SYS_PARTITION_CONFIG_SIZE)
#define SYS_PARTITION_PHY_DATA_SIZE         0x001000
#define SYS_PARTITION_PHY_DATA_ADDR         (SYS_PARTITION_RF_CAL_ADDR + SYS_PARTITION_RF_CAL_SIZE)
#define SYS_PARTITION_SYSTEM_PARAMETER_SIZE 0x003000
#define SYS_PARTITION_SYSTEM_PARAMETER_ADDR (SYS_PARTITION_PHY_DATA_ADDR + SYS_PARTITION_PHY_DATA_SIZE)

// State machine states.
#define SYS_STATE_INIT                    1
#define SYS_STATE_CONFIG                  2
#define SYS_STATE_INIT_OK                 3
#define SYS_STATE_SEND_DATA               4
#define SYS_STATE_DEEP_SLEEP              5
#define SYS_STATE_PRE_SEND_DATA           6
#define SYS_STATE_SEND_DATA_DONE          7
#define SYS_STATE_DEEP_SLEEP_CYCLE        8
#define SYS_STATE_RESET_AFTER_CONFIG_SAVE 9
#define SYS_STATE_ERROR                   (-1)
#define SYS_STATE_ERROR_INIT_MAIN_TASK    (-2)

// Main task.
#define SYS_TASK_QUEUE_MAIN_LEN                 1
#define SYS_TASK_SIGNAL_CONFIG_MODE             1
#define SYS_TASK_SIGNAL_DEEP_SLEEP_CYCLE        2
#define SYS_TASK_SIGNAL_DEEP_SLEEP              3
#define SYS_TASK_SIGNAL_STATE_ERROR             4
#define SYS_TASK_SIGNAL_SEND_DATA               5
#define SYS_TASK_SIGNAL_SEND_DATA_DONE          6
#define SYS_TASK_SIGNAL_RESET_AFTER_CONFIG_SAVE 7

// Deep sleep.
#define SYS_DEEP_SLEEP_OPTION_SAME_AS_PWRUP            1
#define SYS_DEEP_SLEEP_OPTION_NO_RF_CAL                2
#define SYS_DEEP_SLEEP_OPTION_NO_RADIO                 4
#define SYS_DEEP_SLEEP_TIMES                          12
#define SYS_DEEP_SLEEP_DURATION_US          7200000000UL // Two Hours in uS.

typedef struct {
	int state_next;
	int state_current;
} SYS_STATE_t;

typedef struct {
	int sleep_type;
	uint64_t sleep_duration;
} SYS_DEEP_SLEEP_PARAMS_t;

typedef struct {
	uint32_t sleep_count;
} SYS_RTC_DATA_t;

void ICACHE_FLASH_ATTR
sys_cb_init_done(void);

void ICACHE_FLASH_ATTR
sys_deep_sleep_cycle(void);

void ICACHE_FLASH_ATTR
sys_state_toggle_led_error(void);

void ICACHE_FLASH_ATTR
sys_state_transition_with_task(
	int state_current,
	int state_next,
	int sys_task_signal,
	ETSParam sys_task_param
);

void ICACHE_FLASH_ATTR
sys_state_transition_with_wifi_mode(
	int state_current,
	int state_next,
	int mode
);

void ICACHE_FLASH_ATTR
sys_cb_wifi_event(System_Event_t *evt);

void ICACHE_FLASH_ATTR
sys_task_handler_main(os_event_t *task_event);

void ICACHE_FLASH_ATTR
sys_state_transition(int state_current, int state_next);

#endif // SYSTEM_H
