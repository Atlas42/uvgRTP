#include "rtcp_packets.hh"

#include "uvgrtp/debug.hh"



rtp_error_t uvgrtp::construct_rtcp_header(int& ptr, size_t packet_size,
    uint8_t* frame,
    uint16_t secondField,
    uvgrtp::frame::RTCP_FRAME_TYPE frame_type,
    uint32_t ssrc)
{
    if (packet_size > UINT16_MAX)
    {
        LOG_ERROR("RTCP receiver report packet size too large!");
        return RTP_GENERIC_ERROR;
    }

    // header |V=2|P|    SC   |  PT  |             length            |
    frame[ptr] = (2 << 6) | (0 << 5) | secondField;
    frame[ptr + 1] = frame_type;

    // The RTCP header length field is measured in 32-bit words - 1
    *(uint16_t*)&frame[ptr + 2] = htons((uint16_t)packet_size / sizeof(uint32_t) - 1);
    ptr += RTCP_HEADER_SIZE;

    if (ssrc)
    {
        *(uint32_t*)&frame[ptr] = htonl(ssrc);
        ptr += SSRC_CSRC_SIZE;
    }

    return RTP_OK;
}

size_t uvgrtp::get_app_packet_size(size_t payload_len)
{
    return RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE + payload_len;
}

rtp_error_t uvgrtp::construct_app_packet(uint8_t* frame, int& ptr,
    const char* name, const uint8_t* payload, size_t payload_len, 
    uint8_t subtype, uint32_t local_ssrc)
{
    rtp_error_t ret = RTP_OK;

    if ((ret = construct_rtcp_header(ptr, get_app_packet_size(payload_len), frame, (subtype & 0x1f),
        uvgrtp::frame::RTCP_FT_APP, local_ssrc)) != RTP_OK)
    {
        return ret;
    }

    memcpy(&frame[ptr], name, APP_NAME_SIZE);
    memcpy(&frame[ptr + APP_NAME_SIZE], payload, payload_len);
    ptr += APP_NAME_SIZE + payload_len;

    return ret;
}

size_t uvgrtp::get_sdes_packet_size(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items) {
    size_t frame_size = 0;

    /* We currently only support having one source. If uvgRTP is used in a mixer, multiple sources
     * should be supported in SDES packet. */

     // calculate SDES packet size
    frame_size = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE; // our csrc
    frame_size += items.size() * 2; /* sdes item type + length, both take one byte */
    for (auto& item : items)
    {
        if (item.length <= 255)
        {
            frame_size += item.length;
        }
        else
        {
            LOG_ERROR("SDES item text must not be longer than 255 characters");
        }
    }

    return frame_size;
}

rtp_error_t uvgrtp::construct_sdes_packet(uint8_t* frame, int& ptr,
    const std::vector<uvgrtp::frame::rtcp_sdes_item>& items) {

    for (auto& item : items)
    {
        if (item.length <= 255)
        {
            frame[ptr++] = item.type;
            frame[ptr++] = item.length;
            memcpy(frame + ptr, item.data, item.length);
            ptr += item.length;
        }
    }

    return RTP_OK;
}