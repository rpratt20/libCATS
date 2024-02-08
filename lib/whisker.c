#include "cats/whisker.h"
#include "cats/error.h"
#include "cats/util.h"

#include <string.h>
#include <stdint.h>

uint8_t baseLengths[] = {
    3,  // IDENTIFICATION
    5,  // TIMESTAMP
    14, // GPS
    0,  // COMMENT
    1,  // ROUTE
    2,  // DESTINATION
    0,  // ARBITRARY
    6,  // SIMPLEX
    17, // REPEATER
    3,  // NODEINFO
};

int cats_whisker_encode(cats_whisker_t* whisker, uint8_t* dataOut)
{
    uint8_t out[whisker->len+2];
    out[0] = whisker->type;
    out[1] = whisker->len;

    cats_whisker_data_t* data = &whisker->data;
    switch(whisker->type) {
        case WHISKER_TYPE_IDENTIFICATION:
            memcpy(&out[2], &data->identification.icon, sizeof(uint16_t));
            memcpy(&out[4], data->identification.callsign, whisker->len-3);
            memcpy(&out[whisker->len+1], &data->identification.ssid, sizeof(uint8_t));
        break;
        
        case WHISKER_TYPE_COMMENT:
        case WHISKER_TYPE_TIMESTAMP:
        case WHISKER_TYPE_ARBITRARY:
            memcpy(out+2, data->raw, whisker->len);
        break;

        case WHISKER_TYPE_GPS:
            int32_t lat = lat_to_int32(data->gps.latitude);
            int32_t lon = lon_to_int32(data->gps.longitude);
            uint16_t alt = float32_to_float16(data->gps.altitude);
            uint16_t vel = float32_to_float16(data->gps.speed);

            memcpy(&out[2], &lat, sizeof(int32_t));
            memcpy(&out[6], &lon, sizeof(int32_t));
            memcpy(&out[10], &alt, sizeof(uint16_t));
            memcpy(&out[12], &data->gps.maxError, sizeof(uint8_t));
            memcpy(&out[13], &data->gps.heading, sizeof(uint8_t));
            memcpy(&out[14], &vel, sizeof(uint16_t));
        break;

        case WHISKER_TYPE_ROUTE:
            memcpy(&out[2], &data->route.maxDigipeats, sizeof(uint8_t));
            memcpy(&out[3], data->route.routeData, whisker->len-1);
        break;

        case WHISKER_TYPE_DESTINATION:
            memcpy(&out[2], &data->destination.ack, sizeof(uint8_t));
            memcpy(&out[3], data->destination.callsign, whisker->len-2);
            memcpy(&out[whisker->len+1], &data->destination.ssid, sizeof(uint8_t));
        break;

        case WHISKER_TYPE_SIMPLEX:
            memcpy(&out[2], &data->simplex.frequency, sizeof(uint32_t));
            memcpy(&out[6], &data->simplex.modulation, sizeof(uint8_t));
            memcpy(&out[7], &data->simplex.power, sizeof(uint8_t));
        break;

        case WHISKER_TYPE_REPEATER:
            memcpy(&out[2], &data->repeater.uplink, sizeof(uint32_t));
            memcpy(&out[6], &data->repeater.downlink, sizeof(uint32_t));
            memcpy(&out[10], &data->repeater.modulation, sizeof(uint8_t));
            memcpy(&out[11], &data->repeater.tone, 3);
            memcpy(&out[14], &data->repeater.power, sizeof(uint8_t));
            memcpy(&out[15], &data->repeater.latitude, sizeof(int16_t));
            memcpy(&out[17], &data->repeater.longitude, sizeof(int16_t));
            memcpy(&out[19], data->repeater.name, whisker->len-17);
        break;

        // Unsupported type
        default:
            throw(UNSUPPORTED_WHISKER);
    }

    memcpy(dataOut, out, whisker->len+2);
    return CATS_SUCCESS;
}

int cats_whisker_decode(cats_whisker_t* whiskerOut, uint8_t* data)
{
    cats_whisker_t out;
    out.type = data[0];
	out.len = data[1];

    cats_whisker_data_t* whiskerData = &out.data;
    memset(whiskerData, 0x00, sizeof(cats_whisker_data_t));
    switch(out.type) {
        case WHISKER_TYPE_IDENTIFICATION:
            memcpy(whiskerData->identification.callsign, &data[4], out.len-3);
            memcpy(&whiskerData->identification.icon, &data[2], sizeof(uint16_t));
            memcpy(&whiskerData->identification.ssid, &data[out.len+1], sizeof(uint8_t));
        break;
        
        case WHISKER_TYPE_COMMENT:
        case WHISKER_TYPE_TIMESTAMP:
        case WHISKER_TYPE_ARBITRARY:
            memcpy(whiskerData->raw, &data[2], out.len);
        break;

        case WHISKER_TYPE_GPS:
            int32_t lat, lon;
            uint16_t alt, vel;

            memcpy(&lat, &data[2], sizeof(int32_t));
            memcpy(&lon, &data[6], sizeof(int32_t));
            memcpy(&alt, &data[10], sizeof(uint16_t));
            memcpy(&whiskerData->gps.maxError, &data[12], sizeof(uint8_t));
            memcpy(&whiskerData->gps.heading, &data[13], sizeof(uint8_t));
            memcpy(&vel, &data[14], sizeof(uint16_t));

            whiskerData->gps.latitude = int32_to_lat(lat);
            whiskerData->gps.longitude = int32_to_lon(lon);
            whiskerData->gps.altitude = float16_to_float32(alt);
            whiskerData->gps.speed = float16_to_float32(vel);
        break;

        case WHISKER_TYPE_ROUTE:
            memcpy(&whiskerData->route.maxDigipeats, &data[2], sizeof(uint8_t));
            memcpy(whiskerData->route.routeData, &data[3], out.len-1);
        break;

        case WHISKER_TYPE_DESTINATION:
            memcpy(&whiskerData->destination.ack, &data[2], sizeof(uint8_t));
            memcpy(&whiskerData->destination.ssid, &data[out.len+1], sizeof(uint8_t));
            memcpy(whiskerData->destination.callsign, &data[3], out.len-2);
        break;

        case WHISKER_TYPE_SIMPLEX:
            memcpy(&whiskerData->simplex.frequency, &data[2], sizeof(uint32_t));
            memcpy(&whiskerData->simplex.modulation, &data[6], sizeof(uint8_t));
            memcpy(&whiskerData->simplex.power, &data[7], sizeof(uint8_t));
        break;

        case WHISKER_TYPE_REPEATER:
            memcpy(&whiskerData->repeater.uplink, &data[2], sizeof(uint32_t));
            memcpy(&whiskerData->repeater.downlink, &data[6], sizeof(uint32_t));
            memcpy(&whiskerData->repeater.modulation, &data[10], sizeof(uint8_t));
            memcpy(&whiskerData->repeater.tone, &data[11], 3);
            memcpy(&whiskerData->repeater.power, &data[14], sizeof(uint8_t));
            memcpy(&whiskerData->repeater.latitude, &data[15], sizeof(int16_t));
            memcpy(&whiskerData->repeater.longitude, &data[17], sizeof(int16_t));
            memcpy(whiskerData->repeater.name, &data[19], out.len-17);
        break;

        // Unsupported type
        default:
            throw(UNSUPPORTED_WHISKER);
    }

    memcpy(whiskerOut, &out, sizeof(cats_whisker_t));
    return CATS_SUCCESS;
}

int cats_whisker_base_len(cats_whisker_type_t type)
{
    if(type < 0 || type > 9)
        throw(UNSUPPORTED_WHISKER);

    return baseLengths[type];
}