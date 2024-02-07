#include "cats/interleaver.h"
#include "cats/whitener.h"
#include "cats/packet.h"
#include "cats/ldpc.h"
#include "cats/error.h"
#include "cats/util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int cats_packet_prepare(cats_packet_t** pkt)
{
	*pkt = malloc(sizeof(cats_packet_t));
	if((*pkt) == NULL)
		throw(MALLOC_FAIL);
	
	(*pkt)->len = 0;
	(*pkt)->numWhiskers = 0;
	(*pkt)->whiskers = NULL;	
	return CATS_SUCCESS;
}

uint16_t cats_packet_build(cats_packet_t* pkt, uint8_t** out)
{
	if(*out == NULL)
		if((*out = malloc(pkt->len)) == NULL)
			throw(MALLOC_FAIL);
	
	// TODO: Validate the packet before encoding (e.g. ensure only one ID whisker exists, packet isn't too long, etc)
	return cats_packet_encode(pkt->whiskers, pkt->numWhiskers, out);
}

int cats_packet_from_buf(cats_packet_t* pkt, uint8_t* buf, uint16_t bufLen)
{
	// Maybe this is where we should remove the length in the first two bytes?
	pkt->numWhiskers = cats_packet_decode(buf, bufLen, &pkt->whiskers);
	if(pkt->numWhiskers <= 0)
		throw(DECODE_FAIL);
	return CATS_SUCCESS;
}

int cats_packet_add_identification(cats_packet_t* pkt, char* callsign, uint8_t ssid, uint16_t icon)
{
	if(callsign == NULL || strlen(callsign) <= 0 || strlen(callsign) > 252)
		throw(INVALID_OR_NO_CALLSIGN);
	if(pkt->len+5+strlen(callsign) > CATS_MAX_PKT_LEN)
		throw(PACKET_TOO_BIG);

	cats_ident_whisker_t identification;
	identification.ssid = ssid;
	identification.icon = icon;
	memcpy(identification.callsign, callsign, strlen(callsign));

	return cats_packet_add_whisker_data(pkt, WHISKER_TYPE_IDENTIFICATION, (cats_whisker_data_t*)&identification, 3+strlen(callsign));
}

int cats_packet_add_comment(cats_packet_t* pkt, char* comment)
{
	if(comment == NULL || strlen(comment) <= 0)
		throw(INVALID_OR_NO_COMMENT);
	if(strlen(comment) > 255)
		return -1; // TODO: Handle longer comments
	if(pkt->len+2+strlen(comment) > CATS_MAX_PKT_LEN)
		throw(PACKET_TOO_BIG);

	cats_whisker_t whisker;
	whisker.type = WHISKER_TYPE_COMMENT;
	whisker.len = strlen(comment);
	strcpy(whisker.data.raw, comment);

	return cats_packet_add_whisker(pkt, &whisker);
}

int cats_packet_add_gps(cats_packet_t* pkt, double lat, double lon, float alt, uint8_t error, uint8_t heading, float speed)
{
	if(pkt->len+2+cats_whisker_base_len(WHISKER_TYPE_GPS) > CATS_MAX_PKT_LEN)
		throw(PACKET_TOO_BIG);

	cats_gps_whisker_t gps;
	gps.altitude = float32_to_float16(alt);
	gps.heading = heading;
	gps.speed = float32_to_float16(speed);
	gps.maxError = error;
	gps.latitude = lat_to_int32(lat);
	gps.longitude = lon_to_int32(lon);

	return cats_packet_add_whisker_data(pkt, WHISKER_TYPE_GPS, (cats_whisker_data_t*)&gps, cats_whisker_base_len(WHISKER_TYPE_GPS));
}

int cats_packet_add_route()
{
	// TODO
}

int cats_packet_add_destination(uint8_t* callsign, uint8_t ssid, uint8_t ack)
{

}

int cats_packet_add_simplex(uint32_t frequency, cats_modulation_t modulation, uint8_t power)
{

}

int cats_packet_add_repeater(uint32_t up, uint32_t down, cats_modulation_t modulation, uint32_t tone, uint8_t power, uint16_t lat, uint16_t lon, uint8_t* name)
{

}

int cats_packet_add_node_info()
{
	// TODO
}

int cats_packet_add_whisker_data(cats_packet_t* pkt, cats_whisker_type_t type, cats_whisker_data_t* whiskerData, int len)
{
	cats_whisker_t whisker;
	whisker.type = type;
	whisker.len = len;
	whisker.data = *whiskerData;

	return cats_packet_add_whisker(pkt, &whisker);
}

int cats_packet_add_whisker(cats_packet_t* pkt, cats_whisker_t* whisker)
{
	pkt->len += whisker->len+2;
	pkt->numWhiskers++;
	if(pkt->whiskers == NULL) {
		pkt->whiskers = malloc(sizeof(cats_whisker_t));
		if(pkt->whiskers == NULL)
			throw(MALLOC_FAIL);
	}
	else {
		cats_whisker_t* tmp = realloc(pkt->whiskers, pkt->numWhiskers * sizeof(cats_whisker_t));
		if(tmp == NULL)
			throw(MALLOC_FAIL);
		pkt->whiskers = tmp;
	}
	memcpy((void*)pkt->whiskers+(sizeof(cats_whisker_t)*(pkt->numWhiskers-1)), whisker, sizeof(cats_whisker_t));

	return CATS_SUCCESS;
}

cats_whisker_t** cats_packet_find_whiskers(cats_packet_t* pkt, cats_whisker_type_t type)
{
	// Count instances of whisker type
	int found = 0;
	for(int i = 0; i < pkt->numWhiskers; i++)
		if(pkt->whiskers[i].type == type) 
			found++;
	if(found <= 0)
		throw_msg(WHISKER_NOT_FOUND, "cats_packet_find_whiskers: no whiskers of specified type found!");

	// Add matched whiskers to array
	cats_whisker_t** out = malloc(sizeof(cats_whisker_t*)*found);
	if(out == NULL)
		throw(MALLOC_FAIL);
	int idx = 0;
	for(int i = 0; i < pkt->numWhiskers; i++)
		if(pkt->whiskers[i].type == type)
			out[idx++] = &pkt->whiskers[i];

	return out;
}

int cats_packet_get_identification(cats_packet_t* pkt, char* callsign, uint8_t* ssid, uint16_t* icon)
{
	cats_whisker_t** r = cats_packet_find_whiskers(pkt, WHISKER_TYPE_IDENTIFICATION);
	if(r <= CATS_FAIL)
		throw_msg(WHISKER_NOT_FOUND, "cats_packet_get_identification: packet has no identification whisker!");
	cats_whisker_t* whisker = *r;
	free(r);
	
	cats_whisker_data_t* data = &whisker->data;
	strcpy(callsign, data->identification.callsign);
	memcpy(ssid, &data->identification.ssid, sizeof(uint8_t));
	memcpy(icon, &data->identification.icon, sizeof(uint16_t));

	return CATS_SUCCESS;
}

int cats_packet_get_comment(cats_packet_t* pkt, char* comment)
{
	cats_whisker_t** r = cats_packet_find_whiskers(pkt, WHISKER_TYPE_COMMENT);
	if(r <= CATS_FAIL)
		throw_msg(WHISKER_NOT_FOUND, "cats_packet_get_comment: packet has no comment whiskers!");
	cats_whisker_t* whisker = *r; // TODO: Handle comments across multiple whiskers
	free(r);

	if(whisker->len <= 0)
		throw_msg(INVALID_OR_NO_COMMENT, "cats_packet_get_comment: comment whisker length is <= 0!");
	strcpy(comment, whisker->data.raw);

	return CATS_SUCCESS;
}

int cats_packet_encode(cats_whisker_t* whiskers, int whiskerCount, uint8_t** dataOut)
{
	int len = 0;
	for(int i = 0; i < whiskerCount; i++)
		len += whiskers[i].len+2;
	uint8_t* out = malloc(len+2);
	if(out == NULL)
		throw(MALLOC_FAIL);

	int written = 0;
	for(int i = 0; i < whiskerCount; i++) {
		int whiskerLen = whiskers[i].len+2;
		uint8_t whisker[whiskerLen];
		cats_whisker_encode(&(whiskers[i]), whisker);
		memcpy(out+written, whisker, whiskerLen);
		written += whiskerLen;
	}

	uint16_t crc = cats_crc16(out, len);
	memcpy(out+len, &crc, sizeof(uint16_t));
	len += 2;

	cats_whiten(out, len);
	len = cats_ldpc_encode(&out, len);
	cats_interleave(out, len);
	
	uint8_t* tmp = realloc(*dataOut, len);
	if(tmp == NULL)
		throw(MALLOC_FAIL);
	*dataOut = tmp;
	memcpy(*dataOut, out, len);
	free(out);
	
	return len;
}

int cats_packet_decode(uint8_t* data, int len, cats_whisker_t** whiskersOut)
{	
	int pktLen = len;
	uint8_t* pkt = malloc(pktLen);
	memcpy(pkt, data, pktLen);

	cats_deinterleave(pkt, pktLen);
	pktLen = cats_ldpc_decode(&pkt, pktLen);
	if(pktLen < 0)
		throw(LDPC_DECODE_FAIL);
	cats_whiten(pkt, pktLen);

	uint16_t crcActual = cats_crc16(pkt, pktLen-2);
	uint16_t crcExpect = *(uint16_t*)&pkt[pktLen-2]; // Is this okay practice? Probably should use memcpy...
	if(crcActual != crcExpect)
		throw(INVALID_CRC);

	int numWhiskers = 0;
	for(int i = 0; i < pktLen-2; i+=2) {
		uint8_t len = pkt[i+1];
		numWhiskers++;
		i += len;
	}

	if(whiskersOut != NULL) {
		cats_whisker_t* whiskers = malloc(numWhiskers * sizeof(cats_whisker_t));
		int idx = 0;
		for(int i = 0; i < numWhiskers; i++) {
			cats_whisker_decode(&whiskers[i], &pkt[idx]);
			idx += whiskers[i].len+2;
		}
		*whiskersOut = whiskers;
	}

	free(pkt);
	return numWhiskers;
}

// https://stackoverflow.com/a/75139158
uint16_t cats_crc16(uint8_t* data, int len)
{
	uint16_t crc = 0xFFFF;
	for(int i = 0; i < len; i++) {
		crc ^= data[i];
		for(uint8_t j = 0; j < 8; j++)
			crc = (crc & 1) != 0 ? (crc >> 1) ^ 0x8408 : crc >> 1;
	}
	return ~crc;
}