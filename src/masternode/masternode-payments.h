// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2020 The HodlCash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODE_PAYMENTS_H
#define MASTERNODE_PAYMENTS_H

#include <key.h>
#include <masternode/masternode.h>
#include <validation.h>

extern RecursiveMutex cs_vecPayments;
extern RecursiveMutex cs_mapMasternodeBlocks;
extern RecursiveMutex cs_mapMasternodePayeeVotes;

class CMasternodePayments;
class CMasternodePaymentWinner;
class CMasternodeBlockPayees;

extern CMasternodePayments masternodePayments;

#define MNPAYMENTS_SIGNATURES_REQUIRED 6
#define MNPAYMENTS_SIGNATURES_TOTAL 10

bool IsBlockPayeeValid(const CBlock& block, int nBlockHeight);
std::string GetRequiredPaymentsString(int nBlockHeight);
bool IsBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted);
void FillBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake, bool fZPIVStake);

class CMasternodePayee {
public:
    CScript scriptPubKey;
    int nVotes;

    CMasternodePayee()
    {
        scriptPubKey = CScript();
        nVotes = 0;
    }

    CMasternodePayee(CScript payee, int nVotesIn)
    {
        scriptPubKey = payee;
        nVotes = nVotesIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(scriptPubKey);
        READWRITE(nVotes);
    }
};

// Keep track of votes for payees from masternodes
class CMasternodeBlockPayees {
public:
    int nBlockHeight;
    std::vector<CMasternodePayee> vecPayments;

    CMasternodeBlockPayees()
    {
        nBlockHeight = 0;
        vecPayments.clear();
    }
    CMasternodeBlockPayees(int nBlockHeightIn)
    {
        nBlockHeight = nBlockHeightIn;
        vecPayments.clear();
    }

    void AddPayee(CScript payeeIn, int nIncrement)
    {
        LOCK(cs_vecPayments);

        for (CMasternodePayee& payee : vecPayments) {
            if (payee.scriptPubKey == payeeIn) {
                payee.nVotes += nIncrement;
                return;
            }
        }

        CMasternodePayee c(payeeIn, nIncrement);
        vecPayments.push_back(c);
    }

    bool GetPayee(CScript& payee)
    {
        LOCK(cs_vecPayments);

        int nVotes = -1;
        for (CMasternodePayee& p : vecPayments) {
            if (p.nVotes > nVotes) {
                payee = p.scriptPubKey;
                nVotes = p.nVotes;
            }
        }

        return (nVotes > -1);
    }

    bool HasPayeeWithVotes(CScript payee, int nVotesReq)
    {
        LOCK(cs_vecPayments);

        for (CMasternodePayee& p : vecPayments) {
            if (p.nVotes >= nVotesReq && p.scriptPubKey == payee)
                return true;
        }

        return false;
    }

    bool IsTransactionValid(const CTransactionRef& txNew);
    std::string GetRequiredPaymentsString();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nBlockHeight);
        READWRITE(vecPayments);
    }
};

// for storing the winning payments
class CMasternodePaymentWinner {
public:
    CTxIn vinMasternode;
    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CMasternodePaymentWinner()
    {
        nBlockHeight = 0;
        vinMasternode = CTxIn();
        payee = CScript();
    }

    CMasternodePaymentWinner(CTxIn vinIn)
    {
        nBlockHeight = 0;
        vinMasternode = vinIn;
        payee = CScript();
    }

    uint256 GetHash() const;
    uint256 GetSignatureHash() { return GetHash(); }
    std::string GetStrMessage() const;
    const CTxIn GetVin() { return vinMasternode; };
    bool IsValid(CNode* pnode, std::string& strError, CConnman& connman);
    void Relay(CConnman& connman);

    void AddPayee(CScript payeeIn)
    {
        payee = payeeIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(vinMasternode);
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vchSig);
    }

    std::string ToString()
    {
        std::string ret = "";
        ret += vinMasternode.ToString();
        ret += ", " + std::to_string(nBlockHeight);
        ret += ", " + payee.ToString();
        ret += ", " + std::to_string((int)vchSig.size());
        return ret;
    }
};

//
// Masternode Payments Class
// Keeps track of who should get paid for which blocks
//

class CMasternodePayments {
private:
    int nSyncedFromPeer;
    int nLastBlockHeight;

public:
    std::map<uint256, CMasternodePaymentWinner> mapMasternodePayeeVotes;
    std::map<int, CMasternodeBlockPayees> mapMasternodeBlocks;
    std::map<COutPoint, int> mapMasternodesLastVote; //prevout, nBlockHeight

    CMasternodePayments()
    {
        nSyncedFromPeer = 0;
        nLastBlockHeight = 0;
    }

    void Clear()
    {
        LOCK2(cs_mapMasternodeBlocks, cs_mapMasternodePayeeVotes);
        mapMasternodeBlocks.clear();
        mapMasternodePayeeVotes.clear();
    }

    bool AddWinningMasternode(CMasternodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight, CConnman& connman);

    void Sync(CNode* node, int nCountNeeded, CConnman& connman);
    void CleanPaymentList();
    int LastPayment(CMasternode& mn);

    bool GetBlockPayee(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransactionRef& txNew, int nBlockHeight);
    bool IsScheduled(CMasternode& mn, int nNotBlockHeight);

    bool CanVote(COutPoint outMasternode, int nBlockHeight)
    {
        LOCK(cs_mapMasternodePayeeVotes);

        if (mapMasternodesLastVote.count(outMasternode)) {
            if (mapMasternodesLastVote[outMasternode] == nBlockHeight) {
                return false;
            }
        }

        //record this masternode voted
        mapMasternodesLastVote[outMasternode] = nBlockHeight;
        return true;
    }

    int GetMinMasternodePaymentsProto();
    void ProcessMessageMasternodePayments(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake, bool fZPIVStake);
    std::string ToString() const;
    int GetOldestBlock();
    int GetNewestBlock();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(mapMasternodePayeeVotes);
        READWRITE(mapMasternodeBlocks);
    }
};

#endif
