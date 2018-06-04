////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/***
Set of spoof classes that expose all BDV, wallet and address obj methods to SWIG
and handle the data transmission with the BDM server
***/

#ifndef _SWIGCLIENT_H
#define _SWIGCLIENT_H

#include "BDM_seder.h"
#include "StringSockets.h"
#include "bdmenums.h"
#include "log.h"
#include "TxClasses.h"
#include "BlockDataManagerConfig.h"
#include "WebSocketClient.h"
#include "AsyncClient.h"

class WalletManager;
class WalletContainer;

namespace SwigClient
{

   inline void StartCppLogging(string fname, int lvl) { STARTLOGGING(fname, (LogLevel)lvl); }
   inline void ChangeCppLogLevel(int lvl) { SETLOGLEVEL((LogLevel)lvl); }
   inline void DisableCppLogging() { SETLOGLEVEL(LogLvlDisabled); }
   inline void EnableCppLogStdOut() { LOGENABLESTDOUT(); }
   inline void DisableCppLogStdOut() { LOGDISABLESTDOUT(); }

#include <thread>

   class BlockDataViewer;

   ///////////////////////////////////////////////////////////////////////////////
   class LedgerDelegate
   {
   private:
      AsyncClient::LedgerDelegate asyncLed_;

   public:
      LedgerDelegate(void)
      {}

      LedgerDelegate(AsyncClient::LedgerDelegate&);

      vector<LedgerEntryData> getHistoryPage(uint32_t id);
   };

   class BtcWallet;

   ///////////////////////////////////////////////////////////////////////////////
   class ScrAddrObj
   {
   private:
      AsyncClient::ScrAddrObj asyncAddr_;

   public:
      ScrAddrObj(AsyncClient::ScrAddrObj&);

      uint64_t getFullBalance(void) const;
      uint64_t getSpendableBalance(void) const;
      uint64_t getUnconfirmedBalance(void) const;

      uint64_t getTxioCount(void) const;

      vector<UTXO> getSpendableTxOutList(bool);
      const BinaryData& getScrAddr(void) const;
      const BinaryData& getAddrHash(void) const;

      void setComment(const string& comment);
      const string& getComment(void) const;
      int getIndex(void) const;
   };

   ///////////////////////////////////////////////////////////////////////////////
   class BtcWallet
   {
      friend class ScrAddrObj;
      friend class ::WalletContainer;

   protected:
      AsyncClient::BtcWallet asyncWallet_;

   public:
      BtcWallet(AsyncClient::BtcWallet& wlt);
      vector<uint64_t> getBalancesAndCount(
         uint32_t topBlockHeight, bool IGNOREZC);

      vector<UTXO> getSpendableTxOutListForValue(uint64_t val);
      vector<UTXO> getSpendableZCList();
      vector<UTXO> getRBFTxOutList();

      map<BinaryData, uint32_t> getAddrTxnCountsFromDB(void);
      map<BinaryData, vector<uint64_t> > getAddrBalancesFromDB(void);

      vector<LedgerEntryData> getHistoryPage(uint32_t id);
      LedgerEntryData getLedgerEntryForTxHash(
         const BinaryData& txhash);

      ScrAddrObj getScrAddrObjByKey(const BinaryData&,
         uint64_t, uint64_t, uint64_t, uint32_t);

      vector<AddressBookEntry> createAddressBook(void) const;
   };

   ///////////////////////////////////////////////////////////////////////////////
   class Lockbox : public BtcWallet
   {
   private:
      AsyncClient::Lockbox asyncLockbox_;

   public:
      Lockbox(AsyncClient::Lockbox& asynclb);

      void getBalancesAndCountFromDB(uint32_t topBlockHeight, bool IGNOREZC);

      uint64_t getFullBalance(void) const;
      uint64_t getSpendableBalance(void) const;
      uint64_t getUnconfirmedBalance(void) const;
      uint64_t getWltTotalTxnCount(void) const;

      bool hasScrAddr(const BinaryData&) const;
   };

   ///////////////////////////////////////////////////////////////////////////////
   class Blockchain
   {
   private:
      AsyncClient::Blockchain asyncBlockchain_;

   public:
      Blockchain(const BlockDataViewer&);
      bool hasHeaderWithHash(const BinaryData& hash);
      ClientClasses::BlockHeader getHeaderByHeight(unsigned height);
   };

   ///////////////////////////////////////////////////////////////////////////////
   class BlockDataViewer
   {
      friend class ScrAddrObj;
      friend class BtcWallet;
      friend class RemoteCallback;
      friend class LedgerDelegate;
      friend class Blockchain;
      friend class ::WalletManager;

   private:
      AsyncClient::BlockDataViewer bdvAsync_;

   private:
      BlockDataViewer(void)
      {}

      BlockDataViewer(AsyncClient::BlockDataViewer&);
      
      bool isValid(void) const;

      const BlockDataViewer& operator=(const BlockDataViewer& rhs);

      void setTopBlock(unsigned block) const;

   public:
      ~BlockDataViewer(void);
      BtcWallet registerWallet(const string& id,
         const vector<BinaryData>& addrVec,
         bool isNew);

      Lockbox registerLockbox(const string& id,
         const vector<BinaryData>& addrVec,
         bool isNew);

      const string& getID(void) const;

      static BlockDataViewer getNewBDV(
         const string& addr, const string& port, SocketType);

      LedgerDelegate getLedgerDelegateForWallets(void);
      LedgerDelegate getLedgerDelegateForLockboxes(void);
      LedgerDelegate getLedgerDelegateForScrAddr(
         const string&, const BinaryData&);
      Blockchain blockchain(void);

      void goOnline(void);
      void registerWithDB(BinaryData magic_word);
      void unregisterFromDB(void);
      void shutdown(const string&);
      void shutdownNode(const string&);

      void broadcastZC(const BinaryData& rawTx);
      Tx getTxByHash(const BinaryData& txHash);
      BinaryData getRawHeaderForTxHash(const BinaryData& txHash);

      void updateWalletsLedgerFilter(const vector<BinaryData>& wltIdVec);
      bool hasRemoteDB(void);

      NodeStatusStruct getNodeStatus(void);
      unsigned getTopBlock(void) const;
      ClientClasses::FeeEstimateStruct estimateFee(unsigned, const string&);

      vector<LedgerEntryData> getHistoryForWalletSelection(
         const vector<string>& wldIDs, const string& orderingStr);

      uint64_t getValueForTxOut(const BinaryData& txHash, unsigned inputId);
      string broadcastThroughRPC(const BinaryData& rawTx);

      vector<UTXO> getUtxosForAddrVec(const vector<BinaryData>&);

      void registerAddrList(const BinaryData&, const vector<BinaryData>&);

      RemoteCallbackSetupStruct getRemoteCallbackSetupStruct(void) const
      { return bdvAsync_.getRemoteCallbackSetupStruct(); }
   };

   ///////////////////////////////////////////////////////////////////////////////
   class ProcessMutex
   {
   private:
      string addr_;
      string port_;
      thread holdThr_;

   private:

      void hodl();

   public:
      ProcessMutex(const string& addr, const string& port) :
         addr_(addr), port_(port)
      {}

      bool test(const string& uriStr);      
      bool acquire();
      
      virtual ~ProcessMutex(void) = 0;
      virtual void mutexCallback(const string&) = 0;
   };
};


#endif
