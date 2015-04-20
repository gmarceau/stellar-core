// Covpyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "overlay/ItemFetcher.h"
#include "main/Application.h"
#include "overlay/OverlayManager.h"
#include "medida/metrics_registry.h"

#define MS_TO_WAIT_FOR_FETCH_REPLY 500

// TODO.1 I think we need to add something that after some time it retries to
// fetch qsets that it really needs.
// (https://github.com/stellar/stellar-core/issues/81)
/*

*/

namespace stellar
{

template<class T, class TrackerT>
ItemFetcher<T, TrackerT>::ItemFetcher(Application& app, size_t cacheSize) :
    mApp(app)
    , mCache(cacheSize)
    , mItemMapSize(
         app.getMetrics().NewCounter({ "overlay", "memory", "item-fetch-map" }))
{}

template<class T, class TrackerT>
bool
ItemFetcher<T, TrackerT>::get(uint256 hash, std::function<void(T item)> cb)
{
    if (mCache.exists(hash))
    {
        cb(mCache.get(hash));
        return true;
    } else
    {
        return false;
    }

}

template<class T, class TrackerT>
optional<typename ItemFetcher<T, TrackerT>::Tracker> 
ItemFetcher<T, TrackerT>::getOrFetch(uint256 itemID, std::function<void(T item)> cb)
{
    if (get(itemID, cb))
    {
        return nullptr;
    }
    {
        return fetch(itemID, cb);
    }
}

template<class T, class TrackerT>
typename ItemFetcher<T, TrackerT>::TrackerPtr
ItemFetcher<T, TrackerT>::fetch(uint256 itemID, std::function<void(T item)> cb)
{
    auto entry = mTrackers.find(itemID);
    TrackerPtr tracker;

    if (entry == mTrackers.end())
    {
        tracker = std::make_shared<TrackerT>(mApp, itemID, *this);
        tracker->listen(cb);
        mTrackers[itemID] = tracker;
    } else
    {
        tracker = entry->second.lock();
    }

    tracker->tryNextPeer();
    return tracker;
}



template<class T, class TrackerT>
void 
ItemFetcher<T, TrackerT>::doesntHave(uint256 const& itemID, Peer::pointer peer)
{
    if (auto tracker = isNeeded(itemID))
    {
        tracker->doesntHave(peer);
    }
}

template<class T, class TrackerT>
void 
ItemFetcher<T, TrackerT>::recv(uint256 itemID, T item)
{
    if (auto tracker = isNeeded(itemID))
    {
        tracker->recv(item);
    }
}



template<class T, class TrackerT>
optional<typename ItemFetcher<T, TrackerT>::Tracker>
ItemFetcher<T, TrackerT>::isNeeded(uint256 itemID)
{
    auto entry = mTrackers.find(itemID);
    if (entry != mTrackers.end())
    {
        if (auto tracker = entry->second.lock())
        {
            return tracker;
        }
        else
        {
            mTrackers.erase(entry);
            return nullptr;
        }
    }
    return nullptr;
}


template<class T, class TrackerT>
ItemFetcher<T, TrackerT>::Tracker::~Tracker()
{
    cancel();
}


template<class T, class TrackerT>
bool 
ItemFetcher<T, TrackerT>::Tracker::isItemFound()
{
    return mItem != nullptr;
}

template<class T, class TrackerT>
T ItemFetcher<T, TrackerT>::Tracker::get()
{
    return *mItem;
}

template<class T, class TrackerT>
void 
ItemFetcher<T, TrackerT>::Tracker::doesntHave(Peer::pointer peer)
{
    if (mLastAskedPeer == peer)
    {
        tryNextPeer();
    }
}

template<class T, class TrackerT>
void 
ItemFetcher<T, TrackerT>::Tracker::recv(T item)
{
    mItem = make_optional<T>(item);
    for(auto cb : mCallbacks)
    {
        cb(item);
    }
    cancel();
}

template<class T, class TrackerT>
void 
ItemFetcher<T, TrackerT>::Tracker::tryNextPeer()
{
    // will be called by some timer or when we get a 
    // response saying they don't have it

    if (!isItemFound() && mItemFetcher.isNeeded(mItemID))
    { // we still haven't found this item, and someone still wants it
        Peer::pointer peer;

        if (mPeersAsked.size())
        {
            while (!peer && mPeersAsked.size())
            { // keep looping till we find a peer
                // we are still connected to
                peer = mApp.getOverlayManager().getNextPeer(
                    mPeersAsked[mPeersAsked.size() - 1]);
                if (!peer)
                    mPeersAsked.pop_back();
            }
        }
        else
        {
            peer = mApp.getOverlayManager().getRandomPeer();
        }

        if (peer)
        {
            if (find(mPeersAsked.begin(), mPeersAsked.end(), peer) ==
                mPeersAsked.end())
            { // we have never asked this guy
                askPeer(peer);

                mLastAskedPeer = peer;
                mPeersAsked.push_back(peer);
            }

            mTimer.cancel(); // cancel any stray timers
            mTimer.expires_from_now(
                std::chrono::milliseconds(MS_TO_WAIT_FOR_FETCH_REPLY));
            mTimer.async_wait([this]()
            {
                this->tryNextPeer();
            }, VirtualTimer::onFailureNoop);
        }
        else
        { // we have asked all our peers
            // clear list and try again in a bit
            mPeersAsked.clear();
            mTimer.cancel(); // cancel any stray timers
            mTimer.expires_from_now(
                std::chrono::milliseconds(MS_TO_WAIT_FOR_FETCH_REPLY * 2));
            mTimer.async_wait([this]()
            {
                this->tryNextPeer();
            }, VirtualTimer::onFailureNoop);
        }
    }
}

template<class T, class TrackerT>
void ItemFetcher<T, TrackerT>::Tracker::cancel()
{
    mCallbacks.clear();
    mPeersAsked.clear();
    mTimer.cancel();
    mLastAskedPeer = nullptr;
}

template<class T, class TrackerT>
void ItemFetcher<T, TrackerT>::Tracker::listen(std::function<void(T item)> cb)
{
    mCallbacks.push_back(cb);
}


void TxSetTracker::askPeer(Peer::pointer peer)
{
    peer->sendGetTxSet(mItemID);
}

void QuorumSetTracker::askPeer(Peer::pointer peer)
{
    peer->sendGetQuorumSet(mItemID);
}

template ItemFetcher<TxSetFramePtr, TxSetTracker>;
template ItemFetcher<SCPQuorumSetPtr, QuorumSetTracker>;


}
