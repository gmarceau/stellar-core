#pragma once

// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <map>
#include <functional>
#include "generated/SCPXDR.h"
#include "overlay/Peer.h"
#include "util/Timer.h"
#include "util/NonCopyable.h"
#include "lib/util/lrucache.hpp"
#include <util/optional.h>
#include "util/HashOfHash.h"

/*
Manages asking for Transaction or Quorum sets from Peers
*/

namespace medida
{
class Counter;
}

namespace stellar
{
class TxSetFrame;
struct SCPQuorumSet;
using TxSetFramePtr = std::shared_ptr<TxSetFrame>;
using SCPQuorumSetPtr = std::shared_ptr<SCPQuorumSet>;

static std::chrono::milliseconds const MS_TO_WAIT_FOR_FETCH_REPLY{ 500 };

template<class T, class TrackerT>
class ItemFetcher : private NonMovableOrCopyable
{
public:
    class Tracker : private NonMovableOrCopyable
    {
        Application &mApp;
        ItemFetcher &mItemFetcher;
        Peer::pointer mLastAskedPeer;
        std::vector<Peer::pointer> mPeersAsked;
        VirtualTimer mTimer;
        optional<T> mItem;

        std::vector<std::function<void(T item)>> mCallbacks;

    public:
        uint256 mItemID;
        explicit Tracker(Application &app, uint256 const& id, ItemFetcher &itemFetcher) : 
            mApp(app)
          , mItemFetcher(itemFetcher)
          , mTimer(app)
          , mItemID(id) {}
        virtual ~Tracker();

        bool isItemFound();
        T get();
        void cancel();
        void listen(std::function<void(T const &item)> cb);

        virtual void askPeer(Peer::pointer peer) = 0;

        void doesntHave(Peer::pointer peer);
        void recv(T item);
        void tryNextPeer();
    };

    using TrackerPtr = std::shared_ptr<TrackerT>;

      
    explicit ItemFetcher(Application& app, size_t cacheSize);

    // Return the item if available in the cache, else returns nullopt.
    optional<T> get(uint256 itemID);

    // Start fetching the item and returns a tracker with `cb` registered 
    // with the tracker). Releasing all shared_ptr references
    // to the tracker will cancel the fetch.
    // 
    // The fetch might complete immediately if the item is in the cache.
    //
    // The callback must be idempoten and without dependencies on allocated
    // memory, as it can be called anytime in the future between now and a 
    // call `cancel`, including much later if there are other references to this
    // tracker.
    TrackerPtr fetch(uint256 itemID, std::function<void(T const & item)> cb);


    // Hands to item immediately to `cb` if available in cache and returns `nullptr`,
    // else starts fetching the item and returns a tracker.
    TrackerPtr getOrFetch(uint256 itemID, std::function<void(T const & item)> cb);

    void doesntHave(uint256 const& itemID, Peer::pointer peer);

    // recv: notifies all listeners of the arrival of the item and caches it if 
    // it was needed
    void recv(uint256 itemID, T const & item);

    // cache: notifies all listeneers of the arrival of the item and caches it 
    // unconditionaly
    void cache(Hash itemID, T const & item);

    optional<Tracker> isNeeded(uint256 itemID);

protected:
    Application& mApp;
    std::map<uint256, std::weak_ptr<TrackerT>> mTrackers;
    cache::lru_cache<uint256, T> mCache;

    // NB: There are many ItemFetchers in the system at once, but we are sharing
    // a single counter for all the items being fetched by all of them. Be
    // careful, therefore, to only increment and decrement this counter, not set
    // it absolutely.
    medida::Counter& mItemMapSize;

};

class TxSetTracker : public ItemFetcher<TxSetFrame, TxSetTracker>::Tracker
{
public:
    TxSetTracker(Application &app, uint256 id, ItemFetcher<TxSetFrame, TxSetTracker> &itemFetcher) :
        Tracker(app, id, itemFetcher) {}

    void askPeer(Peer::pointer peer) override;
};

class QuorumSetTracker : public ItemFetcher<SCPQuorumSet, QuorumSetTracker>::Tracker
{
public:
    QuorumSetTracker(Application &app, uint256 id, ItemFetcher<SCPQuorumSet, QuorumSetTracker> &itemFetcher) :
        Tracker(app, id, itemFetcher) {}

    void askPeer(Peer::pointer peer) override;
};


class IntTracker : public ItemFetcher<int, IntTracker>::Tracker
{
public:
    std::vector<Peer::pointer> mAsked;
    IntTracker(Application &app, uint256 id, ItemFetcher<int, IntTracker> &itemFetcher) :
        Tracker(app, id, itemFetcher) {}

    void askPeer(Peer::pointer peer) override;
};

using TxSetTrackerPtr = ItemFetcher<TxSetFrame, TxSetTracker>::TrackerPtr;
using QuorumSetTrackerPtr = ItemFetcher<SCPQuorumSet, QuorumSetTracker>::TrackerPtr;
using IntTrackerPtr = ItemFetcher<int, IntTracker>::TrackerPtr;

}

