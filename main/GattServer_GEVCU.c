// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bt.h"
#include "bta_api.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_main.h"
#include "GattServer_GEVCU.h"


#define GEVCU_PROFILE_NUM 			    1
#define GEVCU_PROFILE_APP_IDX 			0
#define ESP_GEVCU_APP_ID			    0x55
#define GEVCU_DEVICE_NAME              "GEVCU 6.2 ECU"
#define GEVCU_MANUFACTURER_DATA_LEN    17
#define GEVCU_SVC_INST_ID	    	0

#define GATTS_DEMO_CHAR_VAL_LEN_MAX		0x40


static uint16_t numAttributes;
uint16_t gevcu_handle_table[280];

GEVCU_PARAM_CACHE_t params;

//Attributes with a length of 0 are special "this is a service definition" lines
//Attributes end with a 0xFFFF UUID record as a terminator. It isn't actually included in resulting list.
//divide chracteristics up into three services (Motoring stuff), "BMS type stuff" "System status/config"
GATT_CHARACTERISTIC_t GEVCU_Characteristics[] = {
//    {0x3100, ESP_GATT_CHAR_PROP_BIT_READ, 0, 0, "",                         //define 0x3100 Service
//        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
//        (uint8_t *)&params.torqueRequested},
    {0x3101, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "TorqueRequested", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_MOMENT_OF_FORCE_NEWTON_METRE, 1, 0},
        (uint8_t *)&params.torqueRequested},
    {0x3102, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "TorqueActual", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_MOMENT_OF_FORCE_NEWTON_METRE, 1, 0}, 
        (uint8_t *)&params.torqueActual},
    {0x3103, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "ThrottleRaw1", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttleRawLevel1},
    {0x3104, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "ThrottleRaw2", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttleRawLevel2},
    {0x3105, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "BrakeRaw", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.brakeRawLevel},
    {0x3106, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "ThrottlePercentage", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttlePercentage},
    {0x3107, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "BrakePercentage", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.brakePercentage},
    {0x3108, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "SpeedRequested", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_ANGULAR_VELOCITY_REVOLUTION_PER_MINUTE, 1, 0}, 
        (uint8_t *)&params.speedRequested},
    {0x3109, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "SpeedActual", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_ANGULAR_VELOCITY_REVOLUTION_PER_MINUTE, 1, 0}, 
        (uint8_t *)&params.speedActual},
    {0x310A, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "PowerMode", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.powerMode},
    {0x310B, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Gear", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.gear},
    {0x310C, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "isRunning", 
        {GATT_PRESENT_FORMAT_BOOLEAN, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.isRunning},
    {0x310D, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "isFaulted", 
        {GATT_PRESENT_FORMAT_BOOLEAN, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.isFaulted},
    {0x310E, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "isWarning", 
        {GATT_PRESENT_FORMAT_BOOLEAN, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.isWarning},
    {0x310F, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "LoggingLevel", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.logLevel},
    {0x3110, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Can0 Bitrate", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_FREQUENCY_HERTZ, 1, 0}, 
        (uint8_t *)&params.can0Speed},
    {0x3111, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Can1 Bitrate", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_FREQUENCY_HERTZ, 1, 0}, 
        (uint8_t *)&params.can1Speed},
    {0x3112, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "HV Bus Voltage", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_ELECTRIC_POTENTIAL_DIFFERENCE_VOLT, 1, 0}, 
        (uint8_t *)&params.busVoltage},
    {0x3113, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "HV Bus Current", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_ELECTRIC_CURRENT_AMPERE, 1, 0}, 
        (uint8_t *)&params.busCurrent},
    {0x3114, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Motor Current", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_ELECTRIC_CURRENT_AMPERE, 1, 0}, 
        (uint8_t *)&params.motorCurrent},
    {0x3115, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Kwh Remaining", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_ENERGY_KILOWATT_HOUR, 1, 0}, 
        (uint8_t *)&params.kwHours},        
    {0x3116, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Mechanical Power", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_POWER_WATT, 1, 0}, 
        (uint8_t *)&params.mechPower},
    {0x3117, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "State of Charge", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.SOC},        
    {0x3118, ESP_GATT_CHAR_PROP_BIT_READ, 4, 4, "Status Bitfield 1", 
        {GATT_PRESENT_FORMAT_UINT32, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.bitfield1},
    {0x3119, ESP_GATT_CHAR_PROP_BIT_READ, 4, 4, "Status Bitfield 2", 
        {GATT_PRESENT_FORMAT_UINT32, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.bitfield2},
    {0x311A, ESP_GATT_CHAR_PROP_BIT_READ, 4, 4, "Dig In Bitfield", 
        {GATT_PRESENT_FORMAT_UINT32, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.digitalInputs},
    {0x311B, ESP_GATT_CHAR_PROP_BIT_READ, 4, 4, "Dig Out Bitfield", 
        {GATT_PRESENT_FORMAT_UINT32, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.digitalOutputs},
    {0x311C, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Motor Temperature", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_THERMODYNAMIC_TEMPERATURE_DEGREE_FAHRENHEIT, 1, 0}, 
        (uint8_t *)&params.motorTemperature},
    {0x311D, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Inverter Temperature", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_THERMODYNAMIC_TEMPERATURE_DEGREE_FAHRENHEIT, 1, 0}, 
        (uint8_t *)&params.inverterTemperature},
    {0x311E, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "System Temperature", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_THERMODYNAMIC_TEMPERATURE_DEGREE_FAHRENHEIT, 1, 0}, 
        (uint8_t *)&params.systemTemperature},
    {0x311F, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Precharge Time", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_TIME_SECOND, 1, 0}, 
        (uint8_t *)&params.prechargeDuration},
    {0x3120, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Precharge Output",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.prechargeRelay},
    {0x3121, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Main Contactor Output",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.mainContRelay},
    {0x3122, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Cooling Relay Output",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.coolingRelay},
    {0x3123, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Cool On Temperature",
        {GATT_PRESENT_FORMAT_SINT8, 0, GATT_PRESENT_UNIT_THERMODYNAMIC_TEMPERATURE_DEGREE_FAHRENHEIT, 1, 0}, 
        (uint8_t *)&params.coolOnTemp},   
    {0x3124, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Cool Off Temperature",
        {GATT_PRESENT_FORMAT_SINT8, 0, GATT_PRESENT_UNIT_THERMODYNAMIC_TEMPERATURE_DEGREE_FAHRENHEIT, 1, 0}, 
        (uint8_t *)&params.coolOffTemp},
    {0x3125, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Brake Light Output",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.brakeLightOut},
    {0x3126, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Reverse Light Output",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.reverseLightOut},
    {0x3127, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Enable Input",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.enableIn},
    {0x3128, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Reverse Input",
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.reverseIn},
    {0x3129, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle 1 Min", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttle1Min},
    {0x312A, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle 2 Min", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttle2Min},
    {0x312B, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle 1 Max", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttle1Max},
    {0x312C, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle 2 Max", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttle2Max},
    {0x312D, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Num Throttle Pots", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.numThrottlePots},
    {0x312E, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Throttle Type", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.throttleType},
    {0x312F, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle Regen Max", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleRegenMax},
    {0x3130, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle Regen Min", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleRegenMin},
    {0x3131, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle Fwd Start", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleFwd},
    {0x3132, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Throttle Map Point", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleMap},
    {0x3133, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Throttle Min Regen", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleLowestRegen},
    {0x3134, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Throttle Max Regen", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleHighestRegen},
    {0x3135, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Throttle Creep", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.throttleCreep},
    {0x3136, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Brake Min", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.brakeMin},
    {0x3137, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Brake Max", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.brakeMax},
    {0x3138, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Brake Min Regen", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.brakeRegenMin},
    {0x3139, ESP_GATT_CHAR_PROP_BIT_READ, 1, 1, "Brake Max Regen", 
        {GATT_PRESENT_FORMAT_UINT8, 0, GATT_PRESENT_UNIT_PERCENTAGE, 1, 0}, 
        (uint8_t *)&params.brakeRegenMax},
    {0x313A, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Nominal Voltage", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_ELECTRIC_POTENTIAL_DIFFERENCE_VOLT, 1, 0}, 
        (uint8_t *)&params.nomVoltage},
    {0x313B, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Max RPMs", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_ANGULAR_VELOCITY_REVOLUTION_PER_MINUTE, 1, 0}, 
        (uint8_t *)&params.maxRPM},
    {0x313C, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "Max Torque", 
        {GATT_PRESENT_FORMAT_UINT16, 0, GATT_PRESENT_UNIT_MOMENT_OF_FORCE_NEWTON_METRE, 1, 0}, 
        (uint8_t *)&params.maxTorque},
    {0x313D, ESP_GATT_CHAR_PROP_BIT_READ, 4, 4, "Device Enable Bits1", 
        {GATT_PRESENT_FORMAT_UINT32, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.deviceEnable1},
    {0x313E, ESP_GATT_CHAR_PROP_BIT_READ, 4, 4, "Device Enable Bits2", 
        {GATT_PRESENT_FORMAT_UINT32, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.deviceEnable2},
    {0xFFFF, ESP_GATT_CHAR_PROP_BIT_READ, 2, 2, "", 
        {GATT_PRESENT_FORMAT_SINT16, 0, GATT_PRESENT_UNIT_NONE, 1, 0}, 
        (uint8_t *)&params.torqueRequested},
};

static uint8_t gevcu_service_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
};

static esp_ble_adv_data_t gevcu_adv_config = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 32,
    .p_service_uuid = gevcu_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t gevcu_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, 
					esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gevcu_profile_tab[GEVCU_PROFILE_NUM] = {
    [GEVCU_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
    
};

/*
 *  GEVCU PROFILE ATTRIBUTES
 ****************************************************************************************
 */

/// GEVCU Service
static const uint16_t gevcu_svc = 0x3100;

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t character_descriptor = ESP_GATT_UUID_CHAR_DESCRIPTION;
static const uint16_t character_presentation = ESP_GATT_UUID_CHAR_PRESENT_FORMAT;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_READ;

/// Attribute table - everything is an attribute, services, characteristics, attributes on characteristics.
//To add attributes like descriptor and presentation to a characteristic you just add them after the characteristic
//and before the next characteristic. This array has to be at least five times the size of the characteristics array
static esp_gatts_attr_db_t gevcu_gatt_db[400];
//ESP_GATT_ATTR_HANDLE_MAX has to be this large too I think. It's found in esp32/esp-idf/components/bt/bluedroid/api/include/esp_gatt_defs.h

static void generateAttrTable()
{
    int counter = 0;
    int attrCount = 0;
    int handleCount = 0; //some attributes don't get handles

    gevcu_gatt_db[attrCount].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
    gevcu_gatt_db[attrCount].att_desc.uuid_length = ESP_UUID_LEN_16;
    gevcu_gatt_db[attrCount].att_desc.uuid_p = (uint8_t *)&primary_service_uuid;
    gevcu_gatt_db[attrCount].att_desc.perm = ESP_GATT_PERM_READ;
    gevcu_gatt_db[attrCount].att_desc.max_length = sizeof(gevcu_svc);
    gevcu_gatt_db[attrCount].att_desc.length = sizeof(gevcu_svc);
    gevcu_gatt_db[attrCount].att_desc.value = (uint8_t *)&gevcu_svc;

    while (GEVCU_Characteristics[counter].id < 0xFFFF)
    {
        //declaration (sets read, write, notify permissions)
        gevcu_gatt_db[attrCount++].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
        gevcu_gatt_db[attrCount].att_desc.uuid_length = ESP_UUID_LEN_16;
        gevcu_gatt_db[attrCount].att_desc.uuid_p = (uint8_t *)&character_declaration_uuid;
        gevcu_gatt_db[attrCount].att_desc.perm = ESP_GATT_PERM_READ;
        gevcu_gatt_db[attrCount].att_desc.max_length = CHAR_DECLARATION_SIZE;
        gevcu_gatt_db[attrCount].att_desc.length = CHAR_DECLARATION_SIZE;
        gevcu_gatt_db[attrCount].att_desc.value = (uint8_t *)&GEVCU_Characteristics[counter].properties;

        //value - sets the UUID of the characteristic too.
        gevcu_gatt_db[attrCount++].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
        gevcu_gatt_db[attrCount].att_desc.uuid_length = ESP_UUID_LEN_16;
        gevcu_gatt_db[attrCount].att_desc.uuid_p = (uint8_t *)&GEVCU_Characteristics[counter].id;
        gevcu_gatt_db[attrCount].att_desc.perm = ESP_GATT_PERM_READ;
        gevcu_gatt_db[attrCount].att_desc.max_length = GEVCU_Characteristics[counter].maxLen;
        gevcu_gatt_db[attrCount].att_desc.length = GEVCU_Characteristics[counter].maxLen;
        gevcu_gatt_db[attrCount].att_desc.value = GEVCU_Characteristics[counter].data;

        //description
        gevcu_gatt_db[attrCount++].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
        gevcu_gatt_db[attrCount].att_desc.uuid_length = ESP_UUID_LEN_16;
        gevcu_gatt_db[attrCount].att_desc.uuid_p = (uint8_t *)&character_descriptor;
        gevcu_gatt_db[attrCount].att_desc.perm = ESP_GATT_PERM_READ;
        gevcu_gatt_db[attrCount].att_desc.max_length = strlen(GEVCU_Characteristics[counter].description);
        gevcu_gatt_db[attrCount].att_desc.length = strlen(GEVCU_Characteristics[counter].description);
        gevcu_gatt_db[attrCount].att_desc.value = (uint8_t *)GEVCU_Characteristics[counter].description;
        handleCount++;

        //Presentation byte
        gevcu_gatt_db[attrCount++].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
        gevcu_gatt_db[attrCount].att_desc.uuid_length = ESP_UUID_LEN_16;
        gevcu_gatt_db[attrCount].att_desc.uuid_p = (uint8_t *)&character_presentation;
        gevcu_gatt_db[attrCount].att_desc.perm = ESP_GATT_PERM_READ;
        gevcu_gatt_db[attrCount].att_desc.max_length = 7;
        gevcu_gatt_db[attrCount].att_desc.length = 7;
        gevcu_gatt_db[attrCount].att_desc.value = (uint8_t *)&GEVCU_Characteristics[counter].presentation;
        handleCount++;

        counter++;
    }

    numAttributes = attrCount + 1;
};



static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    LOG_ERROR("GAP_EVT, event %d\n", event);

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        LOG_ERROR("Start advertising");
        esp_ble_gap_start_advertising(&gevcu_adv_params);
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, 
										   esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) 
{
    LOG_ERROR("event = %x\n",event);
    switch (event) {
    case ESP_GATTS_REG_EVT: //0
		LOG_INFO("%s %d\n", __func__, __LINE__);
        	esp_ble_gap_set_device_name(GEVCU_DEVICE_NAME);
        LOG_INFO("%s %d\n", __func__, __LINE__);
       	esp_ble_gap_config_adv_data(&gevcu_adv_config);

        LOG_INFO("%s %d\n", __func__, __LINE__);
		esp_ble_gatts_create_attr_tab(gevcu_gatt_db, gatts_if, 
								numAttributes, 0);
        LOG_INFO("%s %d\n", __func__, __LINE__);

       	break;
    case ESP_GATTS_READ_EVT: //1   
       	break;
    case ESP_GATTS_WRITE_EVT: //2
      	break;
    case ESP_GATTS_EXEC_WRITE_EVT: //3
		break;
    case ESP_GATTS_MTU_EVT: //4
		break;
   	case ESP_GATTS_CONF_EVT: //5
		break;
    case ESP_GATTS_UNREG_EVT: //6
        break;
    case ESP_GATTS_CREATE_EVT: //7
        LOG_INFO("Create service complete");
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT: //8
        break;
    case ESP_GATTS_ADD_CHAR_EVT: //9
        LOG_INFO("Add characteristic complete");
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: //10
        LOG_INFO("Add descriptor complete");
        break;
    case ESP_GATTS_DELETE_EVT: //11
        break;
    case ESP_GATTS_START_EVT: //12
        break;
    case ESP_GATTS_STOP_EVT: //13
        break;
    case ESP_GATTS_CONNECT_EVT: //14
        break;
    case ESP_GATTS_DISCONNECT_EVT: //15
        //upon disconnect re-enter advertising mode
        esp_ble_gap_start_advertising(&gevcu_adv_params);        
	    break;
    case ESP_GATTS_OPEN_EVT: //16
		break;
    case ESP_GATTS_CANCEL_OPEN_EVT: //17
		break;
    case ESP_GATTS_CLOSE_EVT: //18
		break;
    case ESP_GATTS_LISTEN_EVT: //19
		break;
    case ESP_GATTS_CONGEST_EVT: //20
		break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:{ //22
		LOG_ERROR("The number handle =%i\n",param->add_attr_tab.num_handle);
        LOG_ERROR("Number of attributes %i", numAttributes);
		if(param->add_attr_tab.num_handle == numAttributes) {			
			memcpy(gevcu_handle_table, param->add_attr_tab.handles, 
					numAttributes);
            for (int x = 0 ; x < param->add_attr_tab.num_handle; x++) LOG_ERROR("Handle %i is %i", x, gevcu_handle_table[x]);

			esp_ble_gatts_start_service(gevcu_handle_table[0]);
            LOG_ERROR("Attempted to start service with table ID %i", gevcu_handle_table[0]);
		}
		break;
	}
		
    default:
        break;
    }
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, 
									esp_ble_gatts_cb_param_t *param)
{
    LOG_INFO("EVT %d, gatts if %d\n", event, gatts_if);

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gevcu_profile_tab[GEVCU_PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            LOG_INFO("Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id, 
                    param->reg.status);
            return;
        }
    }
	
    do {
        int idx;
        for (idx = 0; idx < GEVCU_PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == gevcu_profile_tab[idx].gatts_if) {
                if (gevcu_profile_tab[idx].gatts_cb) {
                    gevcu_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void app_main()
{
    esp_err_t ret;

    generateAttrTable();

    esp_bt_controller_init();
    LOG_INFO("%s init bluetooth\n", __func__);
    ret = esp_bluedroid_init();
    if (ret) {
        LOG_ERROR("%s init bluetooth failed\n", __func__);
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        LOG_ERROR("%s enable bluetooth failed\n", __func__);
        return;
    }

    LOG_INFO("%s %d\n", __func__, __LINE__);

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(ESP_GEVCU_APP_ID);
    LOG_INFO("%s %d\n", __func__, __LINE__);
    return;
}