#include <BAL/FcSerialization.hpp>
#include <BAL/BAL.hpp>
#include <BAL/IndexHelpers.hpp>
#include <BAL/Table.hpp>
#include <BAL/Name.hpp>
#include <BAL/Reflect.hpp>

#include <Utils/StaticVariant.hpp>

#include <cstdint>
#include <limits>
#include <set>
#include <map>
#include <optional>

// Name imports
using BAL::Name;
using BAL::AccountName;
using BAL::AccountHandle;
using BAL::Timestamp;
using BAL::Table;
using BAL::SecondaryIndex;
using BAL::SecondaryIndexes_s;
using BAL::MakeStringKey;
using BAL::MakeCompositeKey;
using BAL::UInt128;
using BAL::UInt256;
using std::string;
using std::vector;
using std::set;
using std::map;
using std::optional;
using Util::StaticVariant;

// Define name constants
constexpr Name POLL_GROUPS = "poll.groups"_N;
constexpr Name GROUP_ACCTS = "group.accts"_N;
constexpr Name CONTESTS = "contests"_N;
constexpr Name CONTESTANTS = "contestants"_N;
constexpr Name WRITE_INS = "write.ins"_N;
constexpr Name RESULTS = "results"_N;
constexpr Name TALLIES = "tallies"_N;
constexpr Name DECISIONS = "decisions"_N;
constexpr Name JOURNAL = "journal"_N;

constexpr Name BY_NAME = "by.name"_N;
constexpr Name BY_CONTEST = "by.contest"_N;
constexpr Name BY_RESULT = "by.result"_N;
constexpr Name BY_VOTER = "by.voter"_N;
constexpr Name BY_TIMESTAMP = "by.timestamp"_N;

constexpr Name GLOBAL = "global"_N;

// Declare ID types
using GroupId = BAL::ID<BAL::NumberTag<POLL_GROUPS.value>>;
using ContestId = BAL::ID<BAL::NumberTag<CONTESTS.value>>;
using ContestantId = BAL::ID<BAL::NumberTag<CONTESTANTS.value>>;
using WriteInId = BAL::ID<BAL::NumberTag<WRITE_INS.value>>;
using ResultId = BAL::ID<BAL::NumberTag<RESULTS.value>>;
using TallyId = BAL::ID<BAL::NumberTag<TALLIES.value>>;
using DecisionId = BAL::ID<BAL::NumberTag<DECISIONS.value>>;
using JournalId = BAL::ID<BAL::NumberTag<JOURNAL.value>>;

// Declare enum types
enum class ModificationType : uint8_t {
    addRow,
    deleteRow,
    modifyRow
};
BAL_REFLECT_ENUM(ModificationType, (addRow)(deleteRow)(modifyRow))

// Declare type aliases and name imports
using ContestantIdVariant = std::variant<ContestantId, WriteInId>;
BAL_REFLECT_TYPENAME(ContestantIdVariant)
using Tags = std::set<std::string>;
BAL_REFLECT_TYPENAME(Tags)
using Opinions = std::map<ContestantIdVariant, int32_t>;
BAL_REFLECT_TYPENAME(Opinions)

// Declare structs used in the contract interface
struct ContestantDescriptor {
    std::string name;
    std::string description;
    Tags tags;

    bool operator<(const ContestantDescriptor& other) const {
        return std::tie(name, description) < std::tie(other.name, other.description);
    }
};
BAL_REFLECT(ContestantDescriptor, (name)(description)(tags))

struct FullOpinions {
    map<ContestantId, int32_t> contestantOpinions;
    map<ContestantDescriptor, int32_t> writeInOpinions;
};
BAL_REFLECT(FullOpinions, (contestantOpinions)(writeInOpinions))

// Define the contest tags
const string NO_SPLIT_TAG = "no-split-vote";
const string NO_ABSTAIN_TAG = "no-abstain";
// Define the decision tags
const string ABSTAIN_VOTE_TAG = "abstain";
const string PARTIAL_ABSTAIN_VOTE_TAG_PREFIX = "partial-abstain:";

// Declare contract class
class [[eosio::contract("pollaris")]] Pollaris : public BAL::Contract {
public:
    using BAL::Contract::Contract;

    // Declare contract actions
    [[eosio::action("voter.add")]]
    void addVoter(string groupName, AccountHandle voter, uint32_t weight, Tags tags);
    [[eosio::action("voter.remove")]]
    void removeVoter(string groupName, AccountHandle voter);
    [[eosio::action("group.copy")]]
    void copyGroup(string groupName, string newName);
    [[eosio::action("group.rename")]]
    void renameGroup(string groupName, string newName);
    [[eosio::action("cntst.new")]]
    void newContest(GroupId groupId, string name, string description,
                    set<ContestantDescriptor> contestants, Timestamp begin,
                    Timestamp end, Tags tags);
    [[eosio::action("cntst.modify")]]
    void modifyContest(GroupId groupId, ContestId contestId,
                       optional<string> newName, optional<string> newDescription,
                       optional<Tags> newTags,
                       set<ContestantId> deleteContestants,
                       set<ContestantDescriptor> addContestants,
                       optional<Timestamp> newBegin, optional<Timestamp> newEnd);
    [[eosio::action("cntst.delete")]]
    void deleteContest(GroupId groupId, ContestId contestId);
    [[eosio::action("cntst.tally")]]
    void tallyContest(GroupId groupId, ContestId contestId);
    [[eosio::action("dcsn.set")]]
    void setDecision(GroupId groupId, ContestId contestId, AccountHandle voterName,
                     FullOpinions opinions, Tags tags);
    [[eosio::action("tests.run")]]
    void runTests();

    // Describe contract actions
    using Actions = Util::TypeList::List<DESCRIBE_ACTION("voter.add"_N, Pollaris::addVoter),
                                         DESCRIBE_ACTION("voter.remove"_N, Pollaris::removeVoter),
                                         DESCRIBE_ACTION("group.copy"_N, Pollaris::copyGroup),
                                         DESCRIBE_ACTION("group.rename"_N, Pollaris::renameGroup),
                                         DESCRIBE_ACTION("cntst.new"_N, Pollaris::newContest),
                                         DESCRIBE_ACTION("cntst.modify"_N, Pollaris::modifyContest),
                                         DESCRIBE_ACTION("cntst.delete"_N, Pollaris::deleteContest),
                                         DESCRIBE_ACTION("cntst.tally"_N, Pollaris::tallyContest),
                                         DESCRIBE_ACTION("dcsn.set"_N, Pollaris::setDecision),
                                         DESCRIBE_ACTION("tests.run"_N, Pollaris::runTests)>;

    // Declare tables and indexes
    struct [[eosio::table("poll.groups")]] PollingGroup {
        using Contract = Pollaris;
        constexpr static Name TableName = POLL_GROUPS;

        GroupId id;
        string name;
        Tags tags;

        GroupId primary_key() const { return id; }
        UInt256 nameKey() const { return MakeStringKey(name); }

        using SecondaryIndexes =
            Util::TypeList::List<SecondaryIndex<BY_NAME, PollingGroup, UInt256, &PollingGroup::nameKey>>;
    };
    using PollingGroups = Table<PollingGroup>;

    struct [[eosio::table("group.accts")]] GroupAccount {
        using Contract = Pollaris;
        constexpr static Name TableName = GROUP_ACCTS;

        AccountHandle account;
        uint32_t weight;
        Tags tags;

        AccountHandle primary_key() const { return account; }
    };
    using GroupAccounts = Table<GroupAccount>;

    struct [[eosio::table("contests")]] Contest {
        using Contract = Pollaris;
        constexpr static Name TableName = CONTESTS;

        ContestId id;
        string name;
        string description;
        Timestamp begin;
        Timestamp end;
        Tags tags;

        ContestId primary_key() const { return id; }
    };
    using Contests = Table<Contest>;

    struct [[eosio::table("contestants")]] Contestant {
        using Contract = Pollaris;
        constexpr static Name TableName = CONTESTANTS;

        ContestantId id;
        ContestId contest;
        string name;
        string description;
        Tags tags;

        ContestantId primary_key() const { return id; }
        UInt128 contestKey() const { return MakeCompositeKey(contest, id); }

        static UInt128 contestKeyMin(ContestId contest = ContestId{0ull},
                                       ContestantId contestant = ContestantId{0ull}) {
            return MakeCompositeKey(contest, contestant);
        }
        static UInt128 contestKeyMax(ContestId contest = ContestId{~0ull},
                                       ContestantId contestant = ContestantId{~0ull}) {
            return MakeCompositeKey(contest, contestant);
        }

        using SecondaryIndexes =
            Util::TypeList::List<SecondaryIndex<BY_CONTEST, Contestant, UInt128, &Contestant::contestKey>>;
    };
    using Contestants = Table<Contestant>;

    struct [[eosio::table("write.ins")]] WriteIn {
        using Contract = Pollaris;
        constexpr static Name TableName = WRITE_INS;

        WriteInId id;
        ContestId contest;
        string name;
        string description;
        int16_t refcount = 0;
        Tags tags;

        WriteInId primary_key() const { return id; }
        UInt128 contestKey() const { return MakeCompositeKey(contest, id); }

        static UInt128 contestKeyMin(ContestId contest = ContestId{0ull}, WriteInId writeIn = WriteInId{0ull}) {
            return MakeCompositeKey(contest, writeIn);
        }
        static UInt128 contestKeyMax(ContestId contest = ContestId{~0ull}, WriteInId writeIn = WriteInId{~0ull}) {
            return MakeCompositeKey(contest, writeIn);
        }

        using SecondaryIndexes =
            Util::TypeList::List<SecondaryIndex<BY_CONTEST, WriteIn, UInt128, &WriteIn::contestKey>>;
    };
    using WriteIns = Table<WriteIn>;

    struct [[eosio::table("results")]] Result {
        using Contract = Pollaris;
        constexpr static Name TableName = RESULTS;

        ResultId id;
        ContestId contest;
        Timestamp timestamp;
        Tags tags;

        ResultId primary_key() const { return id; }
        UInt128 contestKey() const { return MakeCompositeKey(contest, ~timestamp.sec_since_epoch()); }

        static UInt128 contestKeyMin(ContestId contest = ContestId{0ull}, Timestamp timestamp = Timestamp(0u)) {
            return MakeCompositeKey(contest, timestamp.sec_since_epoch());
        }
        static UInt128 contestKeyMax(ContestId contest = ContestId{~0ull}, Timestamp timestamp = Timestamp(~0u)) {
            return MakeCompositeKey(contest, timestamp.sec_since_epoch());
        }

        using SecondaryIndexes =
            Util::TypeList::List<SecondaryIndex<BY_CONTEST, Result, UInt128, &Result::contestKey>>;
    };
    using Results = Table<Result>;

    struct [[eosio::table("tallies")]] Tally {
        using Contract = Pollaris;
        constexpr static Name TableName = TALLIES;

        TallyId id;
        ResultId result;
        ContestantIdVariant contestant;
        uint64_t tally;
        Tags tags;

        TallyId primary_key() const { return id; }
        UInt256 resultKey() const { return MakeCompositeKey(result, ~tally, BAL::Decompose(contestant)); }

        static UInt256 resultKeyMin(ResultId result = ResultId{0ull}, uint64_t tally = 0ull,
                                        ContestantIdVariant contestant = BAL::Decompose_MIN<ContestantIdVariant>()) {
            return MakeCompositeKey(result, tally, BAL::Decompose(contestant));
        }
        static UInt256 resultKeyMax(ResultId result = ResultId{~0ull}, uint64_t tally = ~0ull,
                                        ContestantIdVariant contestant = BAL::Decompose_MAX<ContestantIdVariant>()) {
            return MakeCompositeKey(result, tally, BAL::Decompose(contestant));
        }

        using SecondaryIndexes = Util::TypeList::List<SecondaryIndex<BY_RESULT, Tally, UInt256, &Tally::resultKey>>;
    };
    using Tallies = Table<Tally>;

    struct [[eosio::table("decisions")]] Decision {
        using Contract = Pollaris;
        constexpr static Name TableName = DECISIONS;

        DecisionId id;
        ContestId contest;
        AccountHandle voter;
        Timestamp timestamp;
        Opinions opinions;
        Tags tags;

        DecisionId primary_key() const { return id; }
        UInt128 contestKey() const { return MakeCompositeKey(contest, voter); }
        UInt128 voterKey() const { return MakeCompositeKey(voter, ~timestamp.sec_since_epoch()); }

        static UInt128 contestKeyMin(ContestId contest = ContestId{0ull}, AccountHandle voter = std::numeric_limits<AccountHandle>::min()) {
            return MakeCompositeKey(contest, voter);
        }
        static UInt128 contestKeyMax(ContestId contest = ContestId{~0ull}, AccountHandle voter = std::numeric_limits<AccountHandle>::max()) {
            return MakeCompositeKey(contest, voter);
        }

        static UInt128 voterKeyMin(AccountHandle voter = std::numeric_limits<AccountHandle>::min(), Timestamp timestamp = Timestamp(0u)) {
            return MakeCompositeKey(voter, timestamp.sec_since_epoch());
        }
        static UInt128 voterKeyMax(AccountHandle voter = std::numeric_limits<AccountHandle>::max(), Timestamp timestamp = Timestamp(~0u)) {
            return MakeCompositeKey(voter, timestamp.sec_since_epoch());
        }

        using SecondaryIndexes =
            Util::TypeList::List<SecondaryIndex<BY_CONTEST, Decision, UInt128, &Decision::contestKey>,
                                 SecondaryIndex<BY_VOTER, Decision, UInt128, &Decision::voterKey>>;
    };
    using Decisions = Table<Decision>;

    struct [[eosio::table("journal")]] JournalEntry {
        using Contract = Pollaris;
        constexpr static Name TableName = JOURNAL;

        JournalId id;
        Timestamp timestamp;
        Name table;
        uint64_t scope;
        uint64_t key;
        ModificationType modification;

        JournalId primary_key() const { return id; }
        uint64_t timestampKey() const { return timestamp.sec_since_epoch(); }

        using SecondaryIndexes =
            Util::TypeList::List<SecondaryIndex<BY_TIMESTAMP, JournalEntry, uint64_t, &JournalEntry::timestampKey>>;
    };
    using Journal = Table<JournalEntry>;

    using Tables = Util::TypeList::List<PollingGroups, GroupAccounts, Contests, Contestants, WriteIns, Results, Tallies,
                                        Decisions, Journal>;
};

// Reflect the tables
BAL_REFLECT(Pollaris::PollingGroup, (id)(name)(tags))
BAL_REFLECT(Pollaris::GroupAccount, (account)(weight)(tags))
BAL_REFLECT(Pollaris::Contest, (id)(name)(description)(begin)(end)(tags))
BAL_REFLECT(Pollaris::Contestant, (id)(contest)(name)(description)(tags))
BAL_REFLECT(Pollaris::WriteIn, (id)(contest)(name)(description)(refcount)(tags))
BAL_REFLECT(Pollaris::Result, (id)(contest)(timestamp)(tags))
BAL_REFLECT(Pollaris::Tally, (id)(result)(contestant)(tally)(tags))
BAL_REFLECT(Pollaris::Decision, (id)(contest)(voter)(timestamp)(opinions)(tags))
BAL_REFLECT(Pollaris::JournalEntry, (id)(timestamp)(table)(scope)(key)(modification))
