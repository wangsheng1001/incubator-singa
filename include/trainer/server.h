#ifndef INCLUDE_TRAINER_SERVER_H_
#define INCLUDE_TRAINER_SERVER_H_
#include <memory>
#include <utils/param.h>
#include <utils/updater.h>
#include "proto/model.pb.h"
#include "communication/socket.h"

using std::shared_ptr;
namespace singa {
/* Repsond to worker's get/put/udpate request, and periodically syncing with
  * other servers.
  *
  * Normally, the Server creates a response message for each request which
  * will be sent back to the one who issued the request. However, if the request
  * are not processed successfully, the original message will be returned. The
  * sever does not know the returned message (response or the original message),
  * it just sends it to the router. The router will decide to re-send the
  * request to the server or send it to the worker.
  */
class Server{
 public:
  typedef std::map<int, shared_ptr<Param>> ParamShard;

  Server(int thread_id, int group_id, int server_id);
  void Setup(const UpdaterProto& proto, shared_ptr<ParamShard> shard);
  void Run();

 protected:

 	/**
	 * Process GET request.
   *
   * @return the orignal message or response message
   */
	virtual Msg* HandleGet(shared_ptr<Param> param, Msg** msg);

	/**
	 * Process Update request.
   *
   * @return the orignal message or response message
   */
	virtual Msg* HandleUpdate(shared_ptr<Param> param, Msg** msg);

	/**
	 * Process PUT request.
   *
   * @return the original message or response message. If we don't want need to
   * acknowledge the put request, then return nullptr.
	 */
	virtual Msg* HandlePut(shared_ptr<Param> param, Msg **msg);

	/**
   * TODO Process SYNC request.
	 */
	virtual Msg* HandleSyncRequest(shared_ptr<Param> param, Msg** msg);

	/**
   * TODO Process SYNC response.
	 */
	virtual int HandleSyncResponse(shared_ptr<Param> param, Msg** msg);

  /**
   * Scheduler for synchronizing server groups.
   *
   * TODO implement the Caffe's synchronization scheduler for data parallelism
   */
  virtual bool SyncNow();
 
public:
  /*
   * Dump all params to a binary proto file
   */
  static void Checkpoint(const std::string path, const std::map<int, shared_ptr<Param>> params);

  /*
   * Restore params from a binary proto file
   */
  static void Restore(const std::string path, std::map<int, shared_ptr<Param>>& params);

 protected:
  int thread_id_,group_id_, server_id_;
  shared_ptr<Dealer> dealer_;
  shared_ptr<Updater> updater_;
  shared_ptr<ParamShard> shard_;
};
} /* Server */
#endif //INCLUDE_TRAINER_SERVER_H_
