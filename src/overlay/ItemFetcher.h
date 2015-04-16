#ifndef __ITEMFETCHER__
#define __ITEMFETCHER__

// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <map>
#include <functional>
#include "generated/SCPXDR.h"
#include "overlay/OverlayManager.h"
#include "herder/TxSetFrame.h"
#include "overlay/Peer.h"
#include "util/Timer.h"
#include "util/NonCopyable.h"

/*
Manages asking for Transaction or Quorum sets from Peers

LATER: This abstraction can be cleaned up it is a bit wonky

Asks for TransactionSets from our Peers
We need to get these for SCP.
Anywhere else? If someone asked you you can late reply to them

*/

namespace medida
{
class Counter;
}

namespace stellar
{

template<class T>
class TrackingCollar : private NonMovableOrCopyable
{
    Application& mApp;
    Peer::pointer mLastAskedPeer;
    std::vector<Peer::pointer> mPeersAsked;
    VirtualTimer mTimer;
    int mRefCount;

  protected:
    virtual void askPeer(Peer::pointer peer) = 0;

  public:
    typedef std::shared_ptr<TrackingCollar> pointer;

    uint256 mItemID;

    virtual bool isItemFound() = 0;

    TrackingCollar(uint256 const& id, Application& app);
    virtual ~TrackingCollar();

    void doesntHave(Peer::pointer peer);
    void tryNextPeer();
    void cancelFetch();
    void
    refInc()
    {
        mRefCount++;
    }
    void refDec();
    int
    getRefCount()
    {
        return mRefCount;
    }
};

template<class T>
class ItemFetcher
{
  protected:
    Application& mApp;
    std::map<uint256, typename TrackingCollar<T>::pointer> mItemMap;

    // NB: There are many ItemFetchers in the system at once, but we are sharing
    // a single counter for all the items being fetched by all of them. Be
    // careful, therefore, to only increment and decrement this counter, not set
    // it absolutely.
    medida::Counter& mItemMapSize;

  public:
    ItemFetcher(Application& app);

    void clear();
    void stopFetching(uint256 const& itemID);
    void stopFetchingAll();
    // stop fetching items that verify a condition
    void stopFetchingPred(std::function<bool(uint256 const& itemID)> const& pred);

    void doesntHave(uint256 const& itemID, Peer::pointer peer);
    void recv(uint256 itemID, T item);
};

}

#endif
