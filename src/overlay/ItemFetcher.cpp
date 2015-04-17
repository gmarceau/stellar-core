// Covpyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "overlay/ItemFetcher.h"
#include "crypto/SHA.h"
#include "main/Application.h"
#include "overlay/OverlayManager.h"
#include "util/Logging.h"

#include "medida/counter.h"
#include "medida/metrics_registry.h"
#include "xdrpp/marshal.h"

#define MS_TO_WAIT_FOR_FETCH_REPLY 500

// TODO.1 I think we need to add something that after some time it retries to
// fetch qsets that it really needs.
// (https://github.com/stellar/stellar-core/issues/81)
/*

*/

namespace stellar
{

template<class T>
ItemFetcher<T>::ItemFetcher(Application& app, size_t cacheSize) : 
    mApp(app), mCache(cacheSize)
{}

template <class T>
bool 
ItemFetcher<T>::get(uint256 hash, std::function<void(T item)> cb)
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

template <class T>
void 
ItemFetcher<T>::fetch(TrackerPtr tracker, std::function<void(T item)> cb)
{
    assert(mTracking.find(tracker->mItemID) == mTracking.end());


    // TODO: flush expired weak_ptrs

    mTracking[tracker->mItemID] = tracker;
    tracker->tryNextPeer();
    tracker->reg(cb);
}

template <class T>
void ItemFetcher<T>::doesntHave(uint256 const& itemID, Peer::pointer peer)
{
    auto entry = mTracking.find(itemID);
    if (entry != mTracking.end())
    {
        if (auto tracker = entry->second.lock())
        {
            tracker->doesntHave(itemID);
        }
        else
        {
            mTracking.erase(entry);
        }
    }
}

template <class T>
void ItemFetcher<T>::recv(uint256 itemID, T item)
{
    auto entry = mTracking.find(itemID);
    if (entry != mTracking.end())
    {
        if (auto tracker = entry->second.lock())
        {
            tracker->recv(itemID);
        } else
        {
            mTracking.erase(entry);
        }
    }
}

template <class T>
void ItemFetcher<T>::Tracker::doesntHave(Peer::pointer peer)
{
    if (mLastAskedPeer == peer)
    {
        tryNextPeer();
    }
}

template <class T>
void ItemFetcher<T>::Tracker::recv(T item)
{

}


    /*


ItemFetcher::ItemFetcher(Application& app)
     mItemMapSize(
          app.getMetrics().NewCounter({"overlay", "memory", "item-fetch-map"}))
{
}



// TODO.1: This all needs to change
// returns true if we were waiting for this txSet
bool
TxSetFetcher::recvItem(TxSetFramePtr txSet)
{
    if (txSet)
    {
        auto result = mItemMap.find(txSet->getContentsHash());
        if (result != mItemMap.end())
        {
            int refCount = result->second->getRefCount();
            result->second->cancelFetch();
            ((TxSetTrackingCollar*)result->second.get())->mTxSet = txSet;
            if (refCount)
            { // someone was still interested in
                // this tx set so tell SCP
                // LATER: maybe change this to pub/sub
                return true;
            }
        }
        else
        { // doesn't seem like we were looking for it. Maybe just add it for
            // now
            mItemMap[txSet->getContentsHash()] =
                std::make_shared<TxSetTrackingCollar>(txSet->getContentsHash(),
                                                      txSet, mApp);
            mItemMapSize.inc();
            
    }
    return false;
}


void
TrackingCollar::refDec()
{
    mRefCount--;
    if (mRefCount < 1)
        cancelFetch();
}

void
TrackingCollar::cancelFetch()
{
    mRefCount = 0;
    mTimer.cancel();
}

// will be called by some timer or when we get a result saying they don't have
// it
void
TrackingCollar::tryNextPeer()
{
    if (!isItemFound())
    { // we still haven't found this item
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
                mLastAskedPeer = peer;

                askPeer(peer);
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

QSetTrackingCollar::QSetTrackingCollar(uint256 const& id, SCPQuorumSetPtr qSet,
                                       Application& app)
    : Tracker(id, app), mQSet(qSet)
{
}

void
QSetTrackingCollar::askPeer(Peer::pointer peer)
{
    peer->sendGetQuorumSet(mItemID);
}

TxSetTrackingCollar::TxSetTrackingCollar(uint256 const& id, TxSetFramePtr txSet,
                                         Application& app)
    : Tracker(id, app), mTxSet(txSet)
{
}

void
TxSetTrackingCollar::askPeer(Peer::pointer peer)
{
    peer->sendGetTxSet(mItemID);
}

*/

}
