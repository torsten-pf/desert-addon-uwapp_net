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
 * @file   uwAppNet_module.cpp (based on uwApplication_module.cc)
 * @author Federico Favaro
 * @version 1.1.0
 *
 * \brief Provides the implementation of uwAppNetModule class
 *
 */

#include "uwAppNet_cmn_header.h"
#include "uwAppNet_module.h"
#include <tclcl.h>
#include <climits> // USHRT_MAX

static class uwAppNetModuleClass : public TclClass
{
public:
	/**
	 * Constructor of uwAppNetModuleClass class
	 */
	uwAppNetModuleClass()
		: TclClass("Module/UW/APPNET")
	{
	}

	/**
	 *  Creates the TCL object needed for the TCL language interpretation
	 *
	 * @return Pointer to an TclObject
	 */
	TclObject *
	create(int, const char *const *)
	{
		return (new uwAppNetModule());
	}
} class_module_uwAppNetModule;

uwAppNetModule::uwAppNetModule()
	: servPort(0)
	, node_id(0)
	, exp_id(0)
	, debug_(0)
	, PERIOD(1.0)
	, port_num(55550)
	, drop_out_of_order(0)
	, dst_addr(0)
	, chkTimerPeriod(this)
	, sn_check()
	, uidcnt(0)
	, txsn(1)
	, rftt(0)
	, pkts_lost(0)
	, pkts_recv(0)
	, pkts_ooseq(0)
	, pkts_invalid(0)
	, pkts_push_queue(0)	
	, pkts_last_reset(0)
	, lrtime(0)
	, sumrtt(0)
	, sumrtt2(0)
	, rttsamples(0)
	, sumftt(0)
	, sumftt2(0)
	, fttsamples(0)
	, esn(0)
	, sumbytes(0)
	, sumdt(0)
	, hrsn(0)
{
	bind("debug_", (int *) &debug_);
	bind("period_", (double *) &PERIOD);
	bind("node_ID_", (int *) &node_id);
	bind("EXP_ID_", (int *) &exp_id);
	bind("destAddr_", (int *) &dst_addr);
	bind("destPort_", (int *) &port_num);
	bind("Socket_Port_", (int *) &servPort);
	bind("drop_out_of_order_", (int *) &drop_out_of_order);

	bind("SocketReadTimeout", &m_SocketReadTimeout);

	sn_check = new bool[USHRT_MAX];
	for (int i = 0; i < USHRT_MAX; i++) {
		sn_check[i] = false;
	}
	servPort = port_num;
} // end uwAppNetModule() Method

uwAppNetModule::~uwAppNetModule()
{
	stop();
}

int
uwAppNetModule::command(int argc, const char *const *argv)
{
	Tcl &tcl = Tcl::instance();

	if (argc == 2) {
		if (strcasecmp(argv[1], "start") == 0) {
			timeval timeout; timeout.tv_sec = 0; timeout.tv_usec = m_SocketReadTimeout;
			p_tcp_listener = new TcpListener(this, servPort, timeout);
			if (p_tcp_listener && p_tcp_listener->Setup()) {
				chkTimerPeriod.resched(PERIOD);
				p_tcp_listener->Start();
				return TCL_OK;				
			}
			LOG_MSG_ERROR("TCP server socket was not created successfully!")
			return TCL_ERROR;
		} else if (strcasecmp(argv[1], "stop") == 0) {
			stop();
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getsentpkts") == 0) {
			tcl.resultf("%d", getPktSent());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "lostpkts") == 0) {
			tcl.resultf("%f", getPktLost());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getrecvpkts") == 0) {
			tcl.resultf("%d", getPktRecv());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "outofsequencepkts") == 0) {
			tcl.resultf("%f", getPktsOOSequence());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "notknownpktrx") == 0) {
			tcl.resultf("%f", getPktsInvalidRx());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getrecvpktsqueue") == 0) {
			tcl.resultf("%d", getPktsPushQueue());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getrtt") == 0) {
			tcl.resultf("%f", GetRTT());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getrttstd") == 0) {
			tcl.resultf("%f", GetRTTstd());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getftt") == 0) {
			tcl.resultf("%f", GetFTT());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getfttstd") == 0) {
			tcl.resultf("%f", GetFTTstd());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getper") == 0) {
			tcl.resultf("%f", GetPER());
			return TCL_OK;
		} else if (strcasecmp(argv[1], "getthr") == 0) {
			tcl.resultf("%f", GetTHR());
			return TCL_OK;
		} 
	} else if (argc == 3) {
		/*if (strcasecmp(argv[1], "SetSocketProtocol") == 0) {
			string protocol = argv[2];
			if (strcasecmp(protocol.c_str(), "UDP") == 0) {
				socket_active = true;
				tcp_udp = 0;
			} else if (strcasecmp(protocol.c_str(), "TCP") == 0) {
				socket_active = true;
				tcp_udp = 1;
			} else {
				socket_active = false;
				tcp_udp = -1;
			}
			return TCL_OK;
		}*/
	}
	return Module::command(argc, argv);
} // end command() Method

void
uwAppNetModule::recv(Packet *p)
{
	// Communication take place with sockets	
	if (debug_ >= 1)
		std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
					<< "::UWAPPNET::RECV_PACKET_USING_TCP" << std::endl;
	statistics(p);
	
}; // end recv() Method

void
uwAppNetModule::statistics(Packet *p)
{
	hdr_cmn *ch = hdr_cmn::access(p);
	hdr_DATA_APPLICATION *uwApph = HDR_DATA_APPLICATION(p);

	if (ch->ptype_ != PT_DATA_APPLICATION) {
		if (debug_ >= 0)
			std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
					  << "::UWAPPNET::DROP_PACKET_NOT_APPLICATION_TYPE"
					  << std::endl;
		drop(p, 1, UWAPPLICATION_DROP_REASON_UNKNOWN_TYPE);
		incrPktInvalid(); // Increment the number of packet received invalid
		return;
	}
	esn = hrsn + 1; // Increase the expected sequence number

	// Verify if the data packet is already processed.
	if (useDropOutOfOrder()) {
		if (sn_check[uwApph->sn_ &
					0x00ffffff]) { // Packet already processed: drop it
			if (debug_ >= 0)
				std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
						  << "::UWAPPNET::DROP_PACKET_PACKET_ALREADY_"
							 "PROCESSED_ID_"
						  << (int) uwApph->sn_ << std::endl;
			incrPktInvalid();
			drop(p, 1, UWAPPLICATION_DROP_REASON_DUPLICATED_PACKET);
			return;
		}
	}
	// The data packet with this particular SN is not already processed
	// Set true sn_check. In this way we assume that these sn are already
	// processed by the node
	sn_check[uwApph->sn_ & 0x00ffffff] = true;

	// The data packet received is out of order
	if (useDropOutOfOrder()) {
		if (uwApph->sn_ <
				esn) { // packet is out of sequence and is to be discarded
			incrPktOoseq(); // Increase the number of data packets receive out
							// of
			// sequence.
			if (debug_ >= 0)
				std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
						  << "::UWAPPNET::DROP_PACKET_PACKET_OOS_ID_"
						  << (int) uwApph->sn_ << "_LAST_SN_" << hrsn << std::endl;
			drop(p, 1, UWAPPLICATION_DROP_REASON_OUT_OF_SEQUENCE);
			return;
		}
	}

	// Compute the Forward Trip time
	rftt = Scheduler::instance().clock() - ch->timestamp();
	if ((uwApph->rftt_valid_) / 10000) {
		double rtt = rftt + uwApph->rftt();
		updateRTT(rtt);
	}

	updateFTT(rftt); // Update the forward trip time

	hrsn = uwApph->sn_; // Update the highest sequence number received

	// Verify if a packet is lost
	if (useDropOutOfOrder()) {
		if (uwApph->sn_ > esn) {
			if (debug_ >= 0)
				std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
						  << "::UWAPPNET::PACKET_LOST_ID_RECEIVED"
						  << (int) uwApph->sn_ << "_ID_EXPECTED_" << esn
						  << std::endl;
			incrPktLost(uwApph->sn_ - (esn));
		}
	}

	double dt = Scheduler::instance().clock() - lrtime;
	// updateThroughput(ch->size(), dt); //Update Throughput
	updateThroughput(uwApph->payload_size(), dt);
	incrPktRecv(); // Increase the number of data packets received

	lrtime = Scheduler::instance().clock(); // Update the time in which the last
											// packet is received.
	if (debug_ >= 0) {
		std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
				  << "::UWAPPNET::PAYLOAD_RECEIVED--> ";
		for (int i = 0; i < uwApph->payload_size(); i++) {
			std::cout << uwApph->payload_msg[i];
		}
		std::cout << std::endl;
	}
	if (debug_ >= 0)
		std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
				  << "::UWAPPNET::SN_RECEIVED_" << (int) uwApph->sn_
				  << std::endl;
	if (debug_ >= 0)
		std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
				  << "::UWAPPNET::PAYLOAD_SIZE_RECEIVED_"
				  << (int) uwApph->payload_size() << std::endl;
	if (debug_ >= 1)
		std::cout << "[" << getEpoch() << "]::" << NOW << "::" << getNodeId() 
				  << "::UWAPPNET::PAYLOAD_RECEIVED_" << uwApph->payload_msg
				  << std::endl;

	if (p_tcp_listener) {
		p_tcp_listener->send_data(uwApph->payload_msg, (size_t)uwApph->payload_size());
	}		
	Packet::free(p);
} // end statistics method

void
uwAppNetModule::stop()
{
	// Close the connection
	chkTimerPeriod.force_cancel();	
	if (p_tcp_listener) {
		if (p_tcp_listener->Running()) p_tcp_listener->Stop(true);
		delete p_tcp_listener;
		p_tcp_listener = nullptr;
	}
} // end stop() method



double
uwAppNetModule::GetRTT() const
{
	return (rttsamples > 0) ? sumrtt / rttsamples : 0;
} // end GetRTT() method

double
uwAppNetModule::GetRTTstd() const
{
	if (rttsamples > 1) {
		double var =
				(sumrtt2 - (sumrtt * sumrtt / rttsamples)) / (rttsamples - 1);
		return (sqrt(var));
	} else
		return 0;
} // end GetRTTstd() method

void
uwAppNetModule::updateRTT(const double &rtt)
{
	sumrtt += rtt;
	sumrtt2 += rtt * rtt;
	rttsamples++;
} // end updateRTT() method

double
uwAppNetModule::GetFTT() const
{
	return (fttsamples > 0) ? sumftt / fttsamples : 0;
} // end getFTT() method

double
uwAppNetModule::GetFTTstd() const
{
	if (fttsamples > 1) {
		double var = 0;
		var = (sumftt2 - (sumftt * sumftt / fttsamples)) / (fttsamples - 1);
		if (var > 0)
			return (sqrt(var));
		else
			return 0;
	} else {
		return 0;
	}
} // end getFTT() method

double
uwAppNetModule::GetPER() const
{
	if (drop_out_of_order) {
		if ((pkts_recv + pkts_lost) > 0) {
			return ((double) pkts_lost / (double) (pkts_recv + pkts_lost));
		} else {
			return 0;
		}
	} else {
		if (esn > 1)
			return (1 - (double) pkts_recv / (double) (esn - 1));
		else
			return 0;
	}
} // end getPER() method

double
uwAppNetModule::GetTHR() const
{
	return ((sumdt != 0) ? sumbytes * 8 / sumdt : 0);
} // end GetTHR() method

void
uwAppNetModule::updateFTT(const double &ftt)
{
	sumftt += ftt;
	sumftt2 += ftt * ftt;
	fttsamples++;
} // end updateFTT() method

void
uwAppNetModule::updateThroughput(const int &bytes, const double &dt)
{
	sumbytes += bytes;
	sumdt += dt;
} // end updateThroughput() method

void
uwAppNetModule::uwSendTimerAppl::expire(Event *e)
{
	// The protocol that the system is using is TCP
	if (m_->p_tcp_listener)
		m_->p_tcp_listener->init_Packet_TCP();
	m_->chkTimerPeriod.resched(m_->PERIOD);
} // end expire();
