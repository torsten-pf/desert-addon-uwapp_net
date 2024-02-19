//
// Copyright (c) 2017 Regents of the SIGNET lab, University of Padova.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Padova (SIGNET lab) nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

/**
 * @file   tcp_listener.h
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides the TcpListener class
 *
 */

#ifndef _TCP_LISTENER_H_
#define _TCP_LISTENER_H_

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define SOCKET_TYPE int
#define SET_SOCKOPT_TYPE void *
#define SENDTO_TYPE const char *
#define SOCKET_ERROR -1

#include <mutex>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <uwip-module.h>
#include <uwudp-module.h>

#include "stoppable_thread.h"
#include "logging.h"
#include "string_utils.h"

#define MAGIC 0x20041969

/** TCP listener thread with UDP socket
 *
 */
template<typename Owner>
class TcpListener : public StoppableThread
{
public:
    TcpListener(Owner *owner, uint16_t port, timeval read_timeout) : m_ReadTimeout(read_timeout)
    {
        p_Owner = owner;
        m_Port = port;
    }
    virtual ~TcpListener()
    {
        if (servSockDescr)
            ::close(servSockDescr);
    }
    bool Setup()
    {
        if (p_Owner->debugLevel() > 0) LOG_MSG_INFO("Starting data listener on port " << m_Port);
        // set up socket....
        servSockDescr = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (servSockDescr < 0) {
            LOG_MSG_ERROR(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId()  << "::UWAPPNET::TCPLISTENER::SOCKET_ERROR " << strerror(errno));
            return false;
        }

        // allow to reuse the address
        int reuse = 1;
        if (setsockopt(servSockDescr, SOL_SOCKET, SO_REUSEADDR, (SET_SOCKOPT_TYPE)&reuse, sizeof(reuse)) == -1) {
            LOG_MSG_ERROR(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId()  << "::UWAPPNET::TCPLISTENER::SOCKET_ERROR " << strerror(errno));
            return false;
        }

        if (p_Owner->debugLevel() >= 2)
            LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() << "::UWAPPNET::TCPLISTENER::SOCKET_CREATED");
        // Fill the members of sockaddr_in structure
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(m_Port);
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (p_Owner->debugLevel() >= 1)
            LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() << "::UWAPPNET::TCPLISTENER::BINDING_SOCKET_" << m_Port);
        // Bind to the local address
        if (::bind(servSockDescr, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
            LOG_MSG_ERROR(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() << "::UWAPPNET::OPEN_CONNECTION_TCP::BINDING_FAILED " << strerror(errno));
            return false;
        }

        // Listen for incoming connections
        if (listen(servSockDescr, 1)) {
            LOG_MSG_ERROR(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() << "::UWAPPNET::OPEN_CONNECTION_TCP::LISTEN_FAILED " << strerror(errno));
            return false;
        }
        if (p_Owner->debugLevel() >= 2)
            LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() << "::UWAPPNET::OPEN_CONNECTION_TCP::SERVER_READY");
        m_SocketSetUp  = true;
        return true;
    }
    /** Thread function, runs until thread is stopped */
    virtual void Run() override
    {
        try
        {
            if (!m_SocketSetUp)
                return;
            while (!StopRequested())
            {
                socklen_t clnLen = sizeof(sockaddr_in);
                if ((clnSockDescr = accept(servSockDescr, (struct sockaddr *) &(clnAddr), (socklen_t *) &clnLen)) < 0) {
                    LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                                << "::UWAPPNET::READ_PROCESS_TCP::CONNECTION_NOT_ACCEPTED " << strerror(errno));
                    break;
                }
                if (p_Owner->debugLevel() >= 1)
                    LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                            << "::UWAPPNET::READ_PROCESS_TCP::NEW_CLIENT_IP "
                            << inet_ntoa(clnAddr.sin_addr));
                HandleTCPclient();
            }
            if (p_Owner->debugLevel() > 0) LOG_MSG_INFO("Stopping data listener on port " << m_Port);
            ::close(servSockDescr);
            servSockDescr = 0;
        }
        catch (const std::exception &e)
        {
            LOG_MSG_ERROR("Caught exception in listening thread: " << e.what() << " - " << std::strerror(errno));
        }
    }
    bool send_data(const char* data, size_t size)
    {
        if (clnSockDescr) {
            if (p_Owner->debugLevel() > 0)  
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                             << "::UWAPPNET::send_data::SENDING_" << size << "_BYTES_TO_PEER");
			// prepend header (magic + payload size) - little endian
			uint32_t val32 = MAGIC;
			if (write(clnSockDescr, &val32, sizeof(val32)) != (ssize_t) sizeof(val32)) return false;
            val32 = size;
			if (write(clnSockDescr, &val32, sizeof(val32)) != (ssize_t) sizeof(val32)) return false;
			if (write(clnSockDescr, data, size) != (ssize_t) size) return false;
            return true;
		}
        return false;
    }
    void init_Packet_TCP()
    {
        if (!queuePckRead.empty()) {
            Packet *ptmp = nullptr;
            {
                // lock queue for packet removal
                std::unique_lock<std::mutex> lock(m_Mutex);
                ptmp = queuePckRead.front();
                queuePckRead.pop();
            }
            hdr_cmn *ch = HDR_CMN(ptmp);
            hdr_uwudp *uwudph = hdr_uwudp::access(ptmp);
            hdr_uwip *uwiph = hdr_uwip::access(ptmp);
            hdr_DATA_APPLICATION *uwApph = HDR_DATA_APPLICATION(ptmp);
            
            // Common header fields
            ch->uid_ = p_Owner->incrUidCnt();
            ch->ptype_ = PT_DATA_APPLICATION;
            ch->direction_ = hdr_cmn::DOWN;
            ch->timestamp() = Scheduler::instance().clock();

            // Transport header fields
            uwudph->dport() = p_Owner->getDstPort(); //port_num;

            // IP header fields
            uwiph->daddr() = p_Owner->getDstAddr(); //dst_addr;

            // uwApplication packet header fields
            uwApph->sn_ = p_Owner->incrPktSent(); // Sequence number to the data packet
            auto rftt = p_Owner->getRftt();
            if (rftt >= 0) {
                uwApph->rftt_ = (int) (rftt * 10000); // Forward Trip Time
                uwApph->rftt_valid_ = true;
            } else {
                uwApph->rftt_valid_ = false;
            }
            uwApph->priority_ = 0; // Priority of the message
            if (p_Owner->debugLevel() >= 2)
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::INIT_PACKET_TCP::UID_" << ch->uid_);
            if (p_Owner->debugLevel() >= 0)
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::INIT_PACKET_TCP::DEST_"
                        << (int) uwiph->daddr());
            if (p_Owner->debugLevel() >= 0)
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::INIT_PACKET_TCP::SIZE_"
                        << (int) uwApph->payload_size());
            if (p_Owner->debugLevel() >= 0)
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::INIT_PACKET_TCP::SN_"
                        << (int) uwApph->sn_);
            if (p_Owner->debugLevel() >= 0)
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::INIT_PACKET_TCP::INIT_PACKET_TCP::SEND_DOWN_PACKET");
            p_Owner->sendDown(ptmp);
        }
    }
protected:
    /** Message buffer */
    char buffer_msg[MAX_LENGTH_PAYLOAD];
    /** Receive a packet from ther client 
     * @return number of bytes or error (as returned by read())
     */
    int ReceivePacket() {
        int recvMsgSize = 0;
        // Extended: read header (magic + payload size) - little endian 
        uint32_t magic;
        if ((recvMsgSize = read(clnSockDescr, &magic, sizeof(magic))) < 0) {
            LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::HandleTCPclient::READ_ERROR " << strerror(errno));
            return recvMsgSize;
        }
        if (recvMsgSize == 0) 
            return recvMsgSize;
        if (magic != MAGIC) {
            if (p_Owner->debugLevel() >= 0) {
                LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                            << "::UWAPPNET::HandleTCPclient::WRONG_MAGIC 0x" << std::hex << magic << " (wanted 0x" << MAGIC << ")");
            }
            return 0;
        }
        uint32_t size;
        if ((recvMsgSize = read(clnSockDescr, &size, sizeof(size))) < 0) {
            LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::HandleTCPclient::READ_ERROR " << strerror(errno));
            return recvMsgSize;
        }
        if (recvMsgSize == 0) 
            return recvMsgSize;
        if (size > sizeof(buffer_msg)) {
            LOG_MSG_ERROR(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::HandleTCPclient::BUFFER_TOO_SMALL (need " << size << ", have " << sizeof(buffer_msg) << ")");
            return 0;
        }
        if ((recvMsgSize = read(clnSockDescr, buffer_msg, size)) < 0) {
            LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::HandleTCPclient::READ_ERROR " << strerror(errno));
        }
        return recvMsgSize;
    }
    /** Handle a TCP client connection */
    void HandleTCPclient()
    {
        int recvMsgSize = 0;
        while (!StopRequested()) {
            // Is something to read? If not, skip code below
            if (!ReadyToRead(clnSockDescr))
                continue;

            recvMsgSize = ReceivePacket();
            #if 0
            // Todo: read header (start char + payload size) - little endian preferred
            uint32_t magic;
            if ((recvMsgSize = read(clnSockDescr, &magic, sizeof(magic))) < 0) {
                LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                            << "::UWAPPNET::HandleTCPclient::READ_ERROR " << strerror(errno));
                break;
            }
            if (recvMsgSize == 0) { // client disconnected
                LOG_MSG_WARN("Node " <<  p_Owner->getNodeId()  << " - client disconnected");
                shutdown(clnSockDescr, 2);
                break;
            }
                
            if (magic != MAGIC) {
                if (p_Owner->debugLevel() >= 0) {
                    LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                             << "::UWAPPNET::HandleTCPclient::WRONG_MAGIC 0x" << std::hex << magic << " (wanted 0x" << MAGIC << ")");
                }
                continue;
            }
            uint32_t size;
            if ((recvMsgSize = read(clnSockDescr, &size, sizeof(size))) < 0) {
                LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                            << "::UWAPPNET::HandleTCPclient::READ_ERROR " << strerror(errno));
                break;
            }
            if (recvMsgSize == 0) { // client disconnected
                LOG_MSG_WARN("Node " <<  p_Owner->getNodeId()  << " - client disconnected");
                shutdown(clnSockDescr, 2);
                break;
            }
            if (size > sizeof(buffer_msg)) {
                LOG_MSG_ERROR(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                            << "::UWAPPNET::HandleTCPclient::BUFFER_TOO_SMALL (need " << size << ", have " << sizeof(buffer_msg) << ")");
                continue;
            }
            if ((recvMsgSize = read(clnSockDescr, buffer_msg, size)) < 0) {
                LOG_MSG_WARN(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                            << "::UWAPPNET::HandleTCPclient::READ_ERROR " << strerror(errno));
                break;
            }
            #endif
            if (recvMsgSize == 0) { // client disconnected
                LOG_MSG_WARN("Node " <<  p_Owner->getNodeId()  << " - client disconnected");
                shutdown(clnSockDescr, 2);
                break;
            } 
            if (p_Owner->debugLevel() >= 1) {
                LOG_MSG_INFO(p_Owner->getEpoch() << "::" << NOW << "::" << p_Owner->getNodeId() 
                        << "::UWAPPNET::READ_PROCESS_TCP::PAYLOAD_MESSAGE 0x" << buffer_to_hexstring(buffer_msg, recvMsgSize));
            }
            // create packet to be handled by desert
            std::unique_lock<std::mutex> lock(m_Mutex);
            Packet *p = Packet::alloc();
            hdr_DATA_APPLICATION *hdr_Appl = HDR_DATA_APPLICATION(p);
            memcpy(hdr_Appl->payload_msg, buffer_msg, recvMsgSize);                
            hdr_cmn *ch = HDR_CMN(p);
            ch->size() = recvMsgSize;
            hdr_Appl->payload_size() = recvMsgSize;
            queuePckRead.push(p);
            p_Owner->incrPktsPushQueue();                
            
        }
        if (p_Owner->debugLevel() >= 0) 
            LOG_MSG_INFO("Node " <<  p_Owner->getNodeId()  << " - closing client connection");
        // close socket
        ::close(clnSockDescr);
        clnSockDescr = 0;
    }


    /** Uses select() to do a timed wait for new data
     *  @return true if data ara available, false if not
     */
    bool ReadyToRead(int sock)
    {
        int nfds;
        fd_set fdset;

        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);

        nfds = (int)sock;
        int ret = select(nfds + 1, &fdset, NULL, NULL, &m_ReadTimeout);
        if (ret == SOCKET_ERROR)
        {
            LOG_MSG_ERROR("Error on select: " << ret);
        }
        return ret == 1;
    }
    /** Timeout for the select call in ReadyToRead()*/
    timeval m_ReadTimeout;
    /** UDP port number to read position from */
    uint16_t m_Port;
    /** Owner instance */
    Owner *p_Owner;

    bool m_SocketSetUp{false};
    std::mutex m_Mutex;

    struct sockaddr_in clnAddr; /**< Client address */
    struct sockaddr_in servAddr; /**< Server address */
    SOCKET_TYPE servSockDescr{0}; /**< socket descriptor for server */
    SOCKET_TYPE clnSockDescr{0}; /**< *socket descriptor for client */

    /** Queue that stores the DATA packets received from the client */
    std::queue<Packet *> queuePckRead; 
};

#endif // _TCP_LISTENER_H_
