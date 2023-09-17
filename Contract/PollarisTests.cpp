#include "Pollaris.hpp"

/**
 * The range of possible tests of the Pollaris smart contract can be split into
 * two categories: single-stage and multiple-stage (multi-stage) tests.
 *
 * Single-stage tests are those tests which do not require perceived time to elapse.
 * These tests can verify particular functionalities at any single moment.
 *
 * Muliple-stage tests require time to elapse for proper validation.
 * These tests involve initialization of some state (e.g. through the creation of a contest)
 * that permit subsequent actions (e.g. voting during the contest voting period)
 * to be executed and validated later in time.
 *
 * The testing scheme used below consists of four stages.
 *
 * 1. Pre voting-period tests
 * 2. During voting period tests
 * 3. Post voting-period tests
 * 4. (Implicit) Data reset
 *
 * Pre-voting-period tests include both single-stage tests, and the pre-voting-period stage
 * that initialize the state for the tests require time to elapse.  These tests will typically
 * involve the creation of future contests.
 *
 * During voting-period tests include actions and validations that will be performed
 * during a contest's voting period.  These tests typically involve voting.
 *
 * Post-voting-period tests include the validation of voting after a contest's voting period.
 * These tests typically involve tallying final results.
 *
 * An implicit stage is the deletion of all residual test data (`resetTestData()`)
 * still present after any of the other three stages.
 * Whereas single-stage tests would be expected to clean up after themselves, multi-stage
 * tests persist data within the smart contract for use by later stages and for
 * auditing purposes by off-contract agents or persons.  When the data is no longer required,
 * as would be the case prior to the start of a new testing cycle, the test data can and should
 * be deleted.
 *
 * During any test cycle, it is expected that the pre, during, and post stages will only be
 * run once, and that they will be run in sequence: first "pre" then "during" then "post".
 * At the conclusion of a test cycle, or prior to the start of a new test cycle, the "data reset"
 * stage should be triggered.
 */

/**
 * Generally empty, this provide a quick and clean way to run one-off tests during development
 */
void Pollaris::quickTests() {
    BAL::Log("Begin quick tests");

    BAL::Log("Quick tests PASSED");
}

/**
 * Run the pre-voting-period tests
 */
void Pollaris::runPreVotingPeriodTests() {
    // Single-stage tests
    testPollingGroups1();
    testPollingGroupMembership1();
    testCreateAndRenameGroup();

    // Multi-stage tests
    test1Person1VoteScenario1_Pre();
    test1Person1VoteScenario2_Pre();
    test1Person3VoteScenario1_Pre();
    testDifferentWeightedVotingScenario1_Pre();
    testDifferentWeightedVotingScenario2_Pre();
    testContestDeletions1_Pre();

    #if 0 // This is disabled until a backend which supports exceptions is available again
    // Single-stage tests
    testRejectionDetection();
    testRejectInvalidContestCreation();

    // Multi-stage tests
    testRejectEarlyAndLateVoting_Pre();
    testRejectAbstentionProhibition_Pre();
    testRejectSplitVoteProhibition_Pre();
    testRejectInvalidVoting_Pre();
    testRejectModifyGroupAndContest_Pre();
    #endif
}


/**
 * Run the during voting-period tests
 */
void Pollaris::runDuringVotingPeriodTests() {
    test1Person1VoteScenario1_During();
    test1Person1VoteScenario2_During();
    test1Person3VoteScenario1_During();
    testDifferentWeightedVotingScenario1_During();
    testDifferentWeightedVotingScenario2_During();
    testContestDeletions1_During();

    #if 0 // This is disabled until a backend which supports exceptions is available again
    testRejectAbstentionProhibition_During();
    testRejectSplitVoteProhibition_During();
    testRejectInvalidVoting_During();
    testRejectModifyGroupAndContest_During();
    #endif
}

/**
 * Run the post-voting-period tests
 */
void Pollaris::runPostVotingPeriodTests() {
    test1Person1VoteScenario1_Post();
    test1Person1VoteScenario2_Post();
    test1Person3VoteScenario1_Post();
    testDifferentWeightedVotingScenario1_Post();
    testDifferentWeightedVotingScenario2_Post();
    testContestDeletions1_Post();

    #if 0 // This is disabled until a backend which supports exceptions is available again
    testRejectEarlyAndLateVoting_Post();
    testRejectModifyGroupAndContest_Post();
    #endif

    BAL::Log("ALL Tests PASSED");
}


/**
 * BEGINNING OF Helper functions
 */

/**
 * Find and verify by name the existence of an account on the backend.
 * Failure to find the account will result in a verification failure.
 */
BAL::AccountHandle FindAccount(BAL::Contract& contract, const BAL::Name& name) {
    BAL::AccountName accountName(name.to_string());
    std::optional<BAL::AccountHandle> accountId = contract.getAccountHandle(accountName);
    BAL::Verify(accountId.has_value(), std::string("Account (") + name.to_string() + ") was not found!");
    return *accountId;
}

/**
 * Find and verify by ID the existence of an account on the backend.
 * Failure to find the account will result in a verification failure.
 */
BAL::AccountHandle FindAccount(BAL::Contract& contract, const BAL::AccountId& id) {
    std::optional<BAL::AccountHandle> handle = contract.getAccountHandle(id);
    BAL::Verify(handle.has_value(), std::string("Account (") + std::to_string(uint64_t(id)) + ") was not found!");
    return *handle;
}


/**
 * Determine whether a voter is present in a polling group
 * Providing the name of a group that does not exist will cause the function to return false
 */
bool IsVoterPresent(Pollaris::Contract& contract, const GroupId& groupId, const BAL::AccountHandle& voter) {
    // Search for the voter among the members in the group
    const Pollaris::GroupAccounts& group_accounts = contract.getTable<Pollaris::GroupAccounts>(groupId);
    auto groupAcctItr = group_accounts.findId(voter);
    bool isFound = groupAcctItr != group_accounts.end();

    return isFound;
};


/**
 * Find and verify by ID a polling group on the backend.
 * Failure to find the group will result in a verification failure.
 */
GroupId Pollaris::FindGroupId(const std::string& groupName, std::string errorMessage) {
    Pollaris::PollingGroups groups = getTable<Pollaris::PollingGroups>(GLOBAL.value);

    const PollingGroup* group = FindGroup(groups, groupName);
    BAL::Verify(group != nullptr, errorMessage);

    GroupId groupId = group->id;
    return std::move(groupId);
}


/**
 * Seek a contest ID
 * An empty search result will be indicated by a nullptr
 */
const ContestId* SeekContestId(Pollaris::Contract& contract, GroupId& groupId,
                                      const string& contestName) {
    Pollaris::Contests c = contract.getTable<Pollaris::Contests>(groupId);
    for (auto itr = c.begin(); itr != c.end(); itr++)
        if (itr->name == contestName)
            return &(itr->id);

    return nullptr;
}


/**
 * Find and verify by ID the existence of a contest on the backend.
 * Failure to find the contest will result in a verification failure.
 */
ContestId FindContestId(Pollaris::Contract& contract, GroupId& groupId,
                                  const std::string& contestName,
                                  std::string errorMessage = "Contest should have been found!") {
    const ContestId* ptrContestId = SeekContestId(contract, groupId, contestName);
    BAL::Verify(ptrContestId != nullptr, errorMessage);
    ContestId contestId = *ptrContestId;
    return std::move(contestId);
}


/**
 * Seek a contestant by their name and description within a group's contest
 * An empty search result will be indicated by a nullptr
 */
const ContestantId* SeekOfficialContestant(Pollaris::Contestants& groupContestants, ContestId& contestId, const string& name) {
    auto contestantsByContest = groupContestants.getSecondaryIndex<BY_CONTEST>();
    auto contestantsRange = contestantsByContest.range(Pollaris::Contestant::contestKeyMin(contestId),
                                                       Pollaris::Contestant::contestKeyMax(contestId));

    for (auto itr = contestantsRange.first; itr != contestantsRange.second; itr++)
        if (itr->name == name)
            return &(itr->id);

    return nullptr;
}


/**
 * Seek a write-incontestant by their name and description within a group's contest
 * An empty search result will be indicated by a nullptr
 */
const WriteInId* SeekWriteInContestant(Pollaris::WriteIns& groupContestants, ContestId& contestId, const string& name) {
    auto contestantsByContest = groupContestants.getSecondaryIndex<BY_CONTEST>();
    auto contestantsRange = contestantsByContest.range(Pollaris::WriteIn::contestKeyMin(contestId),
                                                       Pollaris::WriteIn::contestKeyMax(contestId));

    for (auto itr = contestantsRange.first; itr != contestantsRange.second; itr++)
        if (itr->name == name)
            return &(itr->id);

    return nullptr;
}


/**
 * Seek a result ID
 * An empty search result will be indicated by a nullptr
 */
const ResultId* SeekNewestResultId(Pollaris::Results& groupResults, ContestId& contestId) {
    auto resultsByContest = groupResults.getSecondaryIndex<BY_CONTEST>();
    auto resultsRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestId),
                                               Pollaris::Result::contestKeyMax(contestId));
    if (resultsRange.first == resultsRange.second)
        return nullptr;

    auto newestResult = resultsRange.first;
    Timestamp ts(resultsRange.first->timestamp);
    for (auto itr = resultsRange.first; itr != resultsRange.second; itr++) {
        if (itr->timestamp >= ts) {
            newestResult = itr;
            ts = itr->timestamp;
        }
    }

    return &(newestResult->id);
}


/**
 * Verify expected tallies versus actual tallies in a result
 */
void VerifyTallies(Pollaris::Contract& contract, GroupId& groupId, ResultId& resultId,
                          map<ContestantIdVariant, uint64_t> expectedTallies) {
    Pollaris::Tallies t = contract.getTable<Pollaris::Tallies>(groupId);
    auto talliesByResult = t.getSecondaryIndex<BY_RESULT>();
    auto talliesRange = talliesByResult.range(Pollaris::Tally::resultKeyMin(resultId),
                                              Pollaris::Tally::resultKeyMax(resultId));

    // Create a map of the actual results
    map<ContestantIdVariant, uint64_t> actualTallies;
    for (auto itr = talliesRange.first; itr != talliesRange.second; itr++) {
        actualTallies.insert(std::make_pair(itr->contestant, itr->tally));
    }

    // Compare expected to actual
    BAL::Verify(expectedTallies.size() == actualTallies.size(),
                "The quantity of expected tallies should not differ from the actual tallies!");
    for (const auto& [contestantId, expectedTally] : expectedTallies) {
        BAL::Verify(actualTallies.find(contestantId) != actualTallies.end(),
                    "The tally for an expected contestant was not found among the actual tallies!");

        const uint64_t actualTally = actualTallies[contestantId];
        BAL::Verify(actualTally == expectedTally,
                    "The expected tally for a contestant differs from its actual tally!");
    }
}

/**
 * Check for no decisions in a group's contest
 */
bool IsDecisionsEmpty(Pollaris::Contract& contract, GroupId& groupId, ContestId& contestId) {
    Pollaris::Decisions d = contract.getTable<Pollaris::Decisions>(groupId);
    auto decisionsByContest = d.getSecondaryIndex<BY_CONTEST>();
    auto decisionsRange = decisionsByContest.range(Pollaris::Decision::contestKeyMin(contestId),
                                                   Pollaris::Decision::contestKeyMax(contestId));
    bool isEmpty = decisionsRange.first == decisionsRange.second;

    return isEmpty;
}

/**
 * Check for no results in a group's contest
 */
bool IsResultsEmpty(Pollaris::Contract& contract, GroupId& groupId, ContestId& contestId) {
    Pollaris::Results r = contract.getTable<Pollaris::Results>(groupId);
    auto resultsByContest = r.getSecondaryIndex<BY_CONTEST>();
    auto resultsRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestId),
                                               Pollaris::Result::contestKeyMax(contestId));
    bool isEmpty = resultsRange.first == resultsRange.second;

    return isEmpty;
}

/**
 * Check for no tallies in a group's results
 */
bool IsTalliesEmpty(Pollaris::Contract& contract, GroupId& groupId, set<ResultId>& contestResults) {
    Pollaris::Tallies t = contract.getTable<Pollaris::Tallies>(groupId);
    auto talliesByResult = t.getSecondaryIndex<BY_RESULT>();

    for (ResultId resultId : contestResults) {
        auto talliesRange = talliesByResult.range(Pollaris::Tally::resultKeyMin(resultId),
                                                  Pollaris::Tally::resultKeyMax(resultId));
        if (talliesRange.first != talliesRange.second)
            return false;
    }

    return true;
}

const uint32_t secondsOfDelayBeforeContestBegins = 2;
const uint32_t contestDurationSecs = 60;

/**
 * END OF Helper functions
 */



const std::string TEST_PREFIX = "Test Unit Tests ";
/**
 * Reset any residual test data associated with groups with names beginning with TEST_PREFIX
 */
void Pollaris::resetTestData() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("Resetting test data ...");

    PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
    for (auto itrGroup = groups.begin(); itrGroup != groups.end(); itrGroup = groups.erase(itrGroup)) {
        if (itrGroup->name.find(TEST_PREFIX) != 0) {
            // Skip groups that do not begin with the TEST_PREFIX
            continue;
        }

        GroupId groupId = itrGroup->id;

        Tallies t = getTable<Pollaris::Tallies>(groupId);
        for (auto itr = t.begin(); itr != t.end(); itr = t.erase(itr)) {}

        Results r = getTable<Pollaris::Results>(groupId);
        for (auto itr = r.begin(); itr != r.end(); itr = r.erase(itr)) {}

        Decisions d = getTable<Decisions>(itrGroup->id);
        for (auto itr = d.begin(); itr != d.end(); itr = d.erase(itr)) {}

        Contestants cx = getTable<Contestants>(itrGroup->id);
        for (auto itr = cx.begin(); itr != cx.end(); itr = cx.erase(itr)) {}

        WriteIns wi = getTable<WriteIns>(itrGroup->id);
        for (auto itr = wi.begin(); itr != wi.end(); itr = wi.erase(itr)) {}

        Contests c = getTable<Contests>(itrGroup->id);
        for (auto itr = c.begin(); itr != c.end(); itr = c.erase(itr)) {}

        GroupAccounts ga = getTable<GroupAccounts>(itrGroup->id);
        for (auto itr = ga.begin(); itr != ga.end(); itr = ga.erase(itr)) {}
    }
    BAL::Log("... test data has been reset");
}


/**
 * Test the direct creation of polling group tables
 */
void Pollaris::testPollingGroups1() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Polling Groups Scenario 1");

    BAL::Name alice = "alice"_N;
    BAL::Name bob = "bob"_N;
    BAL::Verify(alice == "alice"_N, "Identical names do not compare equal!");
    BAL::Verify(alice != bob, "Names alice and bob do not compare different!", alice, bob);

    PollingGroups groups = getTable<PollingGroups>("testing"_N.value);
    auto ClearTables = [&] {
        {
            auto itr = groups.begin();
            while (itr != groups.end()) itr = groups.erase(itr);
        }
    };
    BAL::Log("=> Clearing testing tables");
    ClearTables();

    BAL::Log("=> Index tests");
    BAL::Log("==> Testing PollingGroups table");
    BAL::Log("===> Creating records");
    groups.create([](PollingGroup& g) {
        g.id = GroupId{0};
        g.name = "Latter";
    });
    groups.create([](PollingGroup& g) {
        g.id = GroupId{1};
        g.name = "Former";
    });

    {
        BAL::Log("===> Checking records, primary index");
        auto itr = groups.begin();
        BAL::Verify(itr->id == 0, "ID mismatch: ID 0");
        BAL::Verify(itr->name == "Latter", "Name mismatch: ID 0");
        ++itr;
        BAL::Verify(itr->id == 1, "ID mismatch: ID 1");
        BAL::Verify(itr->name == "Former", "Name mismatch: ID 1");
    }
    {
        BAL::Log("===> Checking records, names index");
        auto groupsByName = groups.getSecondaryIndex<BY_NAME>();
        auto itr = groupsByName.begin();
        BAL::Verify(itr->id == 1, "ID mismatch: ID 1");
        BAL::Verify(itr->name == "Former", "Name mismatch: ID 1");
        ++itr;
        BAL::Verify(itr->id == 0, "ID mismatch: ID 0");
        BAL::Verify(itr->name == "Latter", "Name mismatch: ID 0");
    }

    BAL::Log("=> Clearing testing tables");
    ClearTables();
    BAL::Log("Test: PASSED");
}


/**
 * Test the addition and removal of accounts to a polling group without any contests.
 * The test presumes the existence of an account named "testvoter1".
 */
void Pollaris::testPollingGroupMembership1() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Polling Groups Membership Scenario 1");

    // Initialize values for the test
    const std::string groupName = TEST_PREFIX + "Polling Group Membership 1";
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);

    // Verify the existence of blockchain accounts needed for the test
    BAL::Verify(accountExists(voter1), "Test voter account does not exist:", voter1);

    // Verify that the group has no members
    PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
    {
        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "The test group should not exist at start of test");
    }

    // Add a voter
    BAL::Log("=> Add voter to polling group");
    const uint32_t voter1_weight = 1;
    addVoter(groupName, voter1, voter1_weight, emptyTags);

    // Verify the existence of the group
    BAL::Log("=> Checking polling group");
    const PollingGroup* group = FindGroup(groups, groupName);
    BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

    // Verify the presence of the test voter in the group
    BAL::Log("=> Checking polling group membership");
    {
        const GroupAccounts& group_accounts = getTable<GroupAccounts>(group->id);
        auto groupAcctItr = group_accounts.findId(voter1);
        BAL::Verify(groupAcctItr != group_accounts.end(), "Test voter should no longer be a member of the test group");
    }

    // Remove the voter
    BAL::Log("=> Removing voter from polling group");
    removeVoter(groupName, voter1);

    // Verify the absence of any voters in the group
    BAL::Log("=> Checking polling group membership");
    {
        const GroupAccounts& group_accounts = getTable<GroupAccounts>(group->id);
        BAL::Verify(group_accounts.begin() == group_accounts.end(), "The test group should not have any members");
    }

    // Clean the test artifacts
    BAL::Log("=> Cleaning test artifacts");
    groups.erase(*group);
    // Verify the absence of the group
    {
        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "The test group was not cleared at the test conclusion");
    }

    BAL::Log("Test: PASSED");
}


/**
 * Create a group.  Rename the group.
 * Ensure that membership in the group is retained between renaming.
 *
 * The test presumes the existence of the accounts "testvoter1", "testvoter2"
 */
void Pollaris::testCreateAndRenameGroup() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Group: Create and Rename");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName0 = TEST_PREFIX + "Rename Scenario 1";
    const std::string groupName1 = TEST_PREFIX + "Rename Scenario 1 (Update 1)";
    const std::string groupName2 = TEST_PREFIX + "Rename Scenario 1 (Update 2)";
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);

    // Verify that the group does not exist
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group0 = FindGroup(groups, groupName0);
        BAL::Verify(group0 == nullptr, "Test group 0 should not exist at start of test");

        const PollingGroup* group1 = FindGroup(groups, groupName1);
        BAL::Verify(group1 == nullptr, "Test group 1 should not exist at start of test");

        const PollingGroup* group2 = FindGroup(groups, groupName2);
        BAL::Verify(group2 == nullptr, "Test group 2 should not exist at start of test");
    }


    ///
    /// Add a voter
    ///
    {
        BAL::Log("=> Add Voter 1 to polling group");
        const uint32_t voter1_weight = 1;
        addVoter(groupName0, voter1, voter1_weight, emptyTags);
    }

    // Verify the existence of the group and voter
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        BAL::Log("=> Checking polling group");
        const PollingGroup* group = FindGroup(groups, groupName0);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Log("=> Checking polling group membership");
        BAL::Verify(IsVoterPresent(*this, group->id, voter1), "Test voter should have been found in the test group");
    }


    ///
    /// Add another voter
    ///
    {
        BAL::Log("=> Add Voter 2 to polling group");
        const uint32_t voter2_weight = 1;
        addVoter(groupName0, voter2, voter2_weight, emptyTags);
    }

    // Verify the existence of the group and voter
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        BAL::Log("=> Checking polling group");
        const PollingGroup* group = FindGroup(groups, groupName0);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the both voters in the group
        BAL::Log("=> Checking polling group membership");
        BAL::Verify(IsVoterPresent(*this, group->id, voter1), "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2), "Test voter should have been found in the test group");
    }


    ///
    /// Rename the group while it has members
    ///
    {
        BAL::Log("=> Group Renaming #1");
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* ptrGroup0Before = FindGroup(groups, groupName0);
        BAL::Verify(ptrGroup0Before != nullptr,
                    "Test group should have been found before the update!");

        const PollingGroup* ptrGroup1Before = FindGroup(groups, groupName1);
        BAL::Verify(ptrGroup1Before == nullptr,
                    "Test group should NOT have been found before the update!");

        // Rename
        renameGroup(groupName0, groupName1);

        // Verify the presence of the group with the updated name
        PollingGroups groupsRefresh = getTable<PollingGroups>(GLOBAL.value); // Refresh perception of groups
        const PollingGroup* ptrGroup1After = FindGroup(groupsRefresh, groupName1);
        BAL::Verify(ptrGroup1After != nullptr,
                    "Test group should have been found after the name update");

        // Verify the absence of the group with the original name
        const PollingGroup* ptrGroup0After = FindGroup(groupsRefresh, groupName0);
        BAL::Verify(ptrGroup0After == nullptr,
                    "Test group by the original name should NOT have been found after the name update");

        // Verify that the group IDs are the same
        BAL::Verify(ptrGroup0Before->id == ptrGroup1After->id,
                    "The ID of the group should not have changed when it was renamed");

        // Verify the presence of the test voters in the group
        BAL::Log("=> Checking polling group membership");
        BAL::Verify(IsVoterPresent(*this, ptrGroup1After->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, ptrGroup1After->id, voter2),
                    "Test voter should have been found in the test group");
    }


    ///
    /// Remove a member to create an empty group
    ///
    {
        BAL::Log("=> Removing voters");
        removeVoter(groupName1, voter1);
        removeVoter(groupName1, voter2);
    }

    // Verify the absence of any voters in the group
    BAL::Log("=> Checking polling group membership");
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
        const PollingGroup* group = FindGroup(groups, groupName1);
        const GroupAccounts& group_accounts = getTable<GroupAccounts>(group->id);

        BAL::Verify(group_accounts.begin() == group_accounts.end(), "No voters should have been found in the the test group");
    }


    ///
    /// Rename the group while it has no members
    ///
    {
        BAL::Log("=> Group Renaming #2");
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        // Rename the empty group again
        renameGroup(groupName1, groupName2);

        // Verify the presence of the group with Name 2
        PollingGroups groupsRefresh = getTable<PollingGroups>(GLOBAL.value); // Refresh perception of groups
        const PollingGroup* ptrAfterName2 = FindGroup(groupsRefresh, groupName2);
        BAL::Verify(ptrAfterName2 != nullptr, "Test group should have been found after the name reset");

        // Verify the absence of the group with the Name 1
        const PollingGroup* ptrAfterName1 = FindGroup(groupsRefresh, groupName1);
        BAL::Verify(ptrAfterName1 == nullptr, "Test group should NOT have been found after the name reset");

        // Verify the continued absence of the group with the Name 0
        const PollingGroup* ptrAfterName0 = FindGroup(groupsRefresh, groupName0);
        BAL::Verify(ptrAfterName0 == nullptr, "Test group should NOT have been found after the name reset");
    }


    ///
    /// Clean the test artifacts
    ///
    BAL::Log("=> Cleaning test artifacts");
    {
       // Verify the absence of members in the groups
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        auto ClearGroupData = [&contract=*this](PollingGroups& groups,
                                                const PollingGroup* group) {
            if (group != nullptr) {
                // Clear accounts from the group
                GroupAccounts ga = contract.getTable<GroupAccounts>(group->id);
                for (auto itr = ga.begin(); itr != ga.end(); itr = ga.erase(itr)) {}

                // Clear the group from groups
                groups.erase(*group);
            }
        };

        const PollingGroup* group0 = FindGroup(groups, groupName0);
        ClearGroupData(groups, group0);

        const PollingGroup* group1 = FindGroup(groups, groupName1);
        ClearGroupData(groups, group1);

        const PollingGroup* group2 = FindGroup(groups, groupName2);
        ClearGroupData(groups, group2);
    }

    BAL::Log("Test: PASSED");
}


/**
 * BEGINNING OF test1Person1VoteScenario1
 *
 * Test 1 person-1 vote for official slate without voting updates
 *
 * - 5 equal-weighted voters
 * - 1 contest with 2 contestants
 * - The voters decide 3 to 2.
 * - Tally vote four times:
 *     - before any voting
 *     - after some voting yet before other voting
 *     - after all voting but before deadline
 *     - after deadline
 */
const std::string test1Person1VoteScenario1GroupName = TEST_PREFIX + "1 Person-1 Vote";
const std::string test1Person1VoteScenario1Contest1Name = "Contest for " + test1Person1VoteScenario1GroupName;
const std::string test1Person1VoteScenario1Contestant1Name = "Contestant 1";
const std::string test1Person1VoteScenario1Contestant2Name = "Contestant 2";

/**
 * Test 1 person-1 vote for official slate without voting updates
 *
 * - 5 equal-weighted voters
 * - 1 contest with 2 contestants
 */
void Pollaris::test1Person1VoteScenario1_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 1 Vote Scenario 1: Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person1VoteScenario1GroupName;
    const std::string contestName = test1Person1VoteScenario1Contest1Name;
    const std::string contestDesc = contestName + ": Description";
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Verify that the group does not exist
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "Test group 0 should not exist at start of test");
    }


    ///
    /// Add voters
    ///
    BAL::Log("=> Adding voters to polling group");
    {
        const uint32_t voterWeight = 1;
        addVoter(groupName, voter1, voterWeight, emptyTags);
        addVoter(groupName, voter2, voterWeight, emptyTags);
        addVoter(groupName, voter3, voterWeight, emptyTags);
        addVoter(groupName, voter4, voterWeight, emptyTags);
        addVoter(groupName, voter5, voterWeight, emptyTags);
    }

    // Verify the existence of the group and voter
    GroupId groupId;
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Log("=> Checking polling group membership");
        BAL::Verify(IsVoterPresent(*this, group->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter3),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter4),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter5),
                    "Test voter should have been found in the test group");

        groupId = group->id;
    }

    ///
    /// Create the contest
    ///
    BAL::Log("=> Creating Contest");
    const Timestamp contestBegin = currentTime() + secondsOfDelayBeforeContestBegins;
    Timestamp contestEnd = contestBegin + contestDurationSecs;

    ContestantDescriptor c1;
    ContestantDescriptor c2;
    {
        set<ContestantDescriptor> contestants;

        c1.name = test1Person1VoteScenario1Contestant1Name;
        c1.description = "Description for " + c1.name;
        contestants.insert(c1);

        c2.name = test1Person1VoteScenario1Contestant2Name;
        c2.description = "Description for " + c2.name;
        contestants.insert(c2);

        newContest(groupId, contestName, contestDesc, contestants,
                   contestBegin, contestEnd, emptyTags);
    }

    // Verify the existence of the contest
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");


    // Verify the existence of the contestants
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    // No results are expected before the tally
    {
        Results r = getTable<Results>(groupId);
        BAL::Verify(r.begin() == r.end(), "No results are expected for the test group");

        Tallies t = getTable<Tallies>(groupId);
        BAL::Verify(t.begin() == t.end(), "No tallies are expected for the test group");
    }

    // Tally
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    map<ContestantIdVariant, uint64_t> expectedTallies;
    expectedTallies.insert(std::make_pair(c1Id, 0));
    expectedTallies.insert(std::make_pair(c2Id, 0));

    VerifyTallies(*this, groupId, resultId, expectedTallies);
}


/**
 * Stage 1: 1 voters vote for Candidate 1, 2 voter votes for Candidate 2
 * Stage 2: 2 voters vote for Candidate 2
 */
void Pollaris::test1Person1VoteScenario1_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 1 Vote Scenario 1: During");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person1VoteScenario1GroupName;
    const std::string contestName = test1Person1VoteScenario1Contest1Name;
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }


    ///
    /// Stage 1
    /// 1 voters vote for Candidate 1, 2 voter votes for Candidate 2
    ///
    BAL::Log("=> Stage 1 Voting");
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 1));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        setDecision(groupId, contestId, voter2, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        setDecision(groupId, contestId, voter3, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 1));
        expectedTallies.insert(std::make_pair(c2Id, 2));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 2
    /// 2 voters vote for Candidate 1
    ///
    BAL::Log("=> Stage 2 Voting");
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 1));
        setDecision(groupId, contestId, voter4, opinions, emptyTags);
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 3));
        expectedTallies.insert(std::make_pair(c2Id, 2));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}


/**
 * Tally the votes
 * Expected vote counts:
 * Candidate 1 = 3
 * Candidate 2 = 2
 */
void Pollaris::test1Person1VoteScenario1_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 1 Vote Scenario 1: Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person1VoteScenario1GroupName;
    const std::string contestName = test1Person1VoteScenario1Contest1Name;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 3));
        expectedTallies.insert(std::make_pair(c2Id, 2));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}

/**
 * END OF test1Person1VoteScenario1
 */




/**
 * BEGINNING OF test1Person1VoteScenario2
 * Test 1 person-1 vote for official slate with voting updates
 *
 * - 5 equal-weighted voters
 * - 1 contest with 2 contestants (abstention allowed).
 * - The voters decide 3 to 2 but then swap votes to 1 to 3
 *   (1 voter rescinds, and 2 voters change votes for the other contestant).
 * - Tally vote four times:
 *     - before any voting
 *     - after some voting yet before other voting
 *     - after all voting but before deadline
 *     - after deadline
 */
const std::string test1Person1VoteScenario2GroupName = TEST_PREFIX + "1 Person-1 Vote with Vote Updates";
const std::string test1Person1VoteScenario2Contest1Name = "Contest for " + test1Person1VoteScenario2GroupName;
const std::string test1Person1VoteScenario2Contestant1Name = "Contestant 1";
const std::string test1Person1VoteScenario2Contestant2Name = "Contestant 2";

/**
 * - 5 equal-weighted voters
 * - 1 contest with 2 contestants (abstention allowed).
 */
void Pollaris::test1Person1VoteScenario2_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + test1Person1VoteScenario2Contest1Name + ": Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person1VoteScenario2GroupName;
    const std::string contestName = test1Person1VoteScenario2Contest1Name;


    const std::string contestDesc = contestName + ": Description";
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Verify that the group does not exist
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "Test group 0 should not exist at start of test");
    }


    ///
    /// Add voters
    ///
    BAL::Log("=> Adding voters to polling group");
    {
        const uint32_t voterWeight = 1;
        addVoter(groupName, voter1, voterWeight, emptyTags);
        addVoter(groupName, voter2, voterWeight, emptyTags);
        addVoter(groupName, voter3, voterWeight, emptyTags);
        addVoter(groupName, voter4, voterWeight, emptyTags);
        addVoter(groupName, voter5, voterWeight, emptyTags);
    }

    // Verify the existence of the group and voter
    GroupId groupId;
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        BAL::Log("=> Checking polling group");
        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Verify(IsVoterPresent(*this, group->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter3),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter4),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter5),
                    "Test voter should have been found in the test group");

        groupId = group->id;
    }


    ///
    /// Create the contest
    ///
    BAL::Log("=> Creating contest");
    const Timestamp contestBegin = currentTime() + secondsOfDelayBeforeContestBegins;
    Timestamp contestEnd = contestBegin + contestDurationSecs;

    ContestantDescriptor c1;
    ContestantDescriptor c2;
    {
        set<ContestantDescriptor> contestants;

        c1.name = test1Person1VoteScenario2Contestant1Name;
        c1.description = "Description for " + c1.name;
        contestants.insert(c1);

        c2.name = test1Person1VoteScenario2Contestant2Name;
        c2.description = "Description for " + c2.name;
        contestants.insert(c2);

        newContest(groupId, contestName, contestDesc, contestants,
                   contestBegin, contestEnd, emptyTags);
    }

    // Verify the existence of the contest
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Verify the existence of the contestants
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario2Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario2Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    // No results are expected before the tally
    {
        Results r = getTable<Results>(groupId);
        BAL::Verify(r.begin() == r.end(), "No results are expected for the test group");

        Tallies t = getTable<Tallies>(groupId);
        BAL::Verify(t.begin() == t.end(), "No tallies are expected for the test group");
    }

    // Tally
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    map<ContestantIdVariant, uint64_t> expectedTallies;
    expectedTallies.insert(std::make_pair(c1Id, 0));
    expectedTallies.insert(std::make_pair(c2Id, 0));

    VerifyTallies(*this, groupId, resultId, expectedTallies);
}


/**
 * Stage 1: 3 voters vote for Candidate 1, 2 voter votes for Candidate 2
 * Stage 2: 2 voters for Candidate 1 change votes for Candidate 2
 *          1 voter for Candidate 2 rescinds/abstains
 */
void Pollaris::test1Person1VoteScenario2_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 1 Vote Scenario 2: During");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person1VoteScenario2GroupName;
    const std::string contestName = test1Person1VoteScenario2Contest1Name;
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario2Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario2Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }


    ///
    /// Stage 1
    /// 3 voters vote for Candidate 1, 2 voter votes for Candidate 2
    ///
    BAL::Log("=> Stage 1 Voting");
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 1));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);
        setDecision(groupId, contestId, voter2, opinions, emptyTags);
        setDecision(groupId, contestId, voter3, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        setDecision(groupId, contestId, voter4, opinions, emptyTags);
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 3));
        expectedTallies.insert(std::make_pair(c2Id, 2));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 2
    /// 2 voters for Candidate 1 change votes for Candidate 2
    /// 1 voter for Candidate 2 rescinds/fully abstains
    ///
    BAL::Log("=> Stage 2 Voting");
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);
        setDecision(groupId, contestId, voter2, opinions, emptyTags);

        opinions = FullOpinions();
        Tags abstentionTags;
        abstentionTags.insert(ABSTAIN_VOTE_TAG);
        setDecision(groupId, contestId, voter4, opinions, abstentionTags);
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 1));
        expectedTallies.insert(std::make_pair(c2Id, 3));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}

/**
 * Tally the votes
 * Expected vote counts:
 * Candidate 1 = 1
 * Candidate 2 = 3
 */
void Pollaris::test1Person1VoteScenario2_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 1 Vote Scenario 2: Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person1VoteScenario2GroupName;
    const std::string contestName = test1Person1VoteScenario2Contest1Name;


    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario2Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person1VoteScenario2Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 1));
        expectedTallies.insert(std::make_pair(c2Id, 3));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}

/**
 * END OF test1Person1VoteScenario2
 */


/**
 * BEGINNING OF test1Person3VoteScenario1
 *
 * - 5 equal-weighted voters with 3 votes.
 * - 1 contest with 3 contestants (abstention allowed).
 * - Stage 1: Initial Voting
 *     - 4 voters votes for Candidate 1
 *     - 1 voter votes for Candidate 3
 *     - Tally should be:
 *            Candidate 1 = 4 * 3 = 12
 *            Candidate 2 = 0
 *            Candidate 3 = 1 * 3 = 3
 * - Stage 2: Some voters update their votes
 *            1 voter abstains
 *            1 voter pulls 1 weight from C1
 *            1 voter one shifts 2 weights from C1 to C2
 *            1 voter shifts 3 weights from C3 to C2
 *            1 voter retains their prior vote
 * - Final tally should be:
 *            Candidate 1 = 6
 *            Candidate 2 = 5
 *            Candidate 3 = 0
 * - Tally vote four times:
 *     - before any voting
 *     - after some voting yet before other voting
 *     - after all voting but before deadline
 *     - after deadline
 */
const std::string test1Person3VoteScenario1GroupName = TEST_PREFIX + "1 Person-3 Votes";
const std::string test1Person3VoteScenario1Contest1Name = "Contest for " + test1Person3VoteScenario1GroupName;
const std::string test1Person3VoteScenario1Contestant1Name = "Contestant 1";
const std::string test1Person3VoteScenario1Contestant2Name = "Contestant 2";
const std::string test1Person3VoteScenario1Contestant3Name = "Contestant 3";


/**
 * - 5 equal-weighted voters with 3 votes.
 * - 1 contest with 3 contestants (abstention allowed).
 */
void Pollaris::test1Person3VoteScenario1_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + test1Person3VoteScenario1Contest1Name + ": Pre");


    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person3VoteScenario1GroupName;
    const std::string contestName = test1Person3VoteScenario1Contest1Name;
    const std::string contestDesc = contestName + ": Description";
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Verify that the group does not exist
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "Test group 0 should not exist at start of test");
    }

    ///
    /// Add voters
    ///
    BAL::Log("=> Adding voters to polling group");
    {
        const uint32_t voterWeight = 3;
        addVoter(groupName, voter1, voterWeight, emptyTags);
        addVoter(groupName, voter2, voterWeight, emptyTags);
        addVoter(groupName, voter3, voterWeight, emptyTags);
        addVoter(groupName, voter4, voterWeight, emptyTags);
        addVoter(groupName, voter5, voterWeight, emptyTags);
    }


    // Verify the existence of the group and voter
    GroupId groupId;
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        BAL::Log("=> Checking polling group");
        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Verify(IsVoterPresent(*this, group->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter3),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter4),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter5),
                    "Test voter should have been found in the test group");

        groupId = group->id;
    }


    ///
    /// Create the contest
    ///
    BAL::Log("=> Creating contest");
    const Timestamp contestBegin = currentTime() + secondsOfDelayBeforeContestBegins;
    Timestamp contestEnd = contestBegin + contestDurationSecs;

    ContestantDescriptor c1;
    ContestantDescriptor c2;
    ContestantDescriptor c3;
    {
        set<ContestantDescriptor> contestants;

        c1.name = test1Person3VoteScenario1Contestant1Name;
        c1.description = "Description for " + c1.name;
        contestants.insert(c1);

        c2.name = test1Person3VoteScenario1Contestant2Name;
        c2.description = "Description for " + c2.name;
        contestants.insert(c2);

        c3.name = test1Person3VoteScenario1Contestant3Name;
        c3.description = "Description for " + c3.name;
        contestants.insert(c3);

        newContest(groupId, contestName, contestDesc, contestants,
                   contestBegin, contestEnd, emptyTags);
    }

    // Verify the existence of the contest
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Verify the existence of the contestants
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    // No results are expected before the tally
    {
        Results r = getTable<Results>(groupId);
        BAL::Verify(r.begin() == r.end(), "No results are expected for the test group");

        Tallies t = getTable<Tallies>(groupId);
        BAL::Verify(t.begin() == t.end(), "No tallies are expected for the test group");
    }

    // Tally
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    map<ContestantIdVariant, uint64_t> expectedTallies;
    expectedTallies.insert(std::make_pair(c1Id, 0));
    expectedTallies.insert(std::make_pair(c2Id, 0));
    expectedTallies.insert(std::make_pair(c3Id, 0));

    VerifyTallies(*this, groupId, resultId, expectedTallies);
}


/**
 * - Stage 1: Initial Voting
 *     - 4 voters votes for Candidate 1
 *     - 1 voter votes for Candidate 3
 *     - Tally should be:
 *            Candidate 1 = 4 * 3 = 12
 *            Candidate 2 = 0
 *            Candidate 3 = 1 * 3 = 3
 * - Stage 2: Some voters update their votes
 *            1 voter abstains
 *            1 voter pulls 1 weight from C1
 *            1 voter shifts 2 weights from C1 to C2
 *            1 voter shifts 3 weights from C3 to C2
 *            1 voter retains their prior vote
 */
void Pollaris::test1Person3VoteScenario1_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 3 Vote Scenario 1: During");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person3VoteScenario1GroupName;
    const std::string contestName = test1Person3VoteScenario1Contest1Name;
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Find the group ID
    GroupId groupId;
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found");

        groupId = group->id;
    }

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Stage 1
    /// - 4 voters votes for Candidate 1
    /// - 1 voter votes for Candidate 3
    ///
    /// All voting is full-weight of 3
    ///
    BAL::Log("=> Stage 1 Voting");
    {
        FullOpinions opinions;

        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);
        setDecision(groupId, contestId, voter2, opinions, emptyTags);
        setDecision(groupId, contestId, voter3, opinions, emptyTags);
        setDecision(groupId, contestId, voter4, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 3));
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 4 * 3 = 12
    /// Candidate 2 = 0
    /// Candidate 3 = 1 * 3 = 3
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 12));
        expectedTallies.insert(std::make_pair(c2Id, 0));
        expectedTallies.insert(std::make_pair(c3Id, 3));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 2
    ///
    /// Voter 1 abstains
    /// Voter 2 pulls 1 weight from C1
    /// Voter 3 shifts 2 weights from C1 to C2
    /// Voter 4 retains his prior vote by not updating his vote
    /// Voter 5 shifts 3 weights from C3 to C2
    ///
    BAL::Log("=> Stage 2 Voting");
    {
        FullOpinions opinions;

        // Voter 1
        Tags fullAbstentionTags;
        fullAbstentionTags.insert(ABSTAIN_VOTE_TAG);
        setDecision(groupId, contestId, voter1, opinions, fullAbstentionTags);

        // Voter 2
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 2));
        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "1");
        setDecision(groupId, contestId, voter2, opinions, partialAbstentionTags);

        // Voter 3
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 1));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 2));
        setDecision(groupId, contestId, voter3, opinions, emptyTags);

        // Voter 4
        // Does not update prior vote

        // Voter 5
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 3));
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 2 from V1 + 1 from V2 + 3 from V4 = 6
    /// Candidate 2 = 2 from V2 + 3 from V5 = 5
    /// Candidate 3 = 0
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 6));
        expectedTallies.insert(std::make_pair(c2Id, 5));
        expectedTallies.insert(std::make_pair(c3Id, 0));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}


/**
 * Tally the votes
 *
 * Expected vote counts:
 * Candidate 1 = 6
 * Candidate 2 = 5
 * Candidate 3 = 0
 */
void Pollaris::test1Person3VoteScenario1_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting 1 Person - 1 Vote Scenario 2: Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = test1Person3VoteScenario1GroupName;
    const std::string contestName = test1Person3VoteScenario1Contest1Name;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        test1Person3VoteScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 6));
        expectedTallies.insert(std::make_pair(c2Id, 5));
        expectedTallies.insert(std::make_pair(c3Id, 0));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}

/**
 * END OF test1Person3VoteScenario1
 */


/**
 * BEGINNING OF testDifferentWeightedVotingScenario1
 *
 * Test unequal weighted voting voting for official slate with abstentions.
 * Test updating description of candidates; ensure no changes to tally.
 * Test voting updates.
 */

const std::string testDifferentWeightedVotingScenario1GroupName =
      TEST_PREFIX + "Differently Weighted Votes Group 1";
const std::string testDifferentWeightedVotingScenario1GroupNameUpdated =
      TEST_PREFIX + "Differently Weighted Votes Group 1 (Updated)";
const std::string testDifferentWeightedVotingScenario1ContestName =
      "Contest for " + testDifferentWeightedVotingScenario1GroupName;
const std::string testDifferentWeightedVotingScenario1Contestant1Name = "Resolution 1";
const std::string testDifferentWeightedVotingScenario1Contestant2Name = "Resolution 2";
const std::string testDifferentWeightedVotingScenario1Contestant3Name = "Resolution 3";

void Pollaris::testDifferentWeightedVotingScenario1_Pre() {
    createDifferentWeightedGroup(testDifferentWeightedVotingScenario1GroupName);
    testDifferentWeightedVotingScenario_Pre(testDifferentWeightedVotingScenario1GroupName,
                                            testDifferentWeightedVotingScenario1ContestName);
}


/**
 * Create a polling group
 * - 5 voters
 *    - Voter 1: 5 weight
 *    - Voter 2: 3 weight
 *    - Voter 3: 2 weight
 *    - Voter 4: 2 weight
 *    - Voter 5: 2 weight
 */
void Pollaris::createDifferentWeightedGroup(const std::string& groupName) {
    // Verify that the group does not exist
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "Test group (" + groupName + ") should not exist at start of test");
    }


    ///
    /// Add voters
    ///
    BAL::Log("=> Adding voters to polling group");
    {
        BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
        BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
        BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
        BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
        BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

        const Tags emptyTags;
        uint32_t voterWeight;

        voterWeight = 5;
        addVoter(groupName, voter1, voterWeight, emptyTags);

        voterWeight = 3;
        addVoter(groupName, voter2, voterWeight, emptyTags);

        voterWeight = 2;
        addVoter(groupName, voter3, voterWeight, emptyTags);
        addVoter(groupName, voter4, voterWeight, emptyTags);
        addVoter(groupName, voter5, voterWeight, emptyTags);
    }
}


/**
 * Create a contest
 * - 1 contest with 3 contestants (abstention allowed).
 */
void Pollaris::testDifferentWeightedVotingScenario_Pre(const std::string& groupName,
                                                       const std::string& contestName,
                                                       const Tags& contestTags) {

    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting", contestName, ": Pre");


    ///
    /// Initialize values for the test
    ///
    const std::string contestDesc = contestName + ": Description";
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Verify the existence of the group and voter
    GroupId groupId;
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        BAL::Log("=> Checking polling group");
        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Verify(IsVoterPresent(*this, group->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter3),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter4),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter5),
                    "Test voter should have been found in the test group");

        groupId = group->id;
    }


    ///
    /// Create the contest
    ///
    BAL::Log("=> Creating contest");
    const Timestamp contestBegin = currentTime() + secondsOfDelayBeforeContestBegins;
    Timestamp contestEnd = contestBegin + contestDurationSecs;

    ContestantDescriptor c1;
    ContestantDescriptor c2;
    ContestantDescriptor c3;
    {
        set<ContestantDescriptor> contestants;

        c1.name = testDifferentWeightedVotingScenario1Contestant1Name;
        c1.description = "Description for " + c1.name;
        contestants.insert(c1);

        c2.name = testDifferentWeightedVotingScenario1Contestant2Name;
        c2.description = "Description for " + c2.name;
        contestants.insert(c2);

        c3.name = testDifferentWeightedVotingScenario1Contestant3Name;
        c3.description = "Description for " + c3.name;
        contestants.insert(c3);

        newContest(groupId, contestName, contestDesc, contestants,
                   contestBegin, contestEnd, contestTags);
    }

    // Verify the existence of the contest
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Verify the existence of the contestants
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    // No results are expected before the tally
    {
        BAL::Verify(IsResultsEmpty(*this, groupId, contestId),
                    "No results are expected for the test group");
    }

    // Tally
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    map<ContestantIdVariant, uint64_t> expectedTallies;
    expectedTallies.insert(std::make_pair(c1Id, 0));
    expectedTallies.insert(std::make_pair(c2Id, 0));
    expectedTallies.insert(std::make_pair(c3Id, 0));

    VerifyTallies(*this, groupId, resultId, expectedTallies);
}


void Pollaris::testDifferentWeightedVotingScenario1_During() {
    testDifferentWeightedVotingScenario1_During(testDifferentWeightedVotingScenario1GroupName,
                                                testDifferentWeightedVotingScenario1GroupNameUpdated,
                                                testDifferentWeightedVotingScenario1ContestName);
}

/**
 * - Stage 1: Initial Voting
 *     - Voter 1 votes 5 for Candidate 1
 *     - Voter 2 votes 3 for Candidate 3
 *     - Voter 3 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Voter 4 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Voter 5 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Tally should be:
 *            Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
 *            Candidate 2 = 0 + 0 + 1 + 1 + 1 = 3
 *            Candidate 3 = 0 + 3 + 1 + 1 + 1 = 6
 *
 * - Stage 2: Rename the group while it has members and after a contest has begun
 *     - Tally should be unchanged
 *
 * - Stage 3: Some voters update their votes
 *     - Voter 1 votes 4 for Candidate 1; votes 1 for Candidate 2
 *     - Voter 2 votes 2 for Candidate 1; votes 1 for Candidate 3
 *     - Voter 3-5 maintain their votes.
 *     - Tally should be:
 *            Candidate 1 = 4 + 2 + 0 + 0 + 0 = 6
 *            Candidate 2 = 1 + 0 + 1 + 1 + 1 = 4
 *            Candidate 3 = 0 + 1 + 1 + 1 + 1 = 4
 */
void Pollaris::testDifferentWeightedVotingScenario1_During(const std::string& groupName0,
                                                           const std::string& groupName1,
                                                           const std::string& contestName) {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting", contestName, ": During");


    ///
    /// Initialize values for the test
    ///
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName0, "Test group was NOT found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Stage 1
    /// - Voter 1 votes 5 for Candidate 1
    /// - Voter 2 votes 3 for Candidate 3
    /// - Voter 3 votes 1 for Candidate 2; votes 1 for Candidate 3
    /// - Voter 4 votes 1 for Candidate 2; votes 1 for Candidate 3
    /// - Voter 5 votes 1 for Candidate 2; votes 1 for Candidate 3
    ///
    BAL::Log("=> Stage 1 Voting");
    {
        FullOpinions opinions;

        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 3));
        setDecision(groupId, contestId, voter2, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 1));
        setDecision(groupId, contestId, voter3, opinions, emptyTags);
        setDecision(groupId, contestId, voter4, opinions, emptyTags);
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
    /// Candidate 2 = 0 + 0 + 1 + 1 + 1 = 3
    /// Candidate 3 = 0 + 3 + 1 + 1 + 1 = 3
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 5));
        expectedTallies.insert(std::make_pair(c2Id, 3));
        expectedTallies.insert(std::make_pair(c3Id, 6));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 2
    /// Rename the group while it has members and after a contest has begun
    ///
    {
        BAL::Log("=> Group Renaming #1");
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* ptrGroup0Before = FindGroup(groups, groupName0);
        BAL::Verify(ptrGroup0Before != nullptr,
                    "Test group should have been found before the update!");

        const PollingGroup* ptrGroup1Before = FindGroup(groups, groupName1);
        BAL::Verify(ptrGroup1Before == nullptr,
                    "Test group should NOT have been found before the update!");

        // Rename
        BAL::Verify(groupName0 != groupName1, "The test is invalid because the group names should differ for effective renaming of groups");
        renameGroup(groupName0, groupName1);

        // Verify the presence of the group with the updated name
        PollingGroups groupsRefresh = getTable<PollingGroups>(GLOBAL.value); // Refresh perception of groups
        const PollingGroup* ptrGroup1After = FindGroup(groupsRefresh, groupName1);
        BAL::Verify(ptrGroup1After != nullptr,
                    "Test group should have been found after the name update");

        // Verify the absence of the group with the original name
        const PollingGroup* ptrGroup0After = FindGroup(groupsRefresh, groupName0);
        BAL::Verify(ptrGroup0After == nullptr,
                    "Test group by the original name should NOT have been found after the name update");

        // Verify that the group IDs are the same
        BAL::Verify(ptrGroup0Before->id == ptrGroup1After->id,
                    "The ID of the group should not have changed when it was renamed");
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
    /// Candidate 2 = 0 + 0 + 1 + 1 + 1 = 3
    /// Candidate 3 = 0 + 3 + 1 + 1 + 1 = 3
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 5));
        expectedTallies.insert(std::make_pair(c2Id, 3));
        expectedTallies.insert(std::make_pair(c3Id, 6));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 3
    ///
    /// - Voter 1 votes 4 for Candidate 1; votes 1 for Candidate 2
    /// - Voter 2 votes 2 for Candidate 1; votes 1 for Candidate 3
    /// - Voter 3-5 maintain their votes.
    ///
    BAL::Log("=> Stage 3 Voting");
    {
        FullOpinions opinions;

        // Voter 1
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 4));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);

        // Voter 2
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 2));
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 1));
        setDecision(groupId, contestId, voter2, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 4 + 2 + 0 + 0 + 0 = 6
    /// Candidate 2 = 1 + 0 + 1 + 1 + 1 = 4
    /// Candidate 3 = 0 + 1 + 1 + 1 + 1 = 4
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 6));
        expectedTallies.insert(std::make_pair(c2Id, 4));
        expectedTallies.insert(std::make_pair(c3Id, 4));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}


/**
 * Tally the votes
 *
 * Expected vote counts:
 * Candidate 1 = 6
 * Candidate 2 = 4
 * Candidate 3 = 4
 */
void Pollaris::testDifferentWeightedVotingScenario1_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting", testDifferentWeightedVotingScenario1GroupName, ": Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testDifferentWeightedVotingScenario1GroupNameUpdated;
    const std::string contestName = testDifferentWeightedVotingScenario1ContestName;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 6));
        expectedTallies.insert(std::make_pair(c2Id, 4));
        expectedTallies.insert(std::make_pair(c3Id, 4));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}

/**
 * END OF testDifferentWeightedVotingScenario1
 */


/**
 * BEGINNING OF testDifferentWeightedVotingScenario2
 *
 * Test unequal weighted voting voting for official slate with abstentions.
 * Test shifting of weight to write-in candidates.
 */

const std::string testDifferentWeightedVotingScenario2GroupName =
      TEST_PREFIX + "Write-In Votes Scenario";
const std::string testDifferentWeightedVotingScenario2ContestName =
      "Contest for " + testDifferentWeightedVotingScenario2GroupName;
const std::string testDifferentWeightedVotingScenario2Contestant1Name =
                    testDifferentWeightedVotingScenario1Contestant1Name;
const std::string testDifferentWeightedVotingScenario2Contestant2Name =
                    testDifferentWeightedVotingScenario1Contestant2Name;
const std::string testDifferentWeightedVotingScenario2Contestant3Name =
                    testDifferentWeightedVotingScenario1Contestant3Name;
const std::string testDifferentWeightedVotingScenario2Contestant4Name = "Write-in Resolution 4";
const std::string testDifferentWeightedVotingScenario2Contestant5Name = "Write-in Resolution 5";


/**
 * Same as Scenario 1
 */
void Pollaris::testDifferentWeightedVotingScenario2_Pre() {
    createDifferentWeightedGroup(testDifferentWeightedVotingScenario2GroupName);
    testDifferentWeightedVotingScenario_Pre(testDifferentWeightedVotingScenario2GroupName,
                                            testDifferentWeightedVotingScenario2ContestName);
}


void Pollaris::testDifferentWeightedVotingScenario2_During() {
    testDifferentWeightedVotingScenario2_During(testDifferentWeightedVotingScenario2GroupName,
                                                testDifferentWeightedVotingScenario2ContestName);
}

/**
 * - Stage 1: Initial Voting
 *     - Voter 1 votes 5 for Candidate 1
 *     - Voter 2 votes 3 for Candidate 3
 *     - Voter 3 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Voter 4 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Voter 5 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Tally should be:
 *            Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
 *            Candidate 2 = 0 + 0 + 1 + 1 + 1 = 3
 *            Candidate 3 = 0 + 3 + 1 + 1 + 1 = 6
 *
 * - Stage 2: Voters 3 shift weight to write-in candidate
 *    - Voter 3 votes 2 for Write-in Candidate 4
 *    - Voter 4 votes 2 for Write-in Candidate 4
 *    - Voter 5 votes 2 for Write-in Candidate 4
 *    - Tally should be:
 *            Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
 *            Candidate 2 = 0 + 0 + 0 + 0 + 0 = 0
 *            Candidate 3 = 0 + 3 + 0 + 0 + 0 = 3
 *            Candidate 4 = 0 + 0 + 2 + 2 + 2 = 6
 *
 * - Stage 3: Voters 1 and 2 should shift some of their weight to Write-In Candidate 5
 *    - Voter 1 votes 1 for Candidate 1, votes 3 for Write-in Candidate 5, and abstains with 1 weight
 *    - Voter 2 votes 3 for Write-in Candidate 5
 *    - Tally should be:
 *            Candidate 1 = 1 + 0 + 0 + 0 + 0 = 1
 *            Candidate 2 = 0 + 0 + 0 + 0 + 0 = 0
 *            Candidate 3 = 0 + 0 + 0 + 0 + 0 = 0
 *            Candidate 4 = 0 + 0 + 2 + 2 + 2 = 6
 *            Candidate 5 = 3 + 3 + 0 + 0 + 0 = 6
 *            Abstention = 1
 */
void Pollaris::testDifferentWeightedVotingScenario2_During(const std::string& groupName,
                                                           const std::string& contestName) {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting", contestName, ": During");


    ///
    /// Initialize values for the test
    ///
    const std::string groupName0 = groupName;
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }


    ///
    /// Stage 1
    /// - Voter 1 votes 5 for Candidate 1
    /// - Voter 2 votes 3 for Candidate 3
    /// - Voter 3 votes 1 for Candidate 2; votes 1 for Candidate 3
    /// - Voter 4 votes 1 for Candidate 2; votes 1 for Candidate 3
    /// - Voter 5 votes 1 for Candidate 2; votes 1 for Candidate 3
    ///
    BAL::Log("=> Stage 1 Voting");
    {
        FullOpinions opinions;

        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));
        setDecision(groupId, contestId, voter1, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 3));
        setDecision(groupId, contestId, voter2, opinions, emptyTags);

        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 1));
        setDecision(groupId, contestId, voter3, opinions, emptyTags);
        setDecision(groupId, contestId, voter4, opinions, emptyTags);
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
    /// Candidate 2 = 0 + 0 + 1 + 1 + 1 = 3
    /// Candidate 3 = 0 + 3 + 1 + 1 + 1 = 3
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 5));
        expectedTallies.insert(std::make_pair(c2Id, 3));
        expectedTallies.insert(std::make_pair(c3Id, 6));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 2
    /// - Voters 3-5 vote 2 for Write-In Candidate 4
    ///
    BAL::Log("=> Stage 2 Voting");
    {
        ContestantDescriptor c4Descriptor;
        c4Descriptor.name = testDifferentWeightedVotingScenario2Contestant4Name;
        c4Descriptor.description = "Description for " + c4Descriptor.name;

        FullOpinions opinions;
        opinions.writeInOpinions.insert(std::make_pair(c4Descriptor, 2));
        setDecision(groupId, contestId, voter3, opinions, emptyTags);
        setDecision(groupId, contestId, voter4, opinions, emptyTags);
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }

    // Verify the existence of the contestants
    WriteInId c4Id;
    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);
        const WriteInId* ptrC4Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant4Name);
        BAL::Verify(ptrC4Id != nullptr, "Write-in Contestant 4 should have been found");
        c4Id = *ptrC4Id;
    }

    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 5 + 0 + 0 + 0 + 0 = 5
    /// Candidate 2 = 0 + 0 + 0 + 0 + 0 = 0
    /// Candidate 3 = 0 + 3 + 0 + 0 + 0 = 3
    /// Candidate 4 = 0 + 0 + 2 + 2 + 2 = 6
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 5));
        expectedTallies.insert(std::make_pair(c2Id, 0));
        expectedTallies.insert(std::make_pair(c3Id, 3));
        expectedTallies.insert(std::make_pair(c4Id, 6));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 3
    /// - Voter 1 votes 1 for Candidate 1, votes 3 for Candidate 5, and abstains with 1 weight
    /// - Voter 2 votes 3 for Write-in Candidate 5
    ///
    BAL::Log("=> Stage 3 Voting");
    {
        // Voter 1
        ContestantDescriptor c5Descriptor;
        c5Descriptor.name = testDifferentWeightedVotingScenario2Contestant5Name;
        c5Descriptor.description = "Description for " + c5Descriptor.name;

        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 1));
        opinions.writeInOpinions.insert(std::make_pair(c5Descriptor, 3));
        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "1");
        setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);

        // Voter 2
        opinions = FullOpinions();
        opinions.writeInOpinions.insert(std::make_pair(c5Descriptor, 3));
        setDecision(groupId, contestId, voter2, opinions, emptyTags);
    }


    // Verify the existence of the contestants
    WriteInId c5Id;
    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);
        const WriteInId* ptrC5Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant5Name);
        BAL::Verify(ptrC5Id != nullptr, "Write-in Contestant 5 should have been found");
        c5Id = *ptrC5Id;
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 1 + 0 + 0 + 0 + 0 = 1
    /// Candidate 2 = 0 + 0 + 0 + 0 + 0 = 0
    /// Candidate 3 = 0 + 0 + 0 + 0 + 0 = 0
    /// Candidate 4 = 0 + 0 + 2 + 2 + 2 = 6
    /// Candidate 5 = 3 + 3 + 0 + 0 + 0 = 6
    /// Abstention = 1
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 1));
        expectedTallies.insert(std::make_pair(c2Id, 0));
        expectedTallies.insert(std::make_pair(c3Id, 0));
        expectedTallies.insert(std::make_pair(c4Id, 6));
        expectedTallies.insert(std::make_pair(c5Id, 6));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}


/**
 * Stage 1: Tally the votes
 *
 * Expected vote counts:
 * Candidate 1 = 1 + 0 + 0 + 0 + 0 = 1
 * Candidate 2 = 0 + 0 + 0 + 0 + 0 = 0
 * Candidate 3 = 0 + 0 + 0 + 0 + 0 = 0
 * Candidate 4 = 0 + 0 + 2 + 2 + 2 = 6
 * Candidate 5 = 3 + 3 + 0 + 0 + 0 = 6
 * Abstention = 1
 *
 * Stage 2: Test the deletion of a contest
 * Test the deletion of a contest containing a write-in decisions,
 * and verify the disappearance of all related data.
 */
void Pollaris::testDifferentWeightedVotingScenario2_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting", testDifferentWeightedVotingScenario2GroupName, ": Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testDifferentWeightedVotingScenario2GroupName;
    const std::string contestName = testDifferentWeightedVotingScenario2ContestName;


    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }

    WriteInId c4Id, c5Id;
    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);

        const WriteInId* ptrC4Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant4Name);
        BAL::Verify(ptrC4Id != nullptr, "Write-in Contestant 4 should have been found");
        c4Id = *ptrC4Id;

        const WriteInId* ptrC5Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant5Name);
        BAL::Verify(ptrC5Id != nullptr, "Write-in Contestant 5 should have been found");
        c5Id = *ptrC5Id;
    }


    ///
    /// Stage 1: Tally
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 1));
        expectedTallies.insert(std::make_pair(c2Id, 0));
        expectedTallies.insert(std::make_pair(c3Id, 0));
        expectedTallies.insert(std::make_pair(c4Id, 6));
        expectedTallies.insert(std::make_pair(c5Id, 6));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }


    ///
    /// Stage 2: Delete the contest
    ///
    // Find all contest tallies before deleting the contest
    set<ResultId> contestResults;
    {
        Results r = getTable<Results>(groupId);
        auto resultsByContest = r.getSecondaryIndex<BY_CONTEST>();
        auto resultsRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestId),
                                                   Pollaris::Result::contestKeyMax(contestId));
        BAL::Verify(resultsRange.first != resultsRange.second, "No results were found for the contest!");

        for (auto itr = resultsRange.first; itr != resultsRange.second; itr++) {
            contestResults.insert(itr->id);
        }
    }

    // Delete the contest
    BAL::Log("=> Deleting the contest");
    {
        deleteContest(groupId, contestId);
    }

    // Verify the absence of the contest
    {
        const ContestId* ptrContestId = SeekContestId(*this, groupId, contestName);
        BAL::Verify(ptrContestId == nullptr, "The deleted contest should NOT have been found!");
    }


    // Verify the absence of the contestant IDs
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant1Name);
        BAL::Verify(ptrC1Id == nullptr, "Contestant 1 should NOT have been found!");

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant2Name);
        BAL::Verify(ptrC2Id == nullptr, "Contestant 2 should NOT have been found!");

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant3Name);
        BAL::Verify(ptrC3Id == nullptr, "Contestant 3 should NOT have been found!");
    }

    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);

        const WriteInId* ptrC4Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant4Name);
        BAL::Verify(ptrC4Id == nullptr, "Write-in Contestant 4 should NOT have been found!");

        const WriteInId* ptrC5Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario2Contestant5Name);
        BAL::Verify(ptrC5Id == nullptr, "Write-in Contestant 5 should NOT have been found!");
    }

    // Verify the absence of decisions
    BAL::Verify(IsDecisionsEmpty(*this, groupId, contestId),
                "Contest decisions should NOT have been found!");

    // Verify the absence of results
    BAL::Verify(IsResultsEmpty(*this, groupId, contestId),
                "Contest results should NOT have been found!");

    // Verify the absence of tallies
    BAL::Verify(IsTalliesEmpty(*this, groupId, contestResults),
                "Contest tallies should NOT have been found!");
}

/**
 * END OF testDifferentWeightedVotingScenario2
 */


/**
 * BEGINNING OF testDifferentWeightedVotingScenario3
 */

const std::string testDifferentWeightedVotingScenario3Contestant1Name =
                    testDifferentWeightedVotingScenario1Contestant1Name;
const std::string testDifferentWeightedVotingScenario3Contestant2Name =
                    testDifferentWeightedVotingScenario1Contestant2Name;
const std::string testDifferentWeightedVotingScenario3Contestant3Name =
                    testDifferentWeightedVotingScenario1Contestant3Name;
const std::string testDifferentWeightedVotingScenario3Contestant4Name = "Write-in Resolution 4";

/**
 * - Stage 1: Initial Voting
 *     - Voter 1 abstains without a formal vote
 *     - Voter 2 votes 1 for Candidate 2, 1 for (Write-in) Candidate 4, and partially abstains 1
 *     - Voter 3 votes 2 for Candidate 2
 *     - Voter 4 votes 2 for Candidate 3
 *     - Voter 5 votes 1 for Candidate 2; votes 1 for Candidate 3
 *     - Tally should be:
 *            Candidate 1 = 0 + 0 + 0 + 0 + 0 = 0
 *            Candidate 2 = 0 + 1 + 2 + 0 + 1 = 4
 *            Candidate 3 = 0 + 0 + 0 + 2 + 1 = 3
 *            Candidate 4 = 0 + 1 + 0 + 0 + 0 = 1
 *            Abstentions = 5 + 1 + 0 + 0 + 0 = 6
 */
void Pollaris::testDifferentWeightedVotingScenario3_During(const std::string& groupName,
                                                           const std::string& contestName) {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting", contestName, ": During");


    ///
    /// Initialize values for the test
    ///
    const std::string groupName0 = groupName;
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id, c3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario3Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario3Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario3Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        c3Id = *ptrC3Id;
    }

    ///
    /// Stage 1
    /// Voter 1 abstains without a formal vote
    /// Voter 2 votes 1 for Candidate 2, 1 for (Write-in) Candidate 4, and partially abstains 1
    /// Voter 3 votes 2 for Candidate 2
    /// Voter 4 votes 2 for Candidate 3
    /// Voter 5 votes 1 for Candidate 2; votes 1 for Candidate 3
    ///
    BAL::Log("=> Stage 1 Voting");
    {
        FullOpinions opinions;

        // Voter 1
        // Does nothing

        // Voter 2
        ContestantDescriptor c4Descriptor;
        c4Descriptor.name = testDifferentWeightedVotingScenario3Contestant4Name;
        c4Descriptor.description = "Description for " + c4Descriptor.name;

        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        opinions.writeInOpinions.insert(std::make_pair(c4Descriptor, 1));
        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "1");
        setDecision(groupId, contestId, voter2, opinions, partialAbstentionTags);

        // Voter 3
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 2));
        setDecision(groupId, contestId, voter3, opinions, emptyTags);

        // Voter 4
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 2));
        setDecision(groupId, contestId, voter4, opinions, emptyTags);

        // Voter 5
        opinions = FullOpinions();
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));
        opinions.contestantOpinions.insert(std::make_pair(c3Id, 1));
        setDecision(groupId, contestId, voter5, opinions, emptyTags);
    }


    ///
    /// Tally
    ///
    /// Expected vote counts:
    /// Candidate 1 = 0 + 0 + 0 + 0 + 0 = 0
    /// Candidate 2 = 0 + 1 + 2 + 0 + 1 = 4
    /// Candidate 3 = 0 + 0 + 0 + 2 + 1 = 3
    /// Candidate 4 = 0 + 1 + 0 + 0 + 0 = 1
    /// Abstentions = 5 + 1 + 0 + 0 + 0 = 6
    ///
    BAL::Log("=> Tallying");
    {
        tallyContest(groupId, contestId);
    }

    // Find the relevant result
    ResultId resultId;
    {
        Results r = getTable<Results>(groupId);
        const ResultId* ptrResultId = SeekNewestResultId(r, contestId);
        BAL::Verify(ptrResultId != nullptr, "A tally result should have been found!");
        resultId = *ptrResultId;
    }

    // Find the write-in candidate
    WriteInId c4Id;
    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);
        const WriteInId* ptrC4Id = SeekWriteInContestant(groupContestants, contestId,
                                        testDifferentWeightedVotingScenario3Contestant4Name);
        BAL::Verify(ptrC4Id != nullptr, "Write-in Contestant 4 should have been found!");
        c4Id = *ptrC4Id;
    }

    // Verify tallies
    {
        map<ContestantIdVariant, uint64_t> expectedTallies;
        expectedTallies.insert(std::make_pair(c1Id, 0));
        expectedTallies.insert(std::make_pair(c2Id, 4));
        expectedTallies.insert(std::make_pair(c3Id, 3));
        expectedTallies.insert(std::make_pair(c4Id, 1));

        VerifyTallies(*this, groupId, resultId, expectedTallies);
    }
}

/**
 * END OF testDifferentWeightedVotingScenario3
 */


/**
 * BEGINNING OF testContestDeletions1
 *
 * Test the deletion of a contests and verify its deletion does not affect other contests
 * in the polling group
 */
const std::string testContestDeletions1InitialGroupName =
      TEST_PREFIX + "Contest Deletions Group";
const std::string testContestDeletions1UpdatedGroupName =
      TEST_PREFIX + "Contest Deletions Group (Updated)";
const std::string testContestDeletions1ContestAName =
      "Contest A for " + testContestDeletions1InitialGroupName;
const std::string testContestDeletions1ContestBName =
      "Contest B for " + testContestDeletions1InitialGroupName;
const std::string testContestDeletions1ContestCName =
      "Contest C for " + testContestDeletions1InitialGroupName;

/**
 * Create three different contests for the group
 * Contests A, B, and C will have the same initialization except for their different contest names
 */
void Pollaris::testContestDeletions1_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Contest Deletions Scenario 1: Pre");

    createDifferentWeightedGroup(testContestDeletions1InitialGroupName);

    testDifferentWeightedVotingScenario_Pre(testContestDeletions1InitialGroupName,
                                            testContestDeletions1ContestAName);
    testDifferentWeightedVotingScenario_Pre(testContestDeletions1InitialGroupName,
                                            testContestDeletions1ContestBName);
    testDifferentWeightedVotingScenario_Pre(testContestDeletions1InitialGroupName,
                                            testContestDeletions1ContestCName);
}


/**
 * The three contests will proceed through different voting scenarios
 */
void Pollaris::testContestDeletions1_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Contest Deletions Scenario 1: During");

    // Contest A will proceed through testDifferentWeightedVotingScenario2
    // which involves multiple write-in candidates
    testDifferentWeightedVotingScenario2_During(testContestDeletions1InitialGroupName,
                                                testContestDeletions1ContestAName);

    // Contest B will proceed through testDifferentWeightedVotingScenario1
    // which involves no write-in candidates
    testDifferentWeightedVotingScenario1_During(testContestDeletions1InitialGroupName,
                                                testContestDeletions1UpdatedGroupName,
                                                testContestDeletions1ContestBName);

    // Contest C will proceed through testDifferentWeightedVotingScenario3
    // which involves a single write-in candidate
    testDifferentWeightedVotingScenario3_During(testContestDeletions1UpdatedGroupName,
                                                testContestDeletions1ContestCName);
}


/**
 * Progressively delete the contests while retaining the content of other contests
 *
 * Contest A = testDifferentWeightedVotingScenario2
 * Contest B = testDifferentWeightedVotingScenario1
 * Contest C = testDifferentWeightedVotingScenario3
 */
void Pollaris::testContestDeletions1_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Contest Deletions Scenario 1: Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testContestDeletions1UpdatedGroupName;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    ///
    /// Stage 1: Delete Contest A
    ///
    // Find the contest ID
    ContestId contestAId = FindContestId(*this, groupId, testContestDeletions1ContestAName,
                                        "Contest A should have been found!");

    // Find the contestant IDs
    ContestantId cA1Id, cA2Id, cA3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        cA1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        cA2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        cA3Id = *ptrC3Id;
    }

    WriteInId cA4Id, cA5Id;
    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);

        const WriteInId* ptrC4Id = SeekWriteInContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant4Name);
        BAL::Verify(ptrC4Id != nullptr, "Write-in Contestant 4 should have been found");
        cA4Id = *ptrC4Id;

        const WriteInId* ptrC5Id = SeekWriteInContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant5Name);
        BAL::Verify(ptrC5Id != nullptr, "Write-in Contestant 5 should have been found");
        cA5Id = *ptrC5Id;
    }

    // Find all contest tallies before deleting the contest
    set<ResultId> contestResults;
    {
        Results r = getTable<Results>(groupId);
        auto resultsByContest = r.getSecondaryIndex<BY_CONTEST>();
        auto resultsRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestAId),
                                                   Pollaris::Result::contestKeyMax(contestAId));
        BAL::Verify(resultsRange.first != resultsRange.second, "No results were found for the contest!");

        for (auto itr = resultsRange.first; itr != resultsRange.second; itr++) {
            contestResults.insert(itr->id);
        }
    }

    // Delete the contest
    BAL::Log("=> Deleting Contest A");
    {
        deleteContest(groupId, contestAId);
    }

    // Verify the absence of the contest
    {
        const ContestId* ptrContestId = SeekContestId(*this, groupId, testContestDeletions1ContestAName);
        BAL::Verify(ptrContestId == nullptr, "The deleted contest should NOT have been found!");
    }

    // Verify the absence of the contestant IDs
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant1Name);
        BAL::Verify(ptrC1Id == nullptr, "Contestant 1 should NOT have been found!");

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant2Name);
        BAL::Verify(ptrC2Id == nullptr, "Contestant 2 should NOT have been found!");

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant3Name);
        BAL::Verify(ptrC3Id == nullptr, "Contestant 3 should NOT have been found!");
    }
    {
        WriteIns groupContestants = getTable<WriteIns>(groupId);
        const WriteInId* ptrC4Id = SeekWriteInContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant4Name);
        BAL::Verify(ptrC4Id == nullptr, "Write-in Contestant 4 should NOT have been found!");

        const WriteInId* ptrC5Id = SeekWriteInContestant(groupContestants, contestAId,
                                        testDifferentWeightedVotingScenario2Contestant5Name);
        BAL::Verify(ptrC5Id == nullptr, "Write-in Contestant 5 should NOT have been found!");
    }

    // Verify the absence of decisions
    BAL::Verify(IsDecisionsEmpty(*this, groupId, contestAId),
                "Contest decisions should NOT have been found!");

    // Verify the absence of results
    BAL::Verify(IsResultsEmpty(*this, groupId, contestAId),
                "Contest results should NOT have been found!");

    // Verify the absence of tallies
    BAL::Verify(IsTalliesEmpty(*this, groupId, contestResults),
                "Contest tallies should NOT have been found!");


    ///
    /// Stage 2: Delete Contest C
    ///
    // Find the contest ID
    ContestId contestCId = FindContestId(*this, groupId, testContestDeletions1ContestCName,
                                        "Contest C should have been found!");


    // Find the contestant IDs
    ContestantId cC1Id, cC2Id, cC3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestCId,
                                        testDifferentWeightedVotingScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        cC1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestCId,
                                        testDifferentWeightedVotingScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        cC2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestCId,
                                        testDifferentWeightedVotingScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        cC3Id = *ptrC3Id;
    }

    // Find all contest tallies before deleting the contest
    contestResults.clear();
    {
        Results r = getTable<Results>(groupId);
        auto resultsByContest = r.getSecondaryIndex<BY_CONTEST>();
        auto resultsRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestCId),
                                                   Pollaris::Result::contestKeyMax(contestCId));
        BAL::Verify(resultsRange.first != resultsRange.second, "No results were found for the contest!");

        for (auto itr = resultsRange.first; itr != resultsRange.second; itr++) {
            contestResults.insert(itr->id);
        }
    }

    // Delete the contest
    BAL::Log("=> Deleting Contest C");
    {
        deleteContest(groupId, contestCId);
    }

    // Verify the absence of the contest
    {
        const ContestId* ptrContestId = SeekContestId(*this, groupId, testContestDeletions1ContestCName);
        BAL::Verify(ptrContestId == nullptr, "The deleted contest should NOT have been found!");
    }

    // Verify the absence of decisions
    BAL::Verify(IsDecisionsEmpty(*this, groupId, contestCId),
                "Contest decisions should NOT have been found!");

    // Verify the absence of results
    BAL::Verify(IsResultsEmpty(*this, groupId, contestCId),
                "Contest results should NOT have been found!");

    // Verify the absence of tallies
    BAL::Verify(IsTalliesEmpty(*this, groupId, contestResults),
                "Contest tallies should NOT have been found!");


    ///
    /// Stage 3: Delete Contest B
    ///
    // Find the contest ID
    ContestId contestBId = FindContestId(*this, groupId, testContestDeletions1ContestBName,
                                        "Contest B should have been found!");

    // Find the contestant IDs
    ContestantId cB1Id, cB2Id, cB3Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestBId,
                                        testDifferentWeightedVotingScenario1Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        cB1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestBId,
                                        testDifferentWeightedVotingScenario1Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        cB2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestBId,
                                        testDifferentWeightedVotingScenario1Contestant3Name);
        BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
        cB3Id = *ptrC3Id;
    }

    // Find all contest tallies before deleting the contest
    contestResults.clear();
    {
        Results r = getTable<Results>(groupId);
        auto resultsByContest = r.getSecondaryIndex<BY_CONTEST>();
        auto resultsRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestBId),
                                                   Pollaris::Result::contestKeyMax(contestBId));
        BAL::Verify(resultsRange.first != resultsRange.second, "No results were found for the contest!");

        for (auto itr = resultsRange.first; itr != resultsRange.second; itr++) {
            contestResults.insert(itr->id);
        }
    }

    // Delete the contest
    BAL::Log("=> Deleting Contest B");
    {
        deleteContest(groupId, contestBId);
    }

    // Verify the absence of the contest
    {
        const ContestId* ptrContestId = SeekContestId(*this, groupId, testContestDeletions1ContestBName);
        BAL::Verify(ptrContestId == nullptr, "The deleted contest should NOT have been found!");
    }

    // Verify the absence of decisions
    BAL::Verify(IsDecisionsEmpty(*this, groupId, contestBId),
                "Contest decisions should NOT have been found!");

    // Verify the absence of results
    BAL::Verify(IsResultsEmpty(*this, groupId, contestBId),
                "Contest results should NOT have been found!");

    // Verify the absence of tallies
    BAL::Verify(IsTalliesEmpty(*this, groupId, contestResults),
                "Contest tallies should NOT have been found!");


    ///
    /// Verify the absence of any data related to the polling group
    ///
    {
        // Verify the absence of any contests
        Contests c = getTable<Contests>(groupId);
        BAL::Verify(c.begin() == c.end(), "No Contests should NOT have been found!");

        // Verify the absence of any contestants
        Contestants cx = getTable<Contestants>(groupId);
        BAL::Verify(cx.begin() == cx.end(), "No Contestants should NOT have been found!");

        // Verify the absence of any write-ins
        WriteIns w = getTable<WriteIns>(groupId);
        BAL::Verify(w.begin() == w.end(), "No Write-Ins should NOT have been found!");

        // Verify the absence of any decisions
        Decisions d = getTable<Decisions>(groupId);
        BAL::Verify(d.begin() == d.end(), "No Decisions should NOT have been found!");

        // Verify the absence of any results
        Results r = getTable<Results>(groupId);
        BAL::Verify(r.begin() == r.end(), "No Results should NOT have been found!");

        // Verify the absence of any tallies
        Tallies t = getTable<Pollaris::Tallies>(groupId);
        BAL::Verify(t.begin() == t.end(), "No Tallies should NOT have been found!");
    }
}

/**
 * END OF testContestDeletions1
 */


#if 0 // This is disabled until a backend which supports exceptions is available again
/**
 * BEGINNING OF Rejection Detection test suite
 */

/**
 * Verify the basic mechanism of rejection detection
 */
void Pollaris::testRejectionDetection() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Basic mechanism of rejection detection");

    bool rejectionDetected = false;
    try {
        BAL::Verify(false, "Testing rejection detection");
    } catch (fc::assert_exception&) {
        rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "The basic mechanism of rejection detection was NOT verified!");
    BAL::Log("Test PASSED");
}


/**
 * BEGINNING OF Test rejection of early and late voting
 */
const std::string testRejectPreVoting_GroupName =
      TEST_PREFIX + "Reject Early and Late Voting Group 1";
const std::string testRejectPreVoting_ContestName =
      "Contest for " + testRejectPreVoting_GroupName;
const std::string testRejectPreVoting_Contestant1Name = testDifferentWeightedVotingScenario1Contestant1Name;

void Pollaris::testRejectEarlyAndLateVoting_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectPreVoting_ContestName + ": Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectPreVoting_GroupName;
    const std::string contestName = testRejectPreVoting_ContestName;
    const std::string c1Name = testRejectPreVoting_Contestant1Name;

    createDifferentWeightedGroup(groupName);
    testDifferentWeightedVotingScenario_Pre(groupName,
                                            contestName);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "Contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        c1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;
    }

    // Find the voter
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);


    ///
    /// Attempt to vote immediately after creation of the contest
    ///
    BAL::Log("=> Voting before Voting Period");
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));
        const Tags emptyTags;
        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected, "Rejection of pre-voting was not detected!");
        BAL::Log("Test PASSED: Rejection detected");
    }
}


void Pollaris::testRejectEarlyAndLateVoting_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectPreVoting_ContestName + ": Post");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectPreVoting_GroupName;
    const std::string contestName = testRejectPreVoting_ContestName;
    const std::string c1Name = testRejectPreVoting_Contestant1Name;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "Contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        c1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;
    }

    // Find the voter
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);


    ///
    /// Attempt to vote after the voting period
    ///
    BAL::Log("=> Voting after Voting Period");
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));
        const Tags emptyTags;
        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected, "Rejection of pre-voting was not detected!");
        BAL::Log("Test PASSED: Rejection detected");
    }
}

/**
 * END OF Test rejection of early and late voting
 */


/**
 * BEGINNING OF Test rejection of abstention when abstention is prohibited
 */

const std::string testRejectAbstainingWhenProhibited_GroupName =
      TEST_PREFIX + "Reject Abstaining When Prohibited Group 1";
const std::string testRejectAbstainingWhenProhibited_ContestName =
      "Contest for " + testRejectAbstainingWhenProhibited_GroupName;
const std::string testRejectAbstainingWhenProhibited_Contestant1Name = testDifferentWeightedVotingScenario1Contestant1Name;

void Pollaris::testRejectAbstentionProhibition_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectAbstainingWhenProhibited_ContestName + ": Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectAbstainingWhenProhibited_GroupName;
    const std::string contestName = testRejectAbstainingWhenProhibited_ContestName;

    createDifferentWeightedGroup(groupName);

    Tags abstentionProhibitedTags;
    abstentionProhibitedTags.insert(NO_ABSTAIN_TAG);
    testDifferentWeightedVotingScenario_Pre(groupName, contestName, abstentionProhibitedTags);
}


void Pollaris::testRejectAbstentionProhibition_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectAbstainingWhenProhibited_ContestName + ": During");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectAbstainingWhenProhibited_GroupName;
    const std::string contestName = testRejectAbstainingWhenProhibited_ContestName;
    const std::string c1Name = testRejectAbstainingWhenProhibited_Contestant1Name;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "Contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        c1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;
    }

    // Find the voter
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);


    ///
    /// Attempt to vote with abstention
    ///
    BAL::Log("=> Attempting to vote with abstentions");

    // Attempt full abstention
    {
        FullOpinions opinions;

        Tags fullAbstentionTags;
        fullAbstentionTags.insert(ABSTAIN_VOTE_TAG);

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, fullAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected, "Rejection of prohibition of abstention was not detected!");
    }

    // Attempt full abstention confused with voting
    {
        FullOpinions opinions;

        Tags fullAbstentionTags;
        fullAbstentionTags.insert(ABSTAIN_VOTE_TAG);

        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, fullAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of prohibition of confused full abstention was not detected!");
    }

    // Attempt partial abstention
    {
        FullOpinions opinions;

        opinions.contestantOpinions.insert(std::make_pair(c1Id, 2));
        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "3");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of prohibition of partial abstention was not detected!");
    }

    BAL::Log("Rejections have been properly detected");


    ///
    /// Verify valid voting
    ///
    BAL::Log("=> Verifying non-abstention voting");

    // Attempt full-weighted voting without any tags
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));

        const Tags emptyTags;

        setDecision(groupId, contestId, voter1, opinions, emptyTags);
    }

    BAL::Log("Test PASSED");
}

/**
 * END OF Test rejection of abstention when abstention is prohibited
 */


/**
 * BEGINNING OF Test rejection of split-voting when split-voting is prohibited
 */

const std::string testRejectSplitVotingWhenProhibited_GroupName =
      TEST_PREFIX + "Reject Split-Voting When Prohibited Group 1";
const std::string testRejectSplitVotingWhenProhibited_ContestName =
      "Contest for " + testRejectSplitVotingWhenProhibited_GroupName;
const std::string testRejectSplitVotingWhenProhibited_Contestant1Name = testDifferentWeightedVotingScenario1Contestant1Name;
const std::string testRejectSplitVotingWhenProhibited_Contestant2Name = testDifferentWeightedVotingScenario1Contestant2Name;

void Pollaris::testRejectSplitVoteProhibition_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectSplitVotingWhenProhibited_ContestName + ": Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectSplitVotingWhenProhibited_GroupName;
    const std::string contestName = testRejectSplitVotingWhenProhibited_ContestName;

    createDifferentWeightedGroup(groupName);

    Tags splitProhibitionTags;
    splitProhibitionTags.insert(NO_SPLIT_TAG);
    testDifferentWeightedVotingScenario_Pre(groupName, contestName, splitProhibitionTags);
}


void Pollaris::testRejectSplitVoteProhibition_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectSplitVotingWhenProhibited_ContestName + ": During");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectSplitVotingWhenProhibited_GroupName;
    const std::string contestName = testRejectSplitVotingWhenProhibited_ContestName;
    const std::string c1Name = testRejectSplitVotingWhenProhibited_Contestant1Name;
    const std::string c2Name = testRejectSplitVotingWhenProhibited_Contestant2Name;
    const Tags emptyTags;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "Contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        c1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        c2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }

    // Find the voter
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);


    ///
    /// Attempt to split-vote
    ///
    BAL::Log("=> Attempting to vote with split-voting");

    // Attempt split-voting
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 2));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of split-voting under split-voting prohibition was NOT detected!");
    }

    // Attempt full-abstention which is also prohibited under split-voting
    {
        FullOpinions opinions;

        Tags fullAbstentionTags;
        fullAbstentionTags.insert(ABSTAIN_VOTE_TAG);

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, fullAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of full-abstention voting ") + \
                    "under split-voting prohibition was NOT detected!");

    }

    // Attempt partial-abstention which is also prohibited under split-voting
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));

        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "2");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of partial-abstention voting ") +\
                    "under split-voting prohibition was NOT detected!");

    }

    // Attempt full-weighted voting with partial-abstention tags set to zero
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 5));

        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "0");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of full-weighted + zero partial-abstention voting ") + \
                    "under split-voting prohibition was NOT detected!");
    }

    BAL::Log("Rejections have been properly detected");


    ///
    /// Verify valid voting
    ///
    BAL::Log("=> Verifying full-weighted voting");

    // Attempt full-weighted voting without any tags
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 5));

        setDecision(groupId, contestId, voter1, opinions, emptyTags);
    }

    BAL::Log("Test PASSED");
}

/**
 * END OF Test rejection of split-voting when split-voting is prohibited
 */


/**
 * BEGINNING OF Test rejection of invalid voting
 */

const std::string testRejectInvalidVoting_GroupName =
      TEST_PREFIX + "Reject Invalid Voting Group 1";
const std::string testRejectInvalidVoting_ContestName =
      "Contest for " + testRejectInvalidVoting_GroupName;
const std::string testRejectInvalidVoting_Contestant1Name = testDifferentWeightedVotingScenario1Contestant1Name;
const std::string testRejectInvalidVoting_Contestant2Name = testDifferentWeightedVotingScenario1Contestant2Name;

void Pollaris::testRejectInvalidVoting_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectInvalidVoting_ContestName + ": Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectInvalidVoting_GroupName;
    const std::string contestName = testRejectInvalidVoting_ContestName;

    createDifferentWeightedGroup(groupName);

    Tags noProhibitionTags;

    testDifferentWeightedVotingScenario_Pre(groupName, contestName, noProhibitionTags);
}


void Pollaris::testRejectInvalidVoting_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectInvalidVoting_ContestName + ": During");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectInvalidVoting_GroupName;
    const std::string contestName = testRejectInvalidVoting_ContestName;
    const std::string c1Name = testRejectInvalidVoting_Contestant1Name;
    const std::string c2Name = testRejectInvalidVoting_Contestant2Name;
    const Tags emptyTags;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "Contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        c1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        c2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;
    }

    // Find the voter
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);


    ///
    /// Attempt invalid voting
    ///
    BAL::Log("=> Attempting to vote with invalid settings");

    // Attempt full-abstention with fully-weighted opinions
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 2));

        Tags fullAbstentionTags;
        fullAbstentionTags.insert(ABSTAIN_VOTE_TAG);

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, fullAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of full-abstention with fully-weighted opinions was NOT detected!");
    }

    // Attempt full-abstention with partial opinions
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));

        Tags fullAbstentionTags;
        fullAbstentionTags.insert(ABSTAIN_VOTE_TAG);

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, fullAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of full-abstention with partial opinions was NOT detected!");
    }

    // Attempt full-abstention with partial-abstention
    {
        FullOpinions opinions;

        Tags mixedTags;
        mixedTags.insert(ABSTAIN_VOTE_TAG);
        mixedTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "5");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, mixedTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of full-abstention with partial-abstention was NOT detected!");
    }

    // Attempt full-abstention with partial-abstention and partial-voting
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));

        Tags mixedTags;
        mixedTags.insert(ABSTAIN_VOTE_TAG);
        mixedTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "2");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, mixedTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of full-abstention") + \
                    "with partial-abstention and partial-voting was NOT detected!");
    }

    // Attempt full-weighted voting with partial-abstention tags set to zero
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 5));

        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "0");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of full-weighted + zero partial-abstention voting ") + \
                    "was NOT detected!");
    }

    // Attempt less than full-weighted voting without partial abstention
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of less than full-weighted voting ") + \
                    "without partial abstention was NOT detected!");
    }

    // Attempt less than full-weighted voting with partial abstention
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 3));

        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "1");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of less than full-weighted voting ") + \
                    "with partial abstention was NOT detected!");
    }

    // Attempt more than full-weighted voting without partial abstention
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 6));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of more than full-weighted voting ") + \
                    "without partial abstention was NOT detected!");
    }

    // Attempt more than full-weighted voting with partial abstention
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 6));

        Tags partialAbstentionTags;
        partialAbstentionTags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "1");

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, partialAbstentionTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of more than full-weighted voting ") + \
                    "with partial abstention was NOT detected!");
    }

    // Attempt voting for an candidate that is not affiliated with the contest
    {
        // Create an unaffiliated contest
        const string uContest = "Unaffiliated Contest";
        const string uContestDesc = "Description for " + uContest;
        const string uContestant1 = "Contestant 1 for " + uContest;
        const string uContestant2 = "Contestant 2 for " + uContest;
        {
            const Timestamp uBegin = currentTime() + secondsOfDelayBeforeContestBegins;
            const Timestamp uEnd = uBegin + contestDurationSecs;

            set<ContestantDescriptor> contestants;

            ContestantDescriptor c1;
            c1.name = uContestant1;
            c1.description = "Description for " + c1.name;
            contestants.insert(c1);

            ContestantDescriptor c2;
            c2.name = uContestant2;
            c2.description = "Description for " + c2.name;
            contestants.insert(c2);

            newContest(groupId, uContest, uContestDesc, contestants,
                       uBegin, uEnd, emptyTags);
        }
        ContestId uContestId = FindContestId(*this, groupId, uContest,
                                                        "The unaffiliated contest should have been found!");

        // Identify an unaffiliated contestant
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrUnaffiliatedCxId = SeekOfficialContestant(groupContestants, uContestId,
                                                                         uContestant1);
        BAL::Verify(ptrUnaffiliatedCxId != nullptr, "The unaffiliated contestant should have been found");
        ContestantId cUnaffiliatedId = *ptrUnaffiliatedCxId;

        // Vote for the unaffiliated constants
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(cUnaffiliatedId, 5));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of voting for an unaffiliated contestant ") + \
                    "was NOT detected");
    }


    // Attempt negative voting for an official candidate: negative first, positive second
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, -3000));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, +3005));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of negative voting (negative first) was NOT detected");
    }

    // Attempt negative voting for an official candidate: positive first, negative second
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c2Id, +3005));
        opinions.contestantOpinions.insert(std::make_pair(c1Id, -3000));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of redundant voting (negative second) for the same candidate");
    }

    // Attempt negative voting for a write-in candidate:
    // negative weight for write-in, positive weight for official
    {
        ContestantDescriptor cDescriptor;
        cDescriptor.name = "Negative Write-In";
        cDescriptor.description = "Description for " + cDescriptor.name;

        FullOpinions opinions;

        opinions.contestantOpinions.insert(std::make_pair(c2Id, +3005));
        opinions.writeInOpinions.insert(std::make_pair(cDescriptor, -3000));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of negative voting (negative first) was NOT detected");
    }

    // Attempt negative voting for a write-in candidate:
    // positive weight for write-in, negative weight for official
    {
        ContestantDescriptor cDescriptor;
        cDescriptor.name = "Positive Write-In";
        cDescriptor.description = "Description for " + cDescriptor.name;

        FullOpinions opinions;
        opinions.writeInOpinions.insert(std::make_pair(cDescriptor, +3005));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, -3000));

        bool rejectionDetected = false;
        try {
            setDecision(groupId, contestId, voter1, opinions, emptyTags);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    "Rejection of redundant voting (negative second) for the same candidate");
    }

    BAL::Log("Rejections have been properly detected");


    ///
    /// Verify valid voting
    ///
    BAL::Log("=> Verifying valid voting");

    // Attempt split-voting with partial abstention
    {
        FullOpinions opinions;
        opinions.contestantOpinions.insert(std::make_pair(c1Id, 2));
        opinions.contestantOpinions.insert(std::make_pair(c2Id, 1));

        Tags tags;
        tags.insert(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + "2");

        setDecision(groupId, contestId, voter1, opinions, tags);
    }

    BAL::Log("Test PASSED");
}

/**
 * END OF Test rejection of invalid voting
 */


/**
 * Test rejection of contest creation with invalid parameters
 */
void Pollaris::testRejectInvalidContestCreation() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting Rejection of Invalid Contests");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = TEST_PREFIX + "Reject Invalid Contest Group";
    createDifferentWeightedGroup(groupName);

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    const std::string validContestName = "Contest for " + groupName;
    const std::string validContestDesc = validContestName + ": Description";
    const Tags emptyTags;
    const Timestamp validContestBegin = currentTime() + secondsOfDelayBeforeContestBegins;
    const Timestamp validContestEnd = validContestBegin + contestDurationSecs;

    ContestantDescriptor validC1, validC2;
    set<ContestantDescriptor> validContestants;
    {
        validC1.name = "Contestant 1 for " + validContestName;
        validC1.description = "Description for " + validC1.name;
        validContestants.insert(validC1);

        validC2.name = "Contestant 2 for " + validContestName;
        validC2.description = "Description for " + validC2.name;
        validContestants.insert(validC2);
    }


    ///
    /// Attempt to create a contest with invalid parameters
    ///
    uint16_t qtyRejectionsDetected = 0;
    BAL::Log("=> Attempt to create contests with invalid contest parameters");

    // Invalid contest name
    bool rejectionDetected = false;
    try {
        const std::string& invalidContestName = "";
        newContest(groupId, invalidContestName, validContestDesc, validContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contest name was NOT detected!");

    // Invalid contest end date: end date must be after the beginning
    rejectionDetected = false;
    try {
        const Timestamp invalidContestEnd = validContestBegin;
        newContest(groupId, validContestName, validContestDesc, validContestants,
                   validContestBegin, invalidContestEnd, emptyTags);

    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contest end date was NOT detected!");

    // Invalid contest end date: end date must be after the current time
    rejectionDetected = false;
    try {
        const Timestamp invalidContestEnd = currentTime();
        newContest(groupId, validContestName, validContestDesc, validContestants,
                   validContestBegin, invalidContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contest end date was NOT detected!");

    // Invalid contestants: fewer than 2 contestants
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant quantity was NOT detected!");

    // Invalid contestants: contestant name is too short
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        ContestantDescriptor invalidC2;
        invalidC2.name = "";
        invalidC2.description = "Description for " + invalidC2.name;
        invalidContestants.insert(invalidC2);

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant name was NOT detected!");

    // Invalid contestants: contestant name is too long
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        ContestantDescriptor invalidC2;
        for (uint16_t i = 0; i<=161; i++) invalidC2.name += "A";
        invalidC2.description = "Description for " + invalidC2.name;
        invalidContestants.insert(invalidC2);

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant name was NOT detected!");

    // Invalid contestants: contestant description is too long
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        ContestantDescriptor invalidC2;
        invalidC2.name = validC2.name;
        for (uint16_t i = 0; i<=1001; i++) invalidC2.description += "A";
        invalidContestants.insert(invalidC2);

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant name was NOT detected!");

    // Invalid contestants: contestant tag is too short
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        ContestantDescriptor invalidC2;
        invalidC2.name = validC2.name;
        invalidC2.description = validC2.description;
        Tags invalidTags;
        invalidTags.insert("");
        invalidC2.tags = invalidTags;

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant tag was NOT detected!");

    // Invalid contestants: contestant tag is too long
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        ContestantDescriptor invalidC2;
        invalidC2.name = validC2.name;
        invalidC2.description = validC2.description;
        Tags invalidTags;
        std::string tooLongTag;
        for (uint16_t i = 0; i<=101; i++) tooLongTag += "A";
        invalidTags.insert(tooLongTag);
        invalidC2.tags = invalidTags;

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant tag was NOT detected!");

    // Invalid contestants: excessive contestant tags
    rejectionDetected = false;
    try {
        set<ContestantDescriptor> invalidContestants;
        invalidContestants.insert(validC1);

        ContestantDescriptor invalidC2;
        invalidC2.name = validC2.name;
        invalidC2.description = validC2.description;
        Tags invalidTags;
        for (uint16_t i = 0; i<=101; i++) invalidTags.insert("Valid tag");
        invalidC2.tags = invalidTags;

        newContest(groupId, validContestName, validContestDesc, invalidContestants,
                   validContestBegin, validContestEnd, emptyTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of invalid contestant tag was NOT detected!");

    // Invalid tags: tag is too short
    rejectionDetected = false;
    try {
        Tags invalidTags;
        invalidTags.insert("");

        newContest(groupId, validContestName, validContestDesc, validContestants,
                   validContestBegin, validContestEnd, invalidTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of tag is too short was NOT detected!");

    // Invalid tags: tag is too long
    rejectionDetected = false;
    try {
        Tags invalidTags;
        std::string tooLongTag;
        for (uint16_t i = 0; i<=101; i++) tooLongTag += "A";
        invalidTags.insert(tooLongTag);

        newContest(groupId, validContestName, validContestDesc, validContestants,
                   validContestBegin, validContestEnd, invalidTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of tag is too long was NOT detected!");

    // Invalid tags: excessive tags
    rejectionDetected = false;
    try {
        Tags invalidTags;
        for (uint16_t i = 0; i<=101; i++) invalidTags.insert("Valid Tag " + std::to_string(i));

        newContest(groupId, validContestName, validContestDesc, validContestants,
                   validContestBegin, validContestEnd, invalidTags);
    } catch (fc::assert_exception&) {
       // Success: proceed
       qtyRejectionsDetected++;
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected, "Rejection of excessive tags was NOT detected!");


    ///
    /// Verify the ability to create a contest with valid parameters
    ///
    newContest(groupId, validContestName, validContestDesc, validContestants,
               validContestBegin, validContestEnd, emptyTags);
    const ContestId* ptrContestId = SeekContestId(*this, groupId, validContestName);
    BAL::Verify(ptrContestId != nullptr, "The validly defined contest should have been found!");

    BAL::Log("Test PASSED:", qtyRejectionsDetected,"Rejections detected");
}


/**
 * BEGINNING OF testRejectInvalidModsOfGroupAndContest1
 *
 * Test changing membership of a polling group after a group is associated
 * with at least one contest.
 */
const std::string testRejectInvalidModsOfGroupAndContest1_GroupName = TEST_PREFIX + "Reject invalid modifications of group membership and contests";
const std::string testRejectInvalidModsOfGroupAndContest1_ContestName = "Contest for " + testRejectInvalidModsOfGroupAndContest1_GroupName;
const std::string testRejectInvalidModsOfGroupAndContest1_Contestant1Name = "Contestant 1";
const std::string testRejectInvalidModsOfGroupAndContest1_Contestant2Name = "Contestant 2";
const std::string testRejectInvalidModsOfGroupAndContest1_Contestant3Name = "Contestant 3";


/**
 * Stage 1: Verify capability to add members to a group before contest creation
 * Stage 2: Verify capability to remove members from a group before contest creation
 * Stage 3: Create a contest
 * Stage 4: Attempt to add members
 * Stage 5: Attempt to remove members
 * Stage 6: Attempt an invalid modification of a contest
 * Stage 7: Validly modify the contest to verify capability
 * Stage 8: Revert the modifications
 */
void Pollaris::testRejectModifyGroupAndContest_Pre() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectInvalidModsOfGroupAndContest1_GroupName + ": Pre");

    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectInvalidModsOfGroupAndContest1_GroupName;
    const std::string contestName = testRejectInvalidModsOfGroupAndContest1_ContestName;
    const std::string contestDesc = contestName + ": Description";
    const Tags emptyTags;
    BAL::AccountHandle voter1 = FindAccount(*this, "testvoter1"_N);
    BAL::AccountHandle voter2 = FindAccount(*this, "testvoter2"_N);
    BAL::AccountHandle voter3 = FindAccount(*this, "testvoter3"_N);
    BAL::AccountHandle voter4 = FindAccount(*this, "testvoter4"_N);
    BAL::AccountHandle voter5 = FindAccount(*this, "testvoter5"_N);

    // Verify that the group does not exist
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group == nullptr, "Test group 0 should not exist at start of test");
    }


    ///
    /// Stage 1: Verify capability to add members from a group before contest creation
    /// Create a group with 5 members before contest creation
    ///
    BAL::Log("=> Adding voters to polling group");
    {
        const uint32_t voterWeight = 1;
        addVoter(groupName, voter1, voterWeight, emptyTags);
        addVoter(groupName, voter2, voterWeight, emptyTags);
        addVoter(groupName, voter3, voterWeight, emptyTags);
        addVoter(groupName, voter4, voterWeight, emptyTags);
        addVoter(groupName, voter5, voterWeight, emptyTags);
    }

    // Verify the existence of the group and voter
    GroupId groupId;
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Log("=> Checking polling group membership");
        BAL::Verify(IsVoterPresent(*this, group->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter3),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter4),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter5),
                    "Test voter should have been found in the test group");

        groupId = group->id;
    }


    ///
    /// Stage 2: Verify capability to remove members from a group before contest creation
    /// Remove 2 members from the group
    ///
    BAL::Log("=> Remove 2 voters from polling group");
    {
        removeVoter(groupName, voter4);
        removeVoter(groupName, voter5);
    }

    // Verify the new membership
    {
        PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);

        const PollingGroup* group = FindGroup(groups, groupName);
        BAL::Verify(group != nullptr, "Test group was not found after adding a voter");

        // Verify the presence of the test voter in the group
        BAL::Log("=> Checking polling group membership");
        BAL::Verify(IsVoterPresent(*this, group->id, voter1),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter2),
                    "Test voter should have been found in the test group");
        BAL::Verify(IsVoterPresent(*this, group->id, voter3),
                    "Test voter should have been found in the test group");
        BAL::Verify(!IsVoterPresent(*this, group->id, voter4),
                    "Test voter should have been found in the test group");
        BAL::Verify(!IsVoterPresent(*this, group->id, voter5),
                    "Test voter should have been found in the test group");
    }


    ///
    /// Stage 3: Create a contest
    ///
    BAL::Log("=> Creating Contest");
    const Timestamp contestBegin = currentTime() + secondsOfDelayBeforeContestBegins;
    const Timestamp contestEnd = contestBegin + contestDurationSecs;

    ContestantDescriptor c1;
    ContestantDescriptor c2;
    {
        set<ContestantDescriptor> contestants;

        c1.name = testRejectInvalidModsOfGroupAndContest1_Contestant1Name;
        c1.description = "Description for " + c1.name;
        contestants.insert(c1);

        c2.name = testRejectInvalidModsOfGroupAndContest1_Contestant2Name;
        c2.description = "Description for " + c2.name;
        contestants.insert(c2);

        newContest(groupId, contestName, contestDesc, contestants,
                   contestBegin, contestEnd, emptyTags);
    }

    // Verify the existence of the contest
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "The newly created contest should have been found!");

    // Verify the existence of the contestants
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        testRejectInvalidModsOfGroupAndContest1_Contestant1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        testRejectInvalidModsOfGroupAndContest1_Contestant2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        testRejectInvalidModsOfGroupAndContest1_Contestant3Name);
        BAL::Verify(ptrC3Id == nullptr, "Contestant 3 should NOT have been found");
    }


    ///
    /// Stage 4: Attempt to add members
    ///
    BAL::Log("=> Attempting to add members to the group");
    bool rejectionDetected = false;
    try {
        const uint32_t voterWeight = 1;
        addVoter(groupName, voter4, voterWeight, emptyTags);

    } catch (fc::assert_exception&) {
       // Success: proceed
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected,
                std::string("Rejection of adding members to a group with contests") + \
                            " was NOT detected!");


    ///
    /// Stage 5: Attempt to remove members
    ///
    BAL::Log("=> Attempting to remove members from the group");
    rejectionDetected = false;
    try {
        removeVoter(groupName, voter1);

    } catch (fc::assert_exception&) {
       // Success: proceed
       rejectionDetected = true;
    }
    BAL::Verify(rejectionDetected,
                std::string("Rejection of removing members from a group with contests") + \
                            " was NOT detected!");


    ///
    /// Stage 6: Attempt an invalid modification of a contest
    ///
    optional<string> newName, newDescription;
    optional<Tags> newTags;
    set<ContestantId> deleteContestants;
    set<ContestantDescriptor> addContestants;
    optional<Timestamp> newBegin, newEnd;

    // Attempt to reduce candidates to fewer than 2
    {
        deleteContestants.insert(c1Id);

        rejectionDetected = false;
        try {
            modifyContest(groupId, contestId, newName, newDescription,
                          newTags, deleteContestants, addContestants,
                          newBegin, newEnd);
        } catch (fc::assert_exception&) {
            // Success: proceed
           rejectionDetected = true;
        }
        BAL::Verify(rejectionDetected,
                    std::string("Rejection of removing members from a group with contests") + \
                                " was NOT detected!");
    }

    BAL::Log("=> Rejections have been properly detected");


    ///
    /// Stage 7: Validly modify the contest to verify the capability
    ///
    BAL::Log("=> Verifying capability of modifying a contest");

    // Modify all contest parameters
    ContestantDescriptor c3;
    c3.name = testRejectInvalidModsOfGroupAndContest1_Contestant3Name;
    c3.description = "Description for " + c3.name;

    const optional<string> altName = "Alternative name for " + contestName;
    const optional<string> altDesc = "Alternative description for " + contestDesc;
    const string altTagLabel = "Alternative Tag";
    Tags tags; tags.insert(altTagLabel);
    optional<Tags> altTags = tags;
    deleteContestants.clear(); deleteContestants.insert(c1Id);
    addContestants.clear(); addContestants.insert(c3);
    const optional<Timestamp> altBegin =  currentTime() + secondsOfDelayBeforeContestBegins;
    const optional<Timestamp> altEnd =  *altBegin + contestDurationSecs;

    modifyContest(groupId, contestId, altName, altDesc,
                  altTags, deleteContestants, addContestants,
                  altBegin, altEnd);

    // Verify the changes
    ContestantId c3Id;
    {
        Contests contests = getTable<Contests>(groupId);
        Contest c = contests.getId(contestId, "Unable to find the modified contest");

        // Verify the ability to change the name and description
        BAL::Verify(c.name == *altName, "The contest name was NOT as expected");
        BAL::Verify(c.description == *altDesc, "The contest description was NOT as expected");

        // Verify the tags
        BAL::Verify(c.tags.size() == 1, "The quantity of contest tags was NOT as expected");
        BAL::Verify(c.tags.find(altTagLabel) != c.tags.end(),
                    "The expected contest tag was NOT found");

        // Verify the changes to contestants
        {
            Contestants groupContestants = getTable<Contestants>(groupId);
            const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                                                 testRejectInvalidModsOfGroupAndContest1_Contestant1Name);
            BAL::Verify(ptrC1Id == nullptr, "Contestant 1 should NOT have been found");

            const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                                                 testRejectInvalidModsOfGroupAndContest1_Contestant2Name);
            BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");

            const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                                                 testRejectInvalidModsOfGroupAndContest1_Contestant3Name);
            BAL::Verify(ptrC3Id != nullptr, "Contestant 3 should have been found");
            c3Id = *ptrC3Id;
        }

        // Verify the ability to change the begin and end dates
        BAL::Verify(c.begin == *altBegin, "The contest beginning was NOT as expected");
        BAL::Verify(c.end == *altEnd, "The contest beginning was NOT as expected");
    }


    ///
    /// Stage 8: Revert the modifications
    ///
    deleteContestants.clear(); deleteContestants.insert(c3Id);
    addContestants.clear(); addContestants.insert(c1);
    modifyContest(groupId, contestId, contestName, contestDesc,
                  emptyTags, deleteContestants, addContestants,
                  contestBegin, contestEnd);

    // Verify the changes
    {
        Contests contests = getTable<Contests>(groupId);
        Contest c = contests.getId(contestId, "Unable to find the modified contest");

        // Verify the ability to change the name and description
        BAL::Verify(c.name == contestName, "The contest name was NOT as expected");
        BAL::Verify(c.description == contestDesc, "The contest description was NOT as expected");

        // Verify the tags
        BAL::Verify(c.tags.size() == 0, "The contest tags was NOT as expected");

        // Verify the changes to contestants
        {
            Contestants groupContestants = getTable<Contestants>(groupId);
            const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                                                 testRejectInvalidModsOfGroupAndContest1_Contestant1Name);
            BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");

            const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                                                 testRejectInvalidModsOfGroupAndContest1_Contestant2Name);
            BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");

            const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                                                 testRejectInvalidModsOfGroupAndContest1_Contestant3Name);
            BAL::Verify(ptrC3Id == nullptr, "Contestant 3 should NOT have been found");
        }

        // Verify the ability to change the begin and end dates
        BAL::Verify(c.begin == contestBegin, "The contest beginning was NOT as expected");
        BAL::Verify(c.end == contestEnd, "The contest beginning was NOT as expected");
    }

    BAL::Log("Test PASSED");
}

/**
 * Attempt to modify a contest after the contest has begun.
 *
 * At the start, only Contestant 1 and 2 should be present.
 */
void Pollaris::testRejectModifyGroupAndContest_AfterContestStart() {
    ///
    /// Initialize values for the test
    ///
    const std::string groupName = testRejectInvalidModsOfGroupAndContest1_GroupName;
    const std::string contestName = testRejectInvalidModsOfGroupAndContest1_ContestName;
    const std::string c1Name = testRejectInvalidModsOfGroupAndContest1_Contestant1Name;
    const std::string c2Name = testRejectInvalidModsOfGroupAndContest1_Contestant2Name;
    const std::string c3Name = testRejectInvalidModsOfGroupAndContest1_Contestant3Name;

    // Find the group ID
    GroupId groupId = FindGroupId(groupName, "Test group should have been found");

    // Find the contest ID
    ContestId contestId = FindContestId(*this, groupId, contestName,
                                        "Contest should have been found!");

    // Find the contestant IDs
    ContestantId c1Id, c2Id;
    {
        Contestants groupContestants = getTable<Contestants>(groupId);
        const ContestantId* ptrC1Id = SeekOfficialContestant(groupContestants, contestId,
                                        c1Name);
        BAL::Verify(ptrC1Id != nullptr, "Contestant 1 should have been found");
        c1Id = *ptrC1Id;

        const ContestantId* ptrC2Id = SeekOfficialContestant(groupContestants, contestId,
                                        c2Name);
        BAL::Verify(ptrC2Id != nullptr, "Contestant 2 should have been found");
        c2Id = *ptrC2Id;

        const ContestantId* ptrC3Id = SeekOfficialContestant(groupContestants, contestId,
                                        c3Name);
        BAL::Verify(ptrC3Id == nullptr, "Contestant 3 should NOT have been found");
    }


    ///
    ///  Attempt to modify the contest after it has begun
    ///
    optional<string> newName, newDescription;
    optional<Tags> newTags;
    set<ContestantId> deleteContestants;
    set<ContestantDescriptor> addContestants;
    optional<Timestamp> newBegin, newEnd;

    // Helper for resetting input parameters
    auto resetInputParameters = [&newName, &newDescription, &newTags,
                                 &deleteContestants, &addContestants,
                                 &newBegin, &newEnd]() {
        newName.reset();
        newDescription.reset();
        newTags.reset();
        deleteContestants.clear();
        addContestants.clear();
        newBegin.reset();
        newEnd.reset();
    };

    // Helper for detecting rejection
    auto isRejectionDetected = [contract=this, &groupId, &contestId]
                               (optional<string> &newName, optional<string> &newDescription,
                                optional<Tags> &newTags, set<ContestantId> &deleteContestants,
                                set<ContestantDescriptor> &addContestants,
                                optional<Timestamp> &newBegin, optional<Timestamp> &newEnd)
                               -> bool {
        bool rejectionDetected = false;
        try {
            contract->modifyContest(groupId, contestId, newName, newDescription,
                          newTags, deleteContestants, addContestants,
                          newBegin, newEnd);
        } catch (fc::assert_exception&) {
            rejectionDetected = true;
        }
        return rejectionDetected;
    };


    // Attempt to add a contestant
    ContestantDescriptor c3;
    c3.name = testRejectInvalidModsOfGroupAndContest1_Contestant3Name + " (re-added)";
    c3.description = "Description for " + c3.name;

    addContestants.insert(c3);
    BAL::Verify(isRejectionDetected(newName, newDescription, newTags,
                                    deleteContestants, addContestants,
                                    newBegin, newEnd),
                std::string("Rejection of adding a contestant") + \
                            "  after contest start was NOT detected!");

    // Attempt to replace a contestant
    resetInputParameters();
    addContestants.insert(c3);
    deleteContestants.insert(c2Id);
    BAL::Verify(isRejectionDetected(newName, newDescription, newTags,
                                    deleteContestants, addContestants,
                                    newBegin, newEnd),
                std::string("Rejection of replacing a contestant") + \
                            "  after contest start was NOT detected!");

    // Attempt to change the name
    resetInputParameters();
    newName = "New Contest Name";
    BAL::Verify(isRejectionDetected(newName, newDescription, newTags,
                                    deleteContestants, addContestants,
                                    newBegin, newEnd),
                std::string("Rejection of updating a contest name") + \
                            "  after contest start was NOT detected!");

    // Attempt to change the description
    resetInputParameters();
    newDescription = "New Contest Description";
    BAL::Verify(isRejectionDetected(newName, newDescription, newTags,
                                    deleteContestants, addContestants,
                                    newBegin, newEnd),
                std::string("Rejection of updating a contest description") + \
                            "  after contest start was NOT detected!");

    // Attempt to change the contest beginning
    resetInputParameters();
    newBegin =  currentTime() + secondsOfDelayBeforeContestBegins;
    BAL::Verify(isRejectionDetected(newName, newDescription, newTags,
                                    deleteContestants, addContestants,
                                    newBegin, newEnd),
                std::string("Rejection of updating a contest beginning") + \
                            "  after contest start was NOT detected!");

    // Attempt to change the contest ending
    resetInputParameters();
    newEnd =  currentTime() + secondsOfDelayBeforeContestBegins;
    BAL::Verify(isRejectionDetected(newName, newDescription, newTags,
                                    deleteContestants, addContestants,
                                    newBegin, newEnd),
                std::string("Rejection of updating a contest ending") + \
                            "  after contest start was NOT detected!");

    BAL::Log("Test PASSED: Rejections detected");
}


/**
 * Attempt to modify a contest after the contest has started
 */
void Pollaris::testRejectModifyGroupAndContest_During() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectInvalidModsOfGroupAndContest1_GroupName + ": During");

    testRejectModifyGroupAndContest_AfterContestStart();
}


/**
 * Attempt to modify a contest after the contest has ended
 */
void Pollaris::testRejectModifyGroupAndContest_Post() {
    // Require the contract's authority to run tests
    requireAuthorization(ownerAccount());
    BAL::Log("\n\nTesting " + testRejectInvalidModsOfGroupAndContest1_GroupName + ": Post");

    testRejectModifyGroupAndContest_AfterContestStart();
}

/**
 * END OF testRejectInvalidModsOfGroupAndContest1
 */


/**
 * END OF Rejection Detection test suite
 */
#endif
