/**@file Declarations for TransactionTable and related classes. */
/*
* Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
* Copyright 2010 Kestrel Signal Processing, Inc.
* Copyright 2011, 2012 Range Networks, Inc.
*
* This software is distributed under multiple licenses;
* see the COPYING file in the main directory for licensing
* information for this specific distribuion.
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/



#ifndef TRANSACTIONTABLE_H
#define TRANSACTIONTABLE_H


#include <stdio.h>
#include <list>

#include <Logger.h>
#include <Interthread.h>
#include <Timeval.h>
#include <Sockets.h>


#include <GSML3CommonElements.h>
#include <GSML3MMElements.h>
#include <GSML3CCElements.h>
#include <GSML3RRElements.h>
#include <SIPEngine.h>

namespace GSM {
class LogicalChnanel;
class SACCHLogicalChannel;
}



struct sqlite3;


/**@namespace Control This namepace is for use by the control layer. */
namespace Control {

typedef std::map<std::string, GSM::Z100Timer> TimerTable;




/**
	A TransactionEntry object is used to maintain the state of a transaction
	as it moves from channel to channel.
	The object itself is not thread safe.
*/
class TransactionEntry {

	private:

	mutable Mutex mLock;					///< thread-safe control, shared from gTransactionTable

	/**@name Stable variables, fixed in the constructor or written only once. */
	//@{
	unsigned mID;							///< the internal transaction ID, assigned by a TransactionTable

	GSM::L3MobileIdentity mSubscriber;		///< some kind of subscriber ID, preferably IMSI
	GSM::L3CMServiceType mService;			///< the associated service type
	unsigned mL3TI;							///< the L3 short transaction ID, the version we *send* to the MS

	GSM::L3CalledPartyBCDNumber mCalled;	///< the associated called party number, if known
	GSM::L3CallingPartyBCDNumber mCalling;	///< the associated calling party number, if known

	// TODO -- This should be expaned to deal with long messages.
	//char mMessage[522];						///< text messaging payload
	std::string mMessage;					///< text message payload
	std::string mContentType;				///< text message payload content type
	//@}

	SIP::SIPEngine mSIP;					///< the SIP IETF RFC-3621 protocol engine
	mutable SIP::SIPState mPrevSIPState;	///< previous SIP state, prior to most recent transactions
	GSM::CallState mGSMState;				///< the GSM/ISDN/Q.931 call state
	Timeval mStateTimer;					///< timestamp of last state change.
	TimerTable mTimers;						///< table of Z100-type state timers

	/**@name Handover parameters */
	//@{
	/**@name Inbound */
	//@{
	struct ::sockaddr_in mInboundPeer;		///< other BTS in inbound handover
	unsigned mInboundReference;			///< handover reference
	float mInboundRSSI;				///< access burst RSSI in dB wrt full scale
	float mInboundTimingError;			///< access burst timing error in symbol periods
	unsigned mCSeq;
	string mCallID;
	unsigned mCodec;
	string mRTPState;
	string mSessionID;
	string mSessionVersion;
	string mRemoteUsername;
	string mRemoteDomain;
	string mSIPDisplayname;
	string mSIPUsername;
	string mFromTag;
	string mFromUsername;
	string mFromIP;
	string mToTag;
	string mToUsername;
	string mToIP;
	string mCallIP;
	short mRTPRemPort;
	string mRTPRemIP;
	short mRmtPort;
	string mRmtIP;
	string mSRIMSI;
	string mSRCALLID;

	//@}
	/**@name Outbound */
	//@{
	struct ::sockaddr_in mOutboundPeer;		///< other BTS in outbound handover
	GSM::L3CellDescription mOutboundCell;
	GSM::L3ChannelDescription2 mOutboundChannel;
	GSM::L3HandoverReference mOutboundReference;
	GSM::L3PowerCommandAndAccessType mOutboundPowerCmd;
	GSM::L3SynchronizationIndication mOutboundSynch;
	//@}
	//@}

	unsigned mNumSQLTries;					///< number of SQL tries for DB operations

	GSM::LogicalChannel *mChannel;			///< current channel of the transaction

	bool mTerminationRequested;

	unsigned mHandoverOtherBSTransactionID;

	volatile bool mRemoved;			///< true if ready for removal

	public:

	/** This form is used for MTC or MT-SMS with TI generated by the network. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber, 
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		const GSM::L3CallingPartyBCDNumber& wCalling,
		GSM::CallState wState = GSM::NullState,
		const char *wMessage = NULL);

	/** This form is used for MOC, setting mGSMState to MOCInitiated. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		unsigned wL3TI,
		const GSM::L3CalledPartyBCDNumber& wCalled);

	/** Form for MO-SMS; sets yet-unknown TI to 7 and GSM state to SMSSubmitting */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CalledPartyBCDNumber& wCalled,
		const char* wMessage);

	/** Form for MO-SMS with a parallel call; sets yet-unknown TI to 7 and GSM state to SMSSubmitting */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel);

	/** Form used for handover requests; argument is taken from the message string. */
	TransactionEntry(const struct ::sockaddr_in* peer,
		unsigned wHandoverReference,
		SimpleKeyValue &params,
		const char *proxy,
		GSM::LogicalChannel* wChannel,
		unsigned otherTransactionID);


	/** Delete the database entry upon destruction. */
	~TransactionEntry();

	/**@name Accessors. */
	//@{
	unsigned L3TI() const;
	void L3TI(unsigned wL3TI);

	const GSM::LogicalChannel* channel() const;
	GSM::LogicalChannel* channel();

	void channel(GSM::LogicalChannel* wChannel);

	const GSM::L3MobileIdentity& subscriber() const { return mSubscriber; }

	const GSM::L3CMServiceType& service() const { return mService; }

	const GSM::L3CalledPartyBCDNumber& called() const { return mCalled; }
	void called(const GSM::L3CalledPartyBCDNumber&);

	const GSM::L3CallingPartyBCDNumber& calling() const { return mCalling; }

	const char* message() const { return mMessage.c_str(); }
	void message(const char *wMessage, size_t length);
	const char* messageType() const { return mContentType.c_str(); }
	void messageType(const char *wContentType);

	unsigned ID() const { return mID; }

	GSM::CallState GSMState() const;
	void GSMState(GSM::CallState wState);

	/** Inbound reference set in the constructor, no lock needed. */
	// FIXME -- These should probably all the const references.
	unsigned inboundReference() const { return mInboundReference; }
	unsigned Codec() { return mCodec; }
	unsigned CSeq() { return mCSeq; }
	string CallID() { return mCallID; }
	string RemoteUsername() { return mRemoteUsername; }
	string RemoteDomain() { return mRemoteDomain; }
	string SIPUsername() { return mSIPUsername; }
	string SIPDisplayname() { return mSIPDisplayname; }
	string FromTag() { return mFromTag; }
	string FromUsername() { return mFromUsername; }
	string FromIP() { return mFromIP; }
	string ToTag() { return mToTag; }
	string ToUsername() { return mToUsername; }
	string ToIP() { return mToIP; }
	string CallIP() { return mCallIP; }
	short RTPRemPort() { return mRTPRemPort; }
	string RTPRemIP() { return mRTPRemIP; }
	string RTPState() { return mRTPState; }
	string SessionID() { return mSessionID; }
	string SessionVersion() { return mSessionVersion; }
	short RmtPort() { return mRmtPort; }
	string RmtIP() { return mRmtIP; }
	string SRIMSI() { return mSRIMSI; }
	string SRCALLID() { return mSRCALLID; }
	unsigned HandoverOtherBSTransactionID() { return mHandoverOtherBSTransactionID; }

	GSM::L3HandoverReference outboundReference() const { ScopedLock lock(mLock); return mOutboundReference; }
	GSM::L3CellDescription outboundCell() const { ScopedLock lock(mLock); return mOutboundCell; }
	GSM::L3ChannelDescription2 outboundChannel() const { ScopedLock lock(mLock); return mOutboundChannel; }
	GSM::L3PowerCommandAndAccessType outboundPowerCmd() const { ScopedLock lock(mLock); return mOutboundPowerCmd; }
	GSM::L3SynchronizationIndication outboundSynch() const { ScopedLock lock(mLock); return mOutboundSynch; }

	/** Set the outbound handover parameters and set the state to HandoverOutbound. */
	void setOutboundHandover(
		const GSM::L3HandoverReference& reference,
		const GSM::L3CellDescription& cell,
		const GSM::L3ChannelDescription2& chan,
		const GSM::L3PowerCommandAndAccessType& pwrCmd,
		const GSM::L3SynchronizationIndication& synch
			);

	/** Set the inbound handover parameters on the channel; state should alread be HandoverInbound. */
	void setInboundHandover(
		float wRSSI,
		float wTimingError
			);

	// This is thread-safe because mInboundPeer is only modified in the constructor.
	const struct ::sockaddr_in* inboundPeer() const { return &mInboundPeer; }

	float inboundTimingError() const { ScopedLock lock(mLock); return mInboundTimingError; }

	//@}


	/** Initiate the termination process. */
	void terminate() { ScopedLock lock(mLock); mTerminationRequested=true; }

	bool terminationRequested();

	/**@name SIP-side operations */
	//@{

	SIP::SIPState SIPState() { ScopedLock lock(mLock); return mSIP.state(); } 

	bool SIPFinished() { ScopedLock lock(mLock); return mSIP.finished(); } 

	bool instigator() { ScopedLock lock(mLock); return mSIP.instigator(); }

	SIP::SIPState MOCSendINVITE(const char* calledUser, const char* calledDomain, short rtpPort, unsigned codec);
	SIP::SIPState MOCResendINVITE();
	SIP::SIPState MOCCheckForOK();
	SIP::SIPState MOCSendACK();
	void MOCInitRTP() { ScopedLock lock(mLock); return mSIP.MOCInitRTP(); }

	SIP::SIPState MTCSendTrying();
	SIP::SIPState MTCSendRinging();
	SIP::SIPState MTCCheckForACK();
	SIP::SIPState MTCCheckForCancel();
	SIP::SIPState MTCSendOK(short rtpPort, unsigned codec);
	void MTCInitRTP() { ScopedLock lock(mLock); mSIP.MTCInitRTP(); }

	SIP::SIPState MODSendBYE();
	SIP::SIPState MODSendERROR(osip_message_t * cause, int code, const char * reason, bool cancel);
	SIP::SIPState MODSendCANCEL();
	SIP::SIPState MODResendBYE();
	SIP::SIPState MODResendCANCEL();
	SIP::SIPState MODResendERROR(bool cancel);
	SIP::SIPState MODWaitForBYEOK();
	SIP::SIPState MODWaitForCANCELOK();
	SIP::SIPState MODWaitForERRORACK(bool cancel);
	SIP::SIPState MODWaitFor487();
	SIP::SIPState MODWaitForResponse(vector<unsigned> *validResponses);

	SIP::SIPState MTDCheckBYE();
	SIP::SIPState MTDSendBYEOK();
	SIP::SIPState MTDSendCANCELOK();

	// TODO: Remove contentType from here and use the setter above.
	SIP::SIPState MOSMSSendMESSAGE(const char* calledUser, const char* calledDomain, const char* contentType);
	SIP::SIPState MOSMSWaitForSubmit();

	SIP::SIPState MTSMSSendOK();

	SIP::SIPState inboundHandoverSendINVITE(unsigned RTPPort)
		{ ScopedLock lock(mLock); return mSIP.inboundHandoverSendINVITE(this, RTPPort); }
	SIP::SIPState inboundHandoverCheckForOK()
		{ ScopedLock lock(mLock); return mSIP.inboundHandoverCheckForOK(&mLock); }
	SIP::SIPState inboundHandoverSendACK()
		{ ScopedLock lock(mLock); return mSIP.inboundHandoverSendACK(); }

	bool sendINFOAndWaitForOK(unsigned info);

	void txFrame(unsigned char* frame) { ScopedLock lock(mLock); return mSIP.txFrame(frame); }
	int rxFrame(unsigned char* frame) { ScopedLock lock(mLock); return mSIP.rxFrame(frame); }
	bool startDTMF(char key) { ScopedLock lock(mLock); return mSIP.startDTMF(key); }
	void stopDTMF() { ScopedLock lock(mLock); mSIP.stopDTMF(); }

	void SIPUser(const std::string& IMSI) { ScopedLock lock(mLock); SIPUser(IMSI.c_str()); }
	void SIPUser(const char* IMSI);
	void SIPUser(const char* callID, const char *IMSI , const char *origID, const char *origHost);

	const std::string SIPCallID() const { ScopedLock lock(mLock); return mSIP.callID(); }

	// These are called by SIPInterface.
	void saveINVITE(const osip_message_t* invite, bool local)
		{ ScopedLock lock(mLock); mSIP.saveINVITE(invite,local); }
	void saveBYE(const osip_message_t* bye, bool local)
		{ ScopedLock lock(mLock); mSIP.saveBYE(bye,local); }

	bool sameINVITE(osip_message_t * msg)
		{ ScopedLock lock(mLock); return mSIP.sameINVITE(msg); }

	//@}

	unsigned stateAge() const { ScopedLock lock(mLock); return mStateTimer.elapsed(); }

	/**@name Timer access. */
	//@{

	bool timerExpired(const char* name) const;

	void setTimer(const char* name);

	void setTimer(const char* name, long newLimit);

	void resetTimer(const char* name);

	/** Return true if any Q.931 timer is expired. */
	bool anyTimerExpired() const;

	/** Reset all Q.931 timers. */
	void resetTimers();
	
	//@}

	/** Return true if clearing is in progress in the GSM side. */
	bool clearingGSM() const;

	/** Retrns true if the transaction is "dead". */
	bool dead() const;

	/** Returns true if dead, or if removal already requested. */
	bool deadOrRemoved() const;

	/** Dump information as text for debugging. */
	void text(std::ostream&) const;

	/** Genrate an encoded string for handovers. */
	std::string handoverString() const;

	private:

	friend class TransactionTable;

	/** Create L3 timers from GSM and Q.931 (network side) */
	void initTimers();

	/** Set up a new entry in gTransactionTable's sqlite3 database. */
	void insertIntoDatabase();

	/** Run a database query. */
	void runQuery(const char* query) const;

	/** Echo latest SIPSTATE to the database. */
	SIP::SIPState echoSIPState(SIP::SIPState state) const;

	/** Tag for removal. */
	void remove() { mRemoved=true; mStateTimer.now(); }

	/** Removal status. */
	bool removed() { return mRemoved; }
};


std::ostream& operator<<(std::ostream& os, const TransactionEntry&);


/** A map of transactions keyed by ID. */
class TransactionMap : public std::map<unsigned,TransactionEntry*> {};

/**
	A table for tracking the states of active transactions.
*/
class TransactionTable {

	private:

	sqlite3 *mDB;			///< database connection

	TransactionMap mTable;
	mutable Mutex mLock;
	unsigned mIDCounter;

	public:

	/**
		Initialize a transaction table.
		@param path Path fto sqlite3 database file.
	*/
	void init(const char* path);

	~TransactionTable();

	// TransactionTable does not need a destructor.

	/**
		Return a new ID for use in the table.
	*/
	unsigned newID();

	/**
		Insert a new entry into the table; deleted by the table later.
		@param value The entry to insert into the table; will be deleted by the table later.
	*/
	void add(TransactionEntry* value);

	/**
		Find an entry and return a pointer into the table.
		@param wID The transaction ID to search
		@return NULL if ID is not found or was dead
	*/
	TransactionEntry* find(unsigned wID);

	/**
		Find the longest-running non-SOS call.
		@return NULL if there are no calls or if all are SOS.
	*/
	TransactionEntry* findLongestCall();

	/**
		Return the availability of this particular RTP port
		@return True if Port is available, False otherwise
	*/
	bool RTPAvailable(short rtpPort);

	/**
		Fand an entry by its handover reference.
		@param ref The 8-bit handover reference.
		@return NULL if ID is not found or was dead
	*/
	TransactionEntry* inboundHandover(unsigned ref);

	/**
		Remove an entry from the table and from gSIPMessageMap.
		@param wID The transaction ID to search.
		@return True if the ID was really in the table and deleted.
	*/
	bool remove(unsigned wID);

	bool remove(TransactionEntry* transaction) { return remove(transaction->ID()); }

	/**
		Remove an entry from the table and from gSIPMessageMap,
		if it is in the Paging state.
		@param wID The transaction ID to search.
		@return True if the ID was really in the table and deleted.
	*/
	bool removePaging(unsigned wID);


	/**
		Find an entry by its channel pointer; returns first entry found.
		Also clears dead entries during search.
		@param chan The channel pointer.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* find(const GSM::LogicalChannel *chan);

	/** Find a transaction in the HandoverInbound state on the given channel. */
	TransactionEntry* inboundHandover(const GSM::LogicalChannel *chan);

	/**
		Find an entry by its SACCH channel pointer; returns first entry found.
		Also clears dead entries during search.
		@param chan The channel pointer.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* findBySACCH(const GSM::SACCHLogicalChannel *chan);

	/**
		Find an entry by its channel type and offset.
		Also clears dead entries during search.
		@param chan The channel pointer to the first record found.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* find(GSM::TypeAndOffset chanDesc);

	/**
		Find an entry in the given state by its mobile ID.
		Also clears dead entries during search.
		@param mobileID The mobile to search for.
		@return pointer to entry or NULL if no match
	*/
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, GSM::CallState state);

	/** Return true if there is an ongoing call for this user. */
	bool isBusy(const GSM::L3MobileIdentity& mobileID);


	/** Find by subscriber and SIP call ID. */
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, const char* callID);

	/** Find by subscriber and handover other BS transaction ID. */
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, unsigned transactionID);

	/** Check for duplicated SMS delivery attempts. */
	bool duplicateMessage(const GSM::L3MobileIdentity& mobileID, const std::string& wMessage);

	/**
		Find an entry in the Paging state by its mobile ID, change state to AnsweredPaging and reset T3113.
		Also clears dead entries during search.
		@param mobileID The mobile to search for.
		@return pointer to entry or NULL if no match
	*/
	TransactionEntry* answeredPaging(const GSM::L3MobileIdentity& mobileID);


	/**
		Find the channel, if any, used for current transactions by this mobile ID.
		@param mobileID The target mobile subscriber.
		@return pointer to TCH/FACCH, SDCCH or NULL.
	*/
	GSM::LogicalChannel* findChannel(const GSM::L3MobileIdentity& mobileID);

	/** Count the number of transactions using a particular channel. */
	unsigned countChan(const GSM::LogicalChannel*);

	size_t size() { ScopedLock lock(mLock); return mTable.size(); }

	size_t dump(std::ostream& os, bool showAll=false) const;

	/** Generate a unique handover reference. */
	//unsigned generateInboundHandoverReference(TransactionEntry* transaction);

	private:

	friend class TransactionEntry;

	/** Accessor to database connection. */
	sqlite3* DB() { return mDB; }

	/**
		Remove "dead" entries from the table.
		A "dead" entry is a transaction that is no longer active.
		The caller should hold mLock.
	*/
	void clearDeadEntries();

	/**
		Remove and entry from the table and from gSIPInterface.
	*/
	void innerRemove(TransactionMap::iterator);


	/** Check to see if a given outbound handover reference is in use. */
	//bool outboundReferenceUsed(unsigned ref);

};





}	//Control



/**@addtogroup Globals */
//@{
/** A single global transaction table in the global namespace. */
extern Control::TransactionTable gTransactionTable;
//@}



#endif

// vim: ts=4 sw=4

