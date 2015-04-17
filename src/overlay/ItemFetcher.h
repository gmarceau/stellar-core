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
#include "lib/util/lrucache.hpp"

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

namespace std
{
template<>
struct hash<stellar::uint256>
{
    size_t operator()(stellar::uint256 const & x) const noexcept
    {
        size_t res = x[0];
        res = (res << 8) | x[1];
        res = (res << 8) | x[2];
        res = (res << 8) | x[3];
        return res;
    }
};
}

namespace stellar
{


template<class T>
class ItemFetcher : private NonMovableOrCopyable
{
public:
    class Tracker : private NonMovableOrCopyable
    {
        Peer::pointer mLastAskedPeer;
        std::vector<Peer::pointer> mPeersAsked;
        VirtualTimer mTimer;

        std::vector<std::function<void(T item)>> mCallbacks;

    public:
        uint256 mItemID;

        virtual void askPeer(Peer::pointer peer) = 0;
        virtual bool isItemFound() = 0;
        virtual T get() = 0;

        explicit Tracker(uint256 const& id) {}
        virtual ~Tracker() {}

        void doesntHave(Peer::pointer peer);
        void recv(T item);

        void tryNextPeer();
        void cancel();

        void reg(std::function<void(T item)> cb);
    };

    using TrackerPtr = std::shared_ptr<Tracker>;

protected:
    Application& mApp;
    std::map<uint256, std::weak_ptr<Tracker>> mTracking;
    cache::lru_cache<uint256, T> mCache;

    // NB: There are many ItemFetchers in the system at once, but we are sharing
    // a single counter for all the items being fetched by all of them. Be
    // careful, therefore, to only increment and decrement this counter, not set
    // it absolutely.
    medida::Counter& mItemMapSize;


  public:
      
    explicit ItemFetcher(Application& app, size_t cacheSize);

    // Hand the item to `cb` if available in the cache and returns true,
    // otherwise returns false.
    bool get(uint256 hash, std::function<void(T item)> cb);

    // Start fetching the item tracked by `tracker`. Assumes there 
    // is no tracker registered for this item yet (calls `get` first).
    // Immediately registers `cb` with the tracker.
    void fetch(TrackerPtr tracker, std::function<void(T item)> cb);


    void doesntHave(uint256 const& itemID, Peer::pointer peer);
    void recv(uint256 itemID, T item);
};

}


#endif
