////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _SOCKETOBJ_H
#define _SOCKETOBJ_H

#include <sys/types.h>
#include <string>
#include <sstream>
#include <stdint.h>
#include <functional>
#include <memory>

#ifndef _WIN32
#include <poll.h>
#define socketService socketService_nix
#else
#define socketService socketService_win
#endif

#include "ThreadSafeClasses.h"
#include "bdmenums.h"
#include "log.h"

#include "SocketIncludes.h"
#include "BinaryData.h"

using namespace std;
   
typedef function<bool(vector<uint8_t>, exception_ptr)>  ReadCallback;

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn
{
   virtual ~CallbackReturn(void) = 0;
   virtual void callback(const BinaryDataRef&, exception_ptr) = 0;
};

struct CallbackReturn_CloseBitcoinP2PSocket : public CallbackReturn
{
private:
   shared_ptr<BlockingStack<vector<uint8_t>>> dataStack_;

public:
   CallbackReturn_CloseBitcoinP2PSocket(
      shared_ptr<BlockingStack<vector<uint8_t>>> datastack) :
      dataStack_(datastack)
   {}

   void callback(const BinaryDataRef& bdr, exception_ptr eptr) 
   { dataStack_->terminate(eptr); }
};

///////////////////////////////////////////////////////////////////////////////
struct Socket_ReadPayload
{
   unsigned id_;
   unique_ptr<CallbackReturn> callbackReturn_ = nullptr;

   Socket_ReadPayload(unsigned id) :
      id_(id)
   {}
};

////
struct Socket_WritePayload
{
   vector<uint8_t> data_;
};

////
struct AcceptStruct
{
   SOCKET sockfd_;
   sockaddr saddr_;
   socklen_t addrlen_;
   ReadCallback readCallback_;

   AcceptStruct(void) :
      addrlen_(sizeof(saddr_))
   {}
};

///////////////////////////////////////////////////////////////////////////////
class SocketPrototype
{
   friend class FCGI_Server;
   friend class ListenServer;

private: 
   bool blocking_ = true;

protected:

public:
   typedef function<bool(const vector<uint8_t>&)>  SequentialReadCallback;
   typedef function<void(AcceptStruct)> AcceptCallback;

protected:
   const size_t maxread_ = 4*1024*1024;
   
   struct sockaddr serv_addr_;
   const string addr_;
   const string port_;

   bool verbose_ = true;

private:
   void init(void);

protected:   
   void setBlocking(SOCKET, bool);
   void listen(AcceptCallback, SOCKET& sockfd);

   SocketPrototype(void) :
      addr_(""), port_("")
   {}
   
public:
   SocketPrototype(const string& addr, const string& port, bool init = true);
   virtual ~SocketPrototype(void) = 0;

   bool testConnection(void);
   bool isBlocking(void) const { return blocking_; }
   SOCKET openSocket(bool blocking);
   
   static void closeSocket(SOCKET&);
   virtual void pushPayload(
      Socket_WritePayload&, shared_ptr<Socket_ReadPayload>) = 0;
   virtual bool connectToRemote(void) = 0;

   virtual SocketType type(void) const = 0;
   const string& getAddrStr(void) const { return addr_; }
   const string& getPortStr(void) const { return port_; }
};

///////////////////////////////////////////////////////////////////////////////
class SimpleSocket : public SocketPrototype
{
protected:
   SOCKET sockfd_ = SOCK_MAX;

private:
   int writeToSocket(vector<uint8_t>&);

public:
   SimpleSocket(const string& addr, const string& port) :
      SocketPrototype(addr, port)
   {}
   
   SimpleSocket(SOCKET sockfd) :
      SocketPrototype(), sockfd_(sockfd)
   {}

   ~SimpleSocket(void)
   {
      closeSocket(sockfd_);
   }

   SocketType type(void) const { return SocketSimple; }

   void pushPayload(
      Socket_WritePayload&, shared_ptr<Socket_ReadPayload>);
   vector<uint8_t> readFromSocket(void);
   void shutdown(void);
   void listen(AcceptCallback);
   bool connectToRemote(void);
   SOCKET getSockFD(void) const { return sockfd_; }
};

///////////////////////////////////////////////////////////////////////////////
class PersistentSocket : public SocketPrototype
{
   friend class ListenServer;

private:
   SOCKET sockfd_ = SOCK_MAX;
   vector<thread> threads_;
   
   vector<uint8_t> writeLeftOver_;
   size_t writeOffset_ = 0;

   atomic<bool> run_;

#ifdef _WIN32
   WSAEVENT events_[2];
#else
   SOCKET pipes_[2];
#endif

   BlockingStack<vector<uint8_t>> readQueue_;
   Stack<vector<uint8_t>> writeQueue_;

private:
   void signalService(uint8_t);
#ifdef _WIN32
   void socketService_win(void);
   void setSocketPollEvent(
      SOCKET sockfd, WSAEVENT& wsaEvent, unsigned flags);
#else
   void socketService_nix(void);
#endif
   void readService(void);
   void initPipes(void);
   void cleanUpPipes(void);
   void init(void);

protected:
   virtual bool processPacket(vector<uint8_t>&, vector<uint8_t>&);
   virtual void respond(vector<uint8_t>&) = 0;
   void queuePayloadForWrite(Socket_WritePayload&);

public:
   PersistentSocket(const string& addr, const string& port) :
      SocketPrototype(addr, port)
   {
      init();
   }

   PersistentSocket(SOCKET sockfd) :
      SocketPrototype(), sockfd_(sockfd)
   {
      init();
   }

   ~PersistentSocket(void)
   {
      shutdown();
   }

   void shutdown();
   bool openSocket(bool blocking);
   int getSocketName(struct sockaddr& sa);
   int getPeerName(struct sockaddr& sa);
   bool connectToRemote(void);
   bool isValid(void) const { return sockfd_ != SOCK_MAX; }
};

///////////////////////////////////////////////////////////////////////////////
class ListenServer
{
private:
   struct SocketStruct
   {
   private:
      SocketStruct(const SocketStruct&) = delete;

   public:
      SocketStruct(void)
      {}

      shared_ptr<SimpleSocket> sock_;
      thread thr_;
   };

private:
   unique_ptr<SimpleSocket> listenSocket_;
   map<SOCKET, unique_ptr<SocketStruct>> acceptMap_;
   Stack<SOCKET> cleanUpStack_;

   thread listenThread_;
   mutex mu_;

private:
   void listenThread(ReadCallback);
   void acceptProcess(AcceptStruct);
   ListenServer(const ListenServer&) = delete;

public:
   ListenServer(const string& addr, const string& port);
   ~ListenServer(void)
   {
      stop();
   }

   void start(ReadCallback);
   void stop(void);
   void join(void);
};

#endif
