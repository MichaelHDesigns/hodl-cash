// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2020 The HodlCash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODEMAN_H
#define MASTERNODEMAN_H

#include <base58.h>
#include <key.h>
#include <masternode/activemasternode.h>
#include <masternode/masternode.h>
#include <net.h>
#include <sync.h>
#include <util/system.h>
#include <validation.h>

#define MASTERNODES_DUMP_SECONDS (15 * 60)
#define MASTERNODES_DSEG_SECONDS (3 * 60 * 60)

class CMasternodeMan;
class CActiveMasternode;

extern CMasternodeMan mnodeman;
extern CActiveMasternode activeMasternode;

class CMasternodeMan {
private:
    // critical section to protect the inner data structures
    mutable RecursiveMutex cs;

    // critical section to protect the inner data structures specifically on messaging
    mutable RecursiveMutex cs_process_message;

    // map to hold all MNs
    std::vector<CMasternode> vMasternodes;
    // who's asked for the Masternode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForMasternodeList;
    // who we asked for the Masternode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForMasternodeList;
    // which Masternodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForMasternodeListEntry;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, CMasternodeBroadcast> mapSeenMasternodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CMasternodePing> mapSeenMasternodePing;

    // keep track of dsq count to prevent masternodes from gaming obfuscation queue
    // TODO: Remove this from serialization
    int64_t nDsqCount;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        LOCK(cs);
        READWRITE(vMasternodes);
        READWRITE(mAskedUsForMasternodeList);
        READWRITE(mWeAskedForMasternodeList);
        READWRITE(mWeAskedForMasternodeListEntry);
        READWRITE(nDsqCount);

        READWRITE(mapSeenMasternodeBroadcast);
        READWRITE(mapSeenMasternodePing);
    }

    CMasternodeMan();
    CMasternodeMan(CMasternodeMan& other);

    /// Add an entry
    bool Add(CMasternode& mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode* pnode, CTxIn& outpoint, CConnman& connman);

    /// Check all Masternodes
    void Check();

    /// Check all Masternodes and remove inactive
    void CheckAndRemove(bool forceExpiredRemoval = false);

    /// Clear Masternode vector
    void Clear();

    int CountEnabled(int protocolVersion = -1);

    void CountNetworks(int protocolVersion, int& ipv4, int& ipv6, int& onion);

    void DsegUpdate(CNode* pnode, CConnman& connman);

    /// Find an entry
    CMasternode* Find(const CScript& payee);
    CMasternode* Find(const CTxIn& vin);
    CMasternode* Find(const CPubKey& pubKeyMasternode);

    /// Find an entry in the masternode list that is next to be paid
    CMasternode* GetNextMasternodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CMasternode* FindRandomNotInVec(std::vector<CTxIn>& vecToExclude, int protocolVersion = -1);

    /// Get the current winner for this block
    CMasternode* GetCurrentMasterNode(int mod = 1, int64_t nBlockHeight = 0, int minProtocol = 0);

    std::vector<CMasternode> GetFullMasternodeVector()
    {
        Check();
        return vMasternodes;
    }

    std::vector<std::pair<int, CMasternode>> GetMasternodeRanks(int64_t nBlockHeight, int minProtocol = 0);
    int GetMasternodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);
    CMasternode* GetMasternodeByRank(int nRank, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);

    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);
    void ProcessMasternodeConnections(CConnman& connman);

    /// Return the number of (unique) Masternodes
    int size() { return vMasternodes.size(); }

    /// Return the number of Masternodes older than (default) 8000 seconds
    int stable_size();

    std::string ToString() const;

    void Remove(CTxIn vin);

    int GetEstimatedMasternodes(int nBlock);

    /// Update masternode list and maps using provided CMasternodeBroadcast
    void UpdateMasternodeList(CMasternodeBroadcast mnb, CConnman& connman);
};

#endif
