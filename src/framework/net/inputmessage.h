#ifndef INPUTMESSAGE_H
#define INPUTMESSAGE_H

#include "netdeclarations.h"

class InputMessage
{
public:
    enum {
        BUFFER_MAXSIZE = 4096,
        HEADER_POS = 0,
        HEADER_LENGTH = 2,
        CHECKSUM_POS = 2,
        CHECKSUM_LENGTH = 4,
        DATA_POS = 6,
        UNENCRYPTED_DATA_POS = 8
    };

    InputMessage();

    void reset();

    uint8 getU8();
    uint16 getU16();
    uint32 getU32();
    uint64 getU64();
    std::string getString();

    uint8* getBuffer() { return m_buffer; }
    uint16 getMessageSize() { return m_messageSize; }
    void setMessageSize(uint16 messageSize) { m_messageSize = messageSize; }
    bool end() { return m_readPos == m_messageSize; }

private:
    bool canRead(int bytes);

    uint16 m_readPos;
    uint16 m_messageSize;
    uint8 m_buffer[BUFFER_MAXSIZE];
};

#endif
