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
 * @file   uwAppNet_module.h (based on uwApplication_module.cpp)
 * @author Federico Favaro
 * @version 1.1.0
 *
 * \brief Provides the definition of uwAppNetModule class
 *
 */

#ifndef UWAPPLICATION_MODULE_H
#define UWAPPLICATION_MODULE_H

#include <module.h>
#include <iostream>
#include <math.h>
#include <errno.h>
#include <time.h>

#include <chrono>
#include <queue>

#include "tcp_listener.h"

#define UWAPPLICATION_DROP_REASON_UNKNOWN_TYPE \
	"DPUT" /**< Drop the packet. Packet received is an unknown type*/
#define UWAPPLICATION_DROP_REASON_DUPLICATED_PACKET \
	"DPD" /**< Drop the packet. Packet received is already analyzed*/
#define UWAPPLICATION_DROP_REASON_OUT_OF_SEQUENCE \
	"DOOS" /**< Drop the packet. Packet received is out of sequence. */


extern packet_t
		PT_DATA_APPLICATION; /**< Trigger packet type for UFetch protocol */

class uwAppNetModule : public Module
{
public:
	/**
	 * Constructor of uwAppNetModule class
	 */
	uwAppNetModule();
	/**
	 * Destructor of uwAppNetModule class
	 */
	virtual ~uwAppNetModule();
	/**
	 * TCL command interpreter. It implements the following OTCL methods:
	 *
	 * @param argc Number of arguments in <i>argv</i>.
	 * @param argv Array of strings which are the command parameters (Note that
	 *<i>argv[0]</i> is the name of the object).
	 * @return TCL_OK or TCL_ERROR whether the command has been dispatched
	 *successfully or not.
	 *
	 **/
	virtual int command(int argc, const char *const *argv);

	
	/**
	 * Increase the number of DATA packets stored in the Server queue. This DATA
	 * packets will be sent to the below levels of ISO/OSI stack protocol.
	 */
	virtual void
	incrPktsPushQueue()
	{
		pkts_push_queue++;
	}

	/**
	 * Calculate the epoch of the event. Used in sea-trial mode
	 * @return the epoch of the system
	 */
	inline unsigned long int
	getEpoch()
	{
	  unsigned long int timestamp =
		  (unsigned long int) (std::chrono::duration_cast<std::chrono::milliseconds>(
			  std::chrono::system_clock::now().time_since_epoch()).count() );
	  return timestamp;
	}
	/** Return the debug level */
	int debugLevel() const { return debug_; }
	/** Return the forward trip time */
	inline int getRftt() const { return rftt; }
	/** Return the node ID */
	inline int getNodeId() const { return node_id; }	

	/* Make sendDown() public reachable */
	using Module::sendDown;
protected:
	/**< uwSenderTimer class that manage the timer */
	class uwSendTimerAppl : public TimerHandler
	{
	public:
		uwSendTimerAppl(uwAppNetModule *m)
			: TimerHandler()
		{
			m_ = m;
		}

		virtual ~uwSendTimerAppl()
		{
		}

	protected:
		virtual void expire(Event *e);
		uwAppNetModule *m_;
	}; // End uwSendTimer class


	/**************************************************************************
	 *                          METHODS                                       *
	 **************************************************************************/
	/**
	 * Handle the transmission of DATA packets between CBR layer and the below
	 * level
	 */
	virtual void recv(Packet *);
	/**
	 * Comupte some statistics as the number of packets sent and receive between
	 * two layer, or control if the packet received is out of sequence.
	 */
	virtual void statistics(Packet *p);
	/**
	 * Close the socket connection in the case the communication take place with
	 * socket, otherwise stop the execution of the process, so force the
	 * cancellation
	 * of period time generation.
	 */
	virtual void stop();
	/**
	 * If the communication take place without sockets verify if the data
	 *packets
	 * received by the server is out of order or not. In the first case discard
	 *the
	 * data packet
	 *
	 * @return <i>true</i> enable drop out of order
	 *          <i>false</i> not enabled drop out of order
	 */
	virtual inline bool
	useDropOutOfOrder()
	{
		bool test;
		drop_out_of_order == 1 ? test = true : test = false;
		return test;
	}

	/**************************************************************************
	 *                       METHODS GET and SET                              *
	 **************************************************************************/
public:
	/**
	 * Increment the sequence number and so the number of packets sent by the
	 * server, return the new number
	 */
	inline int incrPktSent() {
		return ++txsn;
	}
	/** Increment identifier counter that identify uniquely the DATA packet, return the new number */
	inline int incrUidCnt() {
		return ++uidcnt;
	}
	inline uint8_t getDstAddr() const {
		return dst_addr;
	}
	inline int getDstPort() const {
		return port_num;
	}
protected:
	/**
	 * Increase the number of DATA packets lost by the server
	 */
	virtual inline void
	incrPktLost(const int &npkts)
	{
		pkts_lost += npkts;
	}
	/**
	 * Increase the number of DATA packet correctly received by the server
	 */
	virtual inline void
	incrPktRecv()
	{
		pkts_recv++;
	}
	/**
	 * Increase the number of DATA packets received out of order by the server
	 */
	virtual inline void
	incrPktOoseq()
	{
		pkts_ooseq++;
	}
	/**
	 * Increse the number of DATA packets received with error by the server
	 */
	virtual inline void
	incrPktInvalid()
	{
		pkts_invalid++;
	}
	/**
	 * return the number of packets sent by the server
	 *
	 * @return txsn
	 */
	virtual inline int
	getPktSent()
	{
		return txsn - 1;
	}
	/**
	 * return the number of DATA packets lost by the server
	 *
	 * @return pkts_lost
	 */
	virtual inline int
	getPktLost()
	{
		return pkts_lost;
	}
	/**
	 * return the number of DATA packet correctly received by the server
	 *
	 * @return pkts_recv
	 */
	virtual inline int
	getPktRecv()
	{
		return pkts_recv;
	}
	/**
	 * return the number of DATA packets received out of order by the server
	 *
	 * @return pkts_ooseq
	 */
	virtual inline int
	getPktsOOSequence()
	{
		return pkts_ooseq;
	}
	/**
	 * return the number of DATA packets received with error by the server
	 *
	 * @return pkts_invalid
	 */
	virtual inline int
	getPktsInvalidRx()
	{
		return pkts_invalid;
	}
	/**
	 * return the number of DATA packets sotred in the server queue
	 *
	 * @return pkts_push_queue
	 */
	virtual inline int
	getPktsPushQueue()
	{
		return pkts_push_queue;
	}
	/**
	 * Returns the average Round Trip Time
	 *
	 * @return the average Round Trip Time
	 */
	virtual double GetRTT() const;
	/**
	 * Return the standard deviation of the Round Trip Time calculated
	 *
	 * @return the standard deviation of the Round Trip Time calculated
	 */
	virtual double GetRTTstd() const;
	/**
	 * Update the RTT after the reception of a new packet
	 *
	 * @param RTT of the current packet received
	 */
	virtual void updateRTT(const double &rtt);
	/**
	 * Returns the average Forward Trip Time
	 *
	 * @return the average Forward Trip Time
	 *
	 */
	virtual double GetFTT() const;
	/**
	 * Return the standard deviation of the Forward Trip Time calculated
	 *
	 * @return the standard deviation of the Forward Trip Time calculated
	 */
	virtual double GetFTTstd() const;
	/**
	 * Rerturn the Packet Error Rate calculated
	 *
	 * @return the Packet Error Rate calculated
	 */
	virtual double GetPER() const;
	/**
	 * Return the Throughput calculated [bps]
	 *
	 * @return Throughput [bps]
	 */
	virtual double GetTHR() const;
	/**
	 * Update the FTT after the reception of a new packet
	 *
	 * @param FTT of the current packet received
	 */
	virtual void updateFTT(const double &ftt);
	/**
	 * Update the Throughput after the reception of a new packet
	 *
	 * @param Throughput of the current packet received
	 */
	virtual void updateThroughput(const int &bytes, const double &dt);

	/**************************************************************************
	 *                          VARIABLES                                     *
	 **************************************************************************/

	int servPort; /**< Server port*/
	
	int node_id;
	int exp_id;
	
	// TCL VARIABLES
	int debug_; /**< Used for debug purposes <i>1</i> debug activated <i>0</i>
				   debug not activated*/
	double PERIOD; /**< Interval time between two successive generation data
				   packets */
	int port_num; /**< Number of the port in which the server provide the
					 service */
	int drop_out_of_order; /**< Enable or not the ordering of data packet
							  received <i>1</i> enabled <i>0</i> not enabled*/
	uint8_t dst_addr; /**< IP destination address. */

	// TIMER VARIABLES
	uwSendTimerAppl
			chkTimerPeriod; /**< Timer that schedule the period between two
							   successive generation of DATA packets*/

	// STATISTICAL VARIABLES
	
	bool *sn_check; /**< Used to keep track of the packets already received. */
	int uidcnt; /**< Identifier counter that identify uniquely the DATA packet
				   generated*/
	int txsn; /**< Transmission sequence number of DATA packet */
	int rftt; /**< Forward trip time*/
	int pkts_lost; /**< Counter of the packet lost during the transmission */
	int pkts_recv; /**< Counter of the packets correctly received by the
					  server */
	int pkts_ooseq; /**< Counter of the packets received out of order by the
					   server */
	int pkts_invalid; /**< Counter of the packets received with errors by the
						 server */
	int pkts_push_queue; /**< Counter of DATA packets received by server and not
							yet passed to the below levels of ISO/OSI stack
							protocol*/
	int pkts_last_reset; /**< Used for error checking after stats are reset. Set
							to pkts_lost+pkts_recv each time resetStats is
							called. */
	double lrtime; /**< Time of last packet reception. */
	double sumrtt; /**< Sum of RTT samples. */
	double sumrtt2; /**< Sum of (RTT^2). */
	int rttsamples; /**< Number of RTT samples. */
	double sumftt; /**< Sum of FTT samples. */
	double sumftt2; /**< Sum of (FTT^2). */
	int fttsamples; /**< Number of FTT samples. */
	uint32_t esn; /**< Expected serial number. */
	double sumbytes; /**< Sum of bytes received. */
	double sumdt; /**< Sum of the delays. */
	int hrsn; /**< Highest received sequence number. */

	/** TCP listener thread class */
	TcpListener<uwAppNetModule> *p_tcp_listener{nullptr};

	/** Socket timeout for select() call in [us] */
	unsigned int m_SocketReadTimeout{50000};

}; // end uwApplication_module class

#endif /* UWAPPLICATION_MODULE_H */
