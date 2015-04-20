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

/*
Manages asking for Transaction or Quorum sets from Peers
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
class TxSetFrame;
struct SCPQuorumSet;
using TxSetFramePtr = std::shared_ptr<TxSetFrame>;
using SCPQuorumSetPtr = std::shared_ptr<SCPQuorumSet>;



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

protected:
    Application& mApp;
    std::map<uint256, std::weak_ptr<TrackerT>> mTrackers;
    cache::lru_cache<uint256, T> mCache;

    // NB: There are many ItemFetchers in the system at once, but we are sharing
    // a single counter for all the items being fetched by all of them. Be
    // careful, therefore, to only increment and decrement this counter, not set
    // it absolutely.
    medida::Counter& mItemMapSize;


  public:
      
    explicit ItemFetcher(Application& app, size_t cacheSize);

    // Hand the item to `cb` if available in the cache and returns true,
    // else returns false
    bool get(uint256 itemID, std::function<void(T const & item)> cb);

    // Start fetching the item and returns a tracker with `cb` registered 
    // with the tracker). Releasing all shared_ptr references
    // to the tracker will cancel the fetch.
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
    void recv(uint256 itemID, T item);

    optional<Tracker> isNeeded(uint256 itemID);
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

using TxSetTrackerPtr = ItemFetcher<TxSetFrame, TxSetTracker>::TrackerPtr;
using QuorumSetTrackerPtr = ItemFetcher<SCPQuorumSet, QuorumSetTracker>::TrackerPtr;

}

