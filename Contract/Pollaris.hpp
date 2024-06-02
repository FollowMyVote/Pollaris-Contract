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
//BAL_REFLECT_ENUM(ModificationType, (addRow)(deleteRow)(modifyRow))

// Declare type aliases and name imports
using ContestantIdVariant = std::variant<ContestantId, WriteInId>;
using Tags = std::set<std::string>;
using Opinions = std::map<ContestantIdVariant, int32_t>;

// Declare structs used in the contract interface
struct ContestantDescriptor {
    std::string name;
    std::string description;
    Tags tags;

    bool operator<(const ContestantDescriptor& other) const {
        return std::tie(name, description) < std::tie(other.name, other.description);
    }
    BAL_REFLECT(ContestantDescriptor, (name)(description)(tags))
};

struct FullOpinions {
    map<ContestantId, int32_t> contestantOpinions;
    map<ContestantDescriptor, int32_t> writeInOpinions;
    BAL_REFLECT(FullOpinions, (contestantOpinions)(writeInOpinions))
};

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
    [[eosio::action("tests.pre")]]
    void runPreVotingPeriodTests();
    [[eosio::action("tests.during")]]
    void runDuringVotingPeriodTests();
    [[eosio::action("tests.post")]]
    void runPostVotingPeriodTests();
    [[eosio::action("tests.reset")]]
    void resetTestData();

    [[eosio::action("tests.quick")]]
    void quickTests();

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
                                         DESCRIBE_ACTION("tests.pre"_N, Pollaris::runPreVotingPeriodTests),
                                         DESCRIBE_ACTION("tests.during"_N, Pollaris::runDuringVotingPeriodTests),
                                         DESCRIBE_ACTION("tests.post"_N, Pollaris::runPostVotingPeriodTests),
                                         DESCRIBE_ACTION("tests.reset"_N, Pollaris::resetTestData),
                                         DESCRIBE_ACTION("tests.quick"_N, Pollaris::quickTests)>;

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

        BAL_REFLECT(Pollaris::PollingGroup, (id)(name)(tags))
    };
    using PollingGroups = Table<PollingGroup>;

    struct [[eosio::table("group.accts")]] GroupAccount {
        using Contract = Pollaris;
        constexpr static Name TableName = GROUP_ACCTS;

        AccountHandle account;
        uint32_t weight;
        Tags tags;

        AccountHandle primary_key() const { return account; }

        BAL_REFLECT(Pollaris::GroupAccount, (account)(weight)(tags))
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

        BAL_REFLECT(Pollaris::Contest, (id)(name)(description)(begin)(end)(tags))
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

        BAL_REFLECT(Pollaris::Contestant, (id)(contest)(name)(description)(tags))
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

        BAL_REFLECT(Pollaris::WriteIn, (id)(contest)(name)(description)(refcount)(tags))
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

        BAL_REFLECT(Pollaris::Result, (id)(contest)(timestamp)(tags))
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

        BAL_REFLECT(Pollaris::Tally, (id)(result)(contestant)(tally)(tags))
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

        BAL_REFLECT(Pollaris::Decision, (id)(contest)(voter)(timestamp)(opinions)(tags))
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

        BAL_REFLECT(Pollaris::JournalEntry, (id)(timestamp)(table)(scope)(key)(modification))
    };
    using Journal = Table<JournalEntry>;

    using Tables = Util::TypeList::List<PollingGroups, GroupAccounts, Contests, Contestants, WriteIns, Results, Tallies,
                                        Decisions, Journal>;

private:
    static const PollingGroup* FindGroup(PollingGroups& groups, std::string_view name);
    GroupId FindGroupId(const std::string& groupName,
                        std::string errorMessage = "Polling group was NOT found!");
    void testPollingGroups1();
    void testPollingGroupMembership1();
    void testCreateAndRenameGroup();

    void test1Person1VoteScenario1_Pre();
    void test1Person1VoteScenario1_During();
    void test1Person1VoteScenario1_Post();

    void test1Person1VoteScenario2_Pre();
    void test1Person1VoteScenario2_During();
    void test1Person1VoteScenario2_Post();

    void test1Person3VoteScenario1_Pre();
    void test1Person3VoteScenario1_During();
    void test1Person3VoteScenario1_Post();

    void createDifferentWeightedGroup(const std::string& groupName);
    void testDifferentWeightedVotingScenario_Pre(const std::string& groupName,
                                                 const std::string& contestName,
                                                 const Tags& contestTags = Tags());
    void testDifferentWeightedVotingScenario1_Pre();
    void testDifferentWeightedVotingScenario1_During(const std::string& initialGroupName,
                                                     const std::string& updatedGroupName,
                                                     const std::string& contestName);
    void testDifferentWeightedVotingScenario1_During();
    void testDifferentWeightedVotingScenario1_Post();

    void testDifferentWeightedVotingScenario2_Pre();
    void testDifferentWeightedVotingScenario2_During(const std::string& groupName,
                                                     const std::string& contestName);
    void testDifferentWeightedVotingScenario2_During();
    void testDifferentWeightedVotingScenario2_Post();

    void testDifferentWeightedVotingScenario3_During(const std::string& groupName,
                                                     const std::string& contestName);

    void testContestDeletions1_Pre();
    void testContestDeletions1_During();
    void testContestDeletions1_Post();

    #if 0 // This is disabled until a backend which supports exceptions is available again
    /**
     * Rejection Detection Tests
     */
    void testRejectionDetection();
    void testRejectInvalidContestCreation();

    void testRejectEarlyAndLateVoting_Pre();
    void testRejectEarlyAndLateVoting_Post();

    void testRejectAbstentionProhibition_Pre();
    void testRejectAbstentionProhibition_During();

    void testRejectSplitVoteProhibition_Pre();
    void testRejectSplitVoteProhibition_During();

    void testRejectInvalidVoting_Pre();
    void testRejectInvalidVoting_During();

    void testRejectModifyGroupAndContest_Pre();
    void testRejectModifyGroupAndContest_AfterContestStart();
    void testRejectModifyGroupAndContest_During();
    void testRejectModifyGroupAndContest_Post();
    #endif
};
