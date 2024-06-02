#include "Pollaris.hpp"

using TallyResults = std::map<ContestantIdVariant, uint64_t>;

inline bool GoodTag(const string& tag) {
    if (tag.empty()) return false;
    if (tag.size() > 100) return false;
    return true;
}
inline bool GoodTags(const Tags& tags) {
    return std::all_of(tags.begin(), tags.end(), GoodTag);
}

inline bool isPrefix(std::string_view prefix, std::string_view text) {
    return text.length() >= prefix.length() && text.substr(0, prefix.size()) == prefix;
}

inline uint32_t GetAbstainWeight(const Tags& tags) {
    auto tagItr = tags.lower_bound(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX);
    if (tagItr == tags.end() || !isPrefix(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX, *tagItr))
        return 0;
    std::size_t endPos;
    auto weight = std::stoul(tagItr->substr(PARTIAL_ABSTAIN_VOTE_TAG_PREFIX.size()), &endPos);
    BAL::Verify((PARTIAL_ABSTAIN_VOTE_TAG_PREFIX.size() + endPos) == tagItr->size(),
                "Invalid partial abstain tag found; cannot continue");
    BAL::Verify(weight != 0, "Invalid partial abstain tag found; abstain weight must be positive");
    // Attempt to reconstruct the tag from the weight we found; if it doesn't match, reject it.
    BAL::Verify((PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + std::to_string(weight)) == *tagItr,
          "Invalid partial abstain tag found: abstaining weight value is malformatted");
    return weight;
}

inline void CheckContestant(const ContestantDescriptor& descriptor) {
        BAL::Verify(!descriptor.name.empty(), "All contestants must specify a name");
        BAL::Verify(descriptor.name.size() <= 160, "Contestant name must not exceed 160 characters");
        BAL::Verify(descriptor.description.size() <= 1000,
                          "Contestant description must not exceed 1000 characters");
        BAL::Verify(descriptor.tags.size() <= 100, "Must not exceed 100 tags");
        BAL::Verify(GoodTags(descriptor.tags), "A contestant specified an invalid tag");
}

inline void CheckContestants(const set<ContestantDescriptor>& contestantDescriptors) {
    for (const auto& cd : contestantDescriptors) { CheckContestant(cd); }
}

inline void UpdateJournal(Pollaris& contract, Name table, uint64_t scope, uint64_t key,
                          ModificationType modification) {
    Pollaris::Journal journal = contract.getTable<Pollaris::Journal>(scope);
    Timestamp now = contract.currentTime();
    // Record new entry's ID before deleting stale entries
    auto newId = journal.nextId();

    // Remove old entries
    auto cutoffTime = now - /* 12 hours */(12 * 60 * 60);
    while (journal.rbegin() != journal.rend() && journal.rbegin()->timestamp < cutoffTime)
        journal.erase(*journal.rbegin());

    // Add new entry
    journal.create([newId, table, scope, key, modification, now](Pollaris::JournalEntry& entry) {
        entry.id = newId;
        entry.timestamp = now;
        entry.table = table;
        entry.scope = scope;
        entry.key = key;
        entry.modification = modification;
    });
}
// Convenience template to convert scope or key from common types to ints
template<typename Scope, typename Key>
inline void UpdateJournal(Pollaris& c, Name t, Scope s, Key k, ModificationType m) {
    UpdateJournal(c, t, BAL::KeyCast(s), BAL::KeyCast(k), m);
}

inline void AddContestants(Pollaris& contract, Pollaris::Contestants& db, ContestId contestId,
                           set<ContestantDescriptor> contestantDescriptors) {
    for (auto& cd : contestantDescriptors) {
        auto id = db.nextId();
        db.create([id, cd=std::move(cd), contest=contestId] (Pollaris::Contestant& c) {
            c.id = id;
            c.contest = contest;
            c.name = std::move(cd.name);
            c.description = std::move(cd.description);
            c.tags = std::move(cd.tags);
        });
        UpdateJournal(contract, CONTESTANTS, db.scope(), id, ModificationType::addRow);
    }
}

inline void RetainWriteIn(Pollaris& contract, GroupId group, WriteInId writeIn) {
    Pollaris::WriteIns writeIns = contract.getTable<Pollaris::WriteIns>(group);
    auto itr = writeIns.findId(writeIn);
    BAL::Verify(itr != writeIns.end(), "Cannot retain write in", writeIn, "because no such write in exists");

    writeIns.modify(itr, [](Pollaris::WriteIn& writeIn) { ++writeIn.refcount; });
    UpdateJournal(contract, WRITE_INS, group, writeIn, ModificationType::modifyRow);
}
inline WriteInId RetainWriteIn(Pollaris& contract, GroupId group, ContestId contest, ContestantDescriptor writeIn) {
    Pollaris::WriteIns writeIns = contract.getTable<Pollaris::WriteIns>(group);
    auto writeInsByContest = writeIns.getSecondaryIndex<BY_CONTEST>();
    auto range = writeInsByContest.range(Pollaris::WriteIn::contestKeyMin(contest),
                                         Pollaris::WriteIn::contestKeyMax(contest));

    // Search for matching write-in
    for (; range.first != range.second; ++range.first)
        if (range.first->name == writeIn.name && range.first->description == writeIn.description &&
                range.first->tags == writeIn.tags) {
            writeInsByContest.modify(range.first, [](Pollaris::WriteIn& writeIn) { ++writeIn.refcount;});
            UpdateJournal(contract, WRITE_INS, group, range.first->id, ModificationType::modifyRow);
            return range.first->id;
        }

    // No match found; create a new one
    auto id = writeIns.nextId();
    writeIns.create([id, contest, &writeIn](Pollaris::WriteIn& wi) {
        wi.id = WriteInId(id);
        wi.contest = contest;
        wi.name = std::move(writeIn.name);
        wi.description = std::move(writeIn.description);
        wi.tags = std::move(writeIn.tags);
        wi.refcount = 1;
    });
    UpdateJournal(contract, WRITE_INS, group, id, ModificationType::addRow);
    return id;
}

inline void ReleaseWriteIn(Pollaris& contract, GroupId group, WriteInId writeIn) {
    Pollaris::WriteIns writeIns = contract.getTable<Pollaris::WriteIns>(group);
    auto& contestant = writeIns.getId(writeIn, "Cannot release write-in candidate: candidate not found");
    if (contestant.refcount <= 1) {
        UpdateJournal(contract, WRITE_INS, group, contestant.id, ModificationType::deleteRow);
        writeIns.erase(contestant);
    } else {
        writeIns.modify(contestant, [](Pollaris::WriteIn& writeIn) { --writeIn.refcount; });
        UpdateJournal(contract, WRITE_INS, group, contestant.id, ModificationType::modifyRow);
    }
}

/// Delete a decision from the provided index referenced by the provided iterator. Iterator will be left pointing to
/// the next decision after the one removed. This will also release any write-ins retained by this decision.
template<typename Index>
inline typename Index::const_iterator DeleteDecision(Pollaris& contract, GroupId group, Index& index,
                                                     typename Index::const_iterator iterator) {
    static_assert(std::is_same_v<typename Index::Row, Pollaris::Decision>, "This function only deletes a Decision");

    // For all opinions about write-in candidates, release the write-in candidate before deleting decision
    for (const auto& opinion : iterator->opinions)
        if (std::holds_alternative<WriteInId>(opinion.first))
            ReleaseWriteIn(contract, group, std::get<WriteInId>(opinion.first));
    UpdateJournal(contract, DECISIONS, group, iterator->id, ModificationType::deleteRow);
    return index.erase(iterator);
}

inline void DeleteContestDecisions(Pollaris& contract, GroupId groupId, ContestId contestId) {
    Pollaris::Decisions decisions = contract.getTable<Pollaris::Decisions>(groupId);
    auto decisionsByContest = decisions.getSecondaryIndex<BY_CONTEST>();
    auto range = decisionsByContest.range(Pollaris::Decision::contestKeyMin(contestId),
                                          Pollaris::Decision::contestKeyMax(contestId));
    while (range.first != range.second)
        range.first = DeleteDecision(contract, groupId, decisionsByContest, range.first);
}

inline void DeleteContestContestants(Pollaris& contract, GroupId groupId, ContestId contestId) {
    Pollaris::Contestants contestants = contract.getTable<Pollaris::Contestants>(groupId);
    auto contestantsByContest = contestants.getSecondaryIndex<BY_CONTEST>();
    auto range = contestantsByContest.range(Pollaris::Contestant::contestKeyMin(contestId),
                                            Pollaris::Contestant::contestKeyMax(contestId));
    while (range.first != range.second) {
        UpdateJournal(contract, CONTESTANTS, groupId, range.first->id, ModificationType::deleteRow);
        range.first = contestantsByContest.erase(range.first);
    }
}

inline void DeleteContestWriteIns(Pollaris& contract, GroupId groupId, ContestId contestId) {
    Pollaris::WriteIns writeIns = contract.getTable<Pollaris::WriteIns>(groupId);
    auto writeInsByContest = writeIns.getSecondaryIndex<BY_CONTEST>();
    auto range = writeInsByContest.range(Pollaris::WriteIn::contestKeyMin(contestId),
                                         Pollaris::WriteIn::contestKeyMax(contestId));
    while (range.first != range.second) {
        UpdateJournal(contract, WRITE_INS, groupId, range.first->id, ModificationType::deleteRow);
        range.first = writeInsByContest.erase(range.first);
    }
}

void DeleteContestResults(Pollaris& contract, GroupId groupId, ContestId contestId) {
    Pollaris::Results results = contract.getTable<Pollaris::Results>(groupId);
    Pollaris::Tallies tallies = contract.getTable<Pollaris::Tallies>(groupId);
    auto resultsByContest = results.getSecondaryIndex<BY_CONTEST>();
    auto talliesByResult = tallies.getSecondaryIndex<BY_RESULT>();

    // For each result in this contest, delete all tallies in that result, and increment iterator by erasing it
    for (auto resultRange = resultsByContest.range(Pollaris::Result::contestKeyMin(contestId),
                                                   Pollaris::Result::contestKeyMax(contestId));
         resultRange.first != resultRange.second;
         resultRange.first = resultsByContest.erase(resultRange.first)) {
        auto result = resultRange.first->id;

        // For each tally in this result, release write-in as necessary, and increment iterator by erasing it
        for (auto tallyRange = talliesByResult.range(Pollaris::Tally::resultKeyMin(result),
                                                     Pollaris::Tally::resultKeyMax(result));
             tallyRange.first != tallyRange.second;
             tallyRange.first = talliesByResult.erase(tallyRange.first)) {
            if (std::holds_alternative<WriteInId>(tallyRange.first->contestant))
                ReleaseWriteIn(contract, groupId, std::get<WriteInId>(tallyRange.first->contestant));

            UpdateJournal(contract, TALLIES, groupId, tallyRange.first->id, ModificationType::deleteRow);
        }

        UpdateJournal(contract, RESULTS, groupId, result, ModificationType::deleteRow);
    }
}

/// Delete all decisions on the given contest which vote for the given contestant
void DeleteDecisionsByContestant(Pollaris& contract, GroupId group, ContestId contest, ContestantId contestant) {
    Pollaris::Decisions decisions = contract.getTable<Pollaris::Decisions>(group);
    auto decisionsByContest = decisions.getSecondaryIndex<BY_CONTEST>();
    auto range = decisionsByContest.range(Pollaris::Decision::contestKeyMin(contest),
                                          Pollaris::Decision::contestKeyMax(contest));

    auto votesForContestant = [contestant](const std::pair<ContestantIdVariant, int32_t>& opinion) {
        // Opinion votes for the particular contestant if it votes for *a* contestant, whose ID is our target,
        // AND if the opinion is non-zero.
        // This fails to preserve an invariant that all decisions' opinions will reference an extant contestant...
        // But it guarantees that all *nonzero* opinions reference an extant contestant.
        return std::holds_alternative<ContestantId>(opinion.first) &&
               std::get<ContestantId>(opinion.first) == contestant &&
               opinion.second != 0;
    };

    // Iterate all decisions on the contest
    while (range.first != range.second) {
        // If any opinion in the decision votes for the target contestant... delete that decision
        if (std::any_of(range.first->opinions.begin(), range.first->opinions.end(), votesForContestant))
            // Delete decision, releasing any write-ins it may have specified
            range.first = DeleteDecision(contract, group, decisionsByContest, range.first);
        else
            ++range.first;
    }
}

const Pollaris::PollingGroup* Pollaris::FindGroup(Pollaris::PollingGroups& groups, std::string_view name) {
    const auto& byName = groups.getSecondaryIndex<BY_NAME>();
    auto nameKey = MakeStringKey(name);

    for (auto range = byName.equalRange(nameKey); range.first != range.second; ++range.first)
        if (range.first->name == name)
            return &*range.first;

    return nullptr;
}

template<typename GetTallies>
TallyResults TallyContest(Pollaris& contract, GroupId group, ContestId contest, GetTallies getTallies) {
    // First, prepare a zero TallyResults
    TallyResults result;
    {
        Pollaris::Contestants contestants = contract.getTable<Pollaris::Contestants>(group);
        auto contestantsByContest = contestants.getSecondaryIndex<BY_CONTEST>();
        for (auto range = contestantsByContest.range(Pollaris::Contestant::contestKeyMin(contest),
                                                     Pollaris::Contestant::contestKeyMax(contest));
             range.first != range.second; ++range.first)
            result[range.first->id] = 0;

        Pollaris::WriteIns writeIns = contract.getTable<Pollaris::WriteIns>(group);
        auto writeInsByContest = writeIns.getSecondaryIndex<BY_CONTEST>();
        for (auto range = writeInsByContest.range(Pollaris::WriteIn::contestKeyMin(contest),
                                                  Pollaris::WriteIn::contestKeyMax(contest));
             range.first != range.second; ++range.first)
            result[range.first->id] = 0;
    }

    // Now tally up the decisions
    Pollaris::Decisions decisions = contract.getTable<Pollaris::Decisions>(group);
    auto decisionsByContest = decisions.getSecondaryIndex<BY_CONTEST>();
    auto decisionRange = decisionsByContest.range(Pollaris::Decision::contestKeyMin(contest),
                                                    Pollaris::Decision::contestKeyMax(contest));

    // For each decision on the contest:
    while (decisionRange.first != decisionRange.second) {
        // There should be only one decision per voter, but if by a bug there's more than one, tally only the last one
        const auto& decision = *(decisionRange.first++);
        if (decisionRange.first != decisionRange.second && decisionRange.first->voter == decision.voter) {
            BAL::Log("WARNING: Ignoring decision ID", uint64_t(decision.id),
                                 "as it has same voter as decision ID", uint64_t(decisionRange.first->id));
            continue;
        }

        // Get the tallies from this individual decision and add them to the overall result
        auto decisionResults = getTallies(decision);
        for (const auto& r : decisionResults)
            result[r.first] += r.second;
        // Note that iterator was already incremented in first line of loop body
    }

    return result;
}

void StoreContestResults(Pollaris& contract, GroupId group, ContestId contest, TallyResults results) {
    // Create a new result record
    Pollaris::Results resultsTable = contract.getTable<Pollaris::Results>(group);
    ResultId resultId = resultsTable.nextId();
    Timestamp now = contract.currentTime();
    resultsTable.create([resultId, contest, now](Pollaris::Result& result) {
        result.id = resultId;
        result.contest = contest;
        result.timestamp = now;
    });
    UpdateJournal(contract, RESULTS, group, resultId, ModificationType::addRow);

    // Create a new tally for each contestant result
    auto table = contract.getTable<Pollaris::Tallies>(group);
    for (const auto& [id, tally] : results) {
        auto recordId = table.nextId();
        table.create([recordId, resultId, &id=id, &tally=tally](Pollaris::Tally& tallyRecord) {
            tallyRecord.id = recordId;
            tallyRecord.result = resultId;
            tallyRecord.contestant = id;
            tallyRecord.tally = tally;
        });

        // If this tally is for a write-in contestant, retain that write-in so its reference count stays accurate.
        if (std::holds_alternative<WriteInId>(id))
            RetainWriteIn(contract, group, std::get<WriteInId>(id));

        UpdateJournal(contract, TALLIES, table.scope(), recordId, ModificationType::addRow);
    }
}

void Pollaris::addVoter(string groupName, BAL::AccountHandle voter, uint32_t weight, Tags tags) {
    // Require the contract's authority to add a voter to a polling group
    requireAuthorization(ownerAccount());

    // Check voter account exists
    BAL::Verify(accountExists(voter), "Unable to add voter to polling group: voter account does not exist");

    // Find the group by name, creating it if it doesn't exist
    BAL::Log("Adding voter", voter, "to group", groupName);
    PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
    auto group = FindGroup(groups, groupName);
    if (group == nullptr) {
        // Group does not yet exist; create it
        BAL::Log("Group does not exist. Creating it.");
        group = &groups.create([id=groups.nextId(), &groupName](PollingGroup& group) {
                group.id = id;
                group.name = std::move(groupName);
        });
        UpdateJournal(*this, POLL_GROUPS, GLOBAL.value, group->id, ModificationType::addRow);
    } else {
        // Group already exists. Check that it has no contests (groups with contests are immutable)
        Contests contests = getTable<Contests>(group->id);
        BAL::Verify(contests.begin() == contests.end(),
                    "Cannot add or modify voters in a polling group once that group has contests");
    }

    // Check if the voter is already in the group
    GroupAccounts accounts = getTable<GroupAccounts>(group->id);
    auto voterItr = accounts.findId(voter);
    if (voterItr != accounts.end()) {
        BAL::Verify(voterItr->weight != weight || voterItr->tags != tags,
                    "Cannot add voter to polling group: voter is already in group with same weight and tags");
        // Update voter in place
        accounts.modify(voterItr, [weight, &tags](GroupAccount& account) {
            account.weight = weight;
            account.tags = std::move(tags);
        });
        UpdateJournal(*this, GROUP_ACCTS, group->id, voter, ModificationType::modifyRow);
        return;
    }

    // Add voter to group
    accounts.create([&voter, weight, &tags](GroupAccount& account) {
        account.account = voter;
        account.weight = weight;
        account.tags = std::move(tags);
    });
    UpdateJournal(*this, GROUP_ACCTS, group->id, voter, ModificationType::addRow);
}

void Pollaris::removeVoter(string groupName, AccountHandle voter) {
    // Require the contract's authority to remove a voter from a polling group
    requireAuthorization(ownerAccount());

    BAL::Verify(accountExists(voter), "Unable to remove voter from polling group: could not find named voter");

    // Find the group by name
    PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
    auto group = FindGroup(groups, groupName);
    BAL::Verify(group != nullptr, "Unable to remove voter from polling group: group name not recognized");

    // Delete all decisions from the voter
    auto decisions = getTable<Decisions>(group->id);
    auto byVoter = decisions.template getSecondaryIndex<BY_VOTER>();
    auto decision = byVoter.lowerBound(Decision::voterKeyMin(voter));
    while (decision != byVoter.end() && decision->voter == voter)
        decision = DeleteDecision(*this, group->id, byVoter, decision);

    // Find the voter in the group
    GroupAccounts accounts = getTable<GroupAccounts>(group->id);
    auto& account = accounts.getId(voter, "Unable to remove voter from polling group: voter name not recognized");
    // Delete the voter from the group
    accounts.erase(account);
    UpdateJournal(*this, GROUP_ACCTS, group->id, voter, ModificationType::deleteRow);
}

void Pollaris::copyGroup(string groupName, string newName) {
    // Require the contract's authority to copy a polling group
    requireAuthorization(ownerAccount());

    // Find the group by name
    PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
    auto oldGroup = FindGroup(groups, groupName);
    BAL::Verify(oldGroup != nullptr,
                "Unable to rename group: referenced group not found. Please check the group name");

    // Check that no group already has the new name
    auto collisionGroup = FindGroup(groups, newName);
    BAL::Verify(collisionGroup == nullptr, "Unable to rename group: new name already belongs to another group");

    // Create the new group
    auto newId = groups.nextId();
    groups.create([newId, &newName, oldGroup](PollingGroup& newGroup) {
        newGroup.id = newId;
        newGroup.name = std::move(newName);
        newGroup.tags = oldGroup->tags;
    });
    UpdateJournal(*this, POLL_GROUPS, GLOBAL.value, newId, ModificationType::addRow);

    // Copy the group members
    GroupAccounts oldAccounts = getTable<GroupAccounts>(oldGroup->id);
    GroupAccounts newAccounts = getTable<GroupAccounts>(newId);
    std::for_each(oldAccounts.begin(), oldAccounts.end(), [this, &newAccounts, newId](const GroupAccount& account) {
        newAccounts.create([&account](GroupAccount& newAccount) {
            std::tie(newAccount.account, newAccount.weight, newAccount.tags)
                    = std::tie(account.account, account.weight, account.tags);
        });
        UpdateJournal(*this, GROUP_ACCTS, newId, account.account, ModificationType::addRow);
    });
}

void Pollaris::renameGroup(string groupName, string newName) {
    // Require the contract's authority to rename a polling group
    requireAuthorization(ownerAccount());

    // Find the group by name
    PollingGroups groups = getTable<PollingGroups>(GLOBAL.value);
    auto group = FindGroup(groups, groupName);
    BAL::Verify(group != nullptr, "Unable to rename group: referenced group not found. Please check the group name");

    // Check that no group already has the new name
    auto collisionGroup = FindGroup(groups, newName);
    BAL::Verify(collisionGroup == nullptr, "Unable to rename group: new name already belongs to another group");

    // Perform the rename
    groups.modify(*group, [&newName](PollingGroup& group) { group.name = std::move(newName); });
    UpdateJournal(*this, POLL_GROUPS, GLOBAL.value, group->id, ModificationType::modifyRow);
}

void Pollaris::newContest(GroupId groupId, string name, string description, set<ContestantDescriptor> contestants,
                         Timestamp begin, Timestamp end, Tags tags) {
    // Require the contract's authority to create a contest
    requireAuthorization(ownerAccount());

    // Static input validation, without regard to database state
    BAL::Verify(!name.empty(), "Contest name must not be empty");
    BAL::Verify(contestants.size() > 1, "At least two contestants must be defined");
    CheckContestants(contestants);
    BAL::Verify(GoodTags(tags), "The contest specifies an invalid tag");
    BAL::Verify(tags.size() <= 100, "Must not exceed 100 tags");
    BAL::Verify(end > begin, "Contest end date must be after begin date");
    BAL::Verify(end > currentTime(), "Contest end must be in the future");

    // Check the group is valid
    BAL::Verify(getTable<PollingGroups>(GLOBAL.value).contains(groupId),
                "No such polling group: " + std::to_string(groupId.value));

    // Create the contest
    Contests contests = getTable<Contests>(groupId);
    ContestId contestId = contests.nextId();
    contests.create([contestId, name=std::move(name), description=std::move(description),
                    tags=std::move(tags), begin, end] (Contest& c) {
        c.id = contestId;
        c.name = std::move(name);
        c.description = std::move(description);
        c.begin = begin;
        c.end = end;
        c.tags = std::move(tags);
    });
    UpdateJournal(*this, CONTESTS, groupId, contestId, ModificationType::addRow);

    // Create the contestants
    Contestants contestantsTable = getTable<Contestants>(groupId);
    AddContestants(*this, contestantsTable, contestId, std::move(contestants));
}

void Pollaris::modifyContest(GroupId groupId, ContestId contestId, optional<string> newName,
                            optional<string> newDescription, optional<Tags> newTags,
                            set<ContestantId> deleteContestants, set<ContestantDescriptor> addContestants,
                            optional<Timestamp> newBegin, optional<Timestamp> newEnd) {
    // Require the contract's authority to modify a contest
    requireAuthorization(ownerAccount());

    // Static input validation, without regard to database state
    if (newName.has_value()) BAL::Verify(newName->length() > 0, "Contest name must not be empty");
    if (newTags.has_value()) {
        BAL::Verify(GoodTags(*newTags), "The new tags contains an invalid tag");
        BAL::Verify(newTags->size() <= 100, "Must not exceed 100 tags");
    }
    CheckContestants(addContestants);
    if (newBegin.has_value()) BAL::Verify(*newBegin >= currentTime(),
                                      "If modifying a contest begin date, the new begin date must not be in the past");

    // Check the group is valid
    BAL::Verify(getTable<PollingGroups>(GLOBAL.value).contains(groupId),
                "No such polling group: " + std::to_string(groupId.value));

    // Fetch the contest record
    Contests contests = getTable<Contests>(groupId);
    const auto& contest = contests.getId(contestId, "Referenced contest not found. Check token and contest IDs");
    // Check begin/end dates' validity
    if (contest.begin < currentTime()) {
        // Modifying a contest which has already begun. No change to begin date allowed
        BAL::Verify(!newBegin, "Cannot change contest begin date after contest has begun");
        // Cannot change name or description either
        BAL::Verify(!newName && !newDescription, "Cannot change contest name or description after contest has begun");
    }
    BAL::Verify(newEnd.value_or(contest.end) > newBegin.value_or(contest.begin),
          "Contest end date must be after begin date");

    // Get range for the existing contestants
    Contestants contestants = getTable<Contestants>(groupId);
    auto contestantsByContest = contestants.getSecondaryIndex<BY_CONTEST>();
    auto contestantsRange = contestantsByContest.range(Contestant::contestKeyMin(contestId),
                                                       Contestant::contestKeyMax(contestId));
    size_t totalContestants = std::distance(contestantsRange.first, contestantsRange.second);
    // Size checks on the contestant counts
    BAL::Verify(totalContestants >= deleteContestants.size(),
            "Set of contestants to delete is larger than the total number of contestants");
    BAL::Verify(totalContestants - deleteContestants.size() + addContestants.size() > 1,
            "At least two contestants must be defined");

    // Delete the removed contestants
    while (contestantsRange.first != contestantsRange.second && !deleteContestants.empty()) {
        auto removedContestantId = contestantsRange.first->id;
        // Check if the inspected contestant is one to delete
        auto toDeleteItr = deleteContestants.find(removedContestantId);
        // If it is, delete it and remove it from the set to delete. Either way, move to inspect next contestant
        if (toDeleteItr != deleteContestants.end()) {
            // Remove any decisions referencing the deleted contestant
            DeleteDecisionsByContestant(*this, groupId, contestId, removedContestantId);
            // Remove the contestant
            UpdateJournal(*this, CONTESTANTS, groupId, removedContestantId, ModificationType::deleteRow);
            contestantsRange.first = contestantsByContest.erase(contestantsRange.first);
            deleteContestants.erase(toDeleteItr);
        } else {
            ++contestantsRange.first;
        }
    }
    BAL::Verify(deleteContestants.empty(), "Set of contestants to delete contained contestants not in the contest");
    // Add the new contestants
    AddContestants(*this, contestants, contestId, std::move(addContestants));

    // Update the contest
    contests.modify(contest, [&newName, &newDescription, &newTags, &newBegin, &newEnd](Contest& c) {
        if (newName.has_value()) c.name = std::move(*newName);
        if (newDescription.has_value()) c.description = std::move(*newDescription);
        if (newTags.has_value()) c.tags = std::move(*newTags);
        if (newBegin.has_value()) c.begin = *newBegin;
        if (newEnd.has_value()) c.end = *newEnd;
    });
    UpdateJournal(*this, CONTESTS, groupId, contest.id, ModificationType::modifyRow);

    // If, due to some bug, there are already decisions on this contest, delete them.
    DeleteContestDecisions(*this, groupId, contestId);
    // Same goes for write-in contestants
    DeleteContestWriteIns(*this, groupId, contestId);
}

void Pollaris::deleteContest(GroupId groupId, ContestId contestId) {
    // Require the contract's authority to delete a contest
    requireAuthorization(ownerAccount());

    // Check group validity
    BAL::Verify(getTable<PollingGroups>(GLOBAL.value).contains(groupId),
                "No such polling group: " + std::to_string(groupId.value));

    // Check contest exists
    Contests contests = getTable<Contests>(groupId);
    auto& c = contests.getId(contestId, "Cannot delete contest: contest does not exist. Please check contest ID");

    // Delete results, decisions, contestants, and write-ins
    DeleteContestResults(*this, groupId, contestId);
    DeleteContestDecisions(*this, groupId, contestId);
    DeleteContestContestants(*this, groupId, contestId);
    DeleteContestWriteIns(*this, groupId, contestId);

    // Finally, delete the contest itself
    UpdateJournal(*this, CONTESTS, groupId, c.id, ModificationType::deleteRow);
    contests.erase(c);
}

void Pollaris::tallyContest(GroupId groupId, ContestId contestId) {
    // Require the contract's authority to tally a contest
    requireAuthorization(ownerAccount());

    // Check group ID validity
    BAL::Verify(getTable<PollingGroups>(GLOBAL.value).contains(groupId),
                "No such polling group: " + std::to_string(groupId.value));

    // Get contest
    Contests contests = getTable<Contests>(groupId);
    const auto& contest = contests.getId(contestId, "Unable to tally contest: contest does not exist. "
                                                    "Please check contest ID");

    // Tally up the results. From this point, we should NOT fail the transaction!
    TallyResults results;
    results = TallyContest(*this, groupId, contestId,
                           [table=getTable<GroupAccounts>(groupId), &contest](const Decision& decision)
                            -> TallyResults {
        TallyResults result;
        // If decision timestamp is not within range, decision is not counted
        if (decision.timestamp < contest.begin || decision.timestamp > contest.end) {
            BAL::Log("Ignoring decision ID", uint64_t(decision.id), "because timestamp is out of range");
            return {};
        }
        // If voter is not in polling group, he gets no vote
        auto voterItr = table.findId(decision.voter);
        if (voterItr == table.end()) {
            BAL::Log("Ignoring decision ID", uint64_t(decision.id), "because voter is not in polling group");
            return {};
        }
        // If voter has no voting weight, he gets no vote
        if (voterItr->weight <= 0) {
            BAL::Log("Ignoring decision ID", uint64_t(decision.id), "because voter has no voting weight");
            return {};
        }

        // Voter can split his voting weight across multiple candidates
        uint64_t totalWeight = 0;
        bool noSplit = contest.tags.count(NO_SPLIT_TAG);
        for (const auto& o : decision.opinions) {
            if (o.second > 0) {
                // If vote splitting is prohibited, check that this is the first positive opinion
                if (noSplit && totalWeight != 0) {
                    BAL::Log("Ignoring decision ID", uint64_t(decision.id),
                             "because decision splits its vote, but contest does not permit split votes.");
                    return {};
                }
                totalWeight += o.second;
                result[o.first] = o.second;
            }
        };
        if (totalWeight <= 0) {
            BAL::Log("Ignoring decision ID", uint64_t(decision.id), "because no contestant was selected");
            return {};
        }
        if (totalWeight != voterItr->weight) {
            // Total weight does not match voter weight. Check if the difference is a partial abstain vote
            // We do not call GetAbstainWeight() because it might fail the transaction,
            // and once a tally starts it should not fail.
            bool foundTag = false;
            if (totalWeight < voterItr->weight && contest.tags.count(NO_ABSTAIN_TAG) == 0) {
                auto expectedTag = PARTIAL_ABSTAIN_VOTE_TAG_PREFIX + std::to_string(voterItr->weight - totalWeight);
                foundTag = decision.tags.count(expectedTag);
            }
            if (!foundTag) {
                // Did not find a partial abstain tag to rectify the decision. Disqualify it.
                BAL::Log("Ignoring decision ID", uint64_t(decision.id),
                         "because opinion sum does not equal voter's weight");
                return {};
            }
        }

        // Return the vote
        return result;
    });

    // Store the results
    StoreContestResults(*this, groupId, contestId, std::move(results));
}

void Pollaris::setDecision(GroupId groupId, ContestId contestId, AccountHandle voter, FullOpinions opinions,
                           Tags tags) {
    // Require the voter's authority to set a decision
    requireAuthorization(voter);

    // Check the tags
    BAL::Verify(tags.size() <= 100, "Unable to set decision: too many tags");
    BAL::Verify(GoodTags(tags), "Unable to set decision: a tag was invalid");
    bool abstainVote = tags.count(ABSTAIN_VOTE_TAG);
    auto abstainWeight = GetAbstainWeight(tags);
    BAL::Verify(!(abstainVote && abstainWeight > 0),
                "Unable to set decision: decision cannot both fully and partially abstain");
    if (abstainVote)
        BAL::Verify(opinions.writeInOpinions.empty() && opinions.contestantOpinions.empty(),
                    "Unable to set decision: abstain decisions cannot specify opinions");

    // Check that write-ins are reasonable
    BAL::Verify(opinions.writeInOpinions.size() <= 16,
                "Unable to set decision: only up to 16 write-in candidates allowed");
    for (const auto& pair : opinions.writeInOpinions)
        CheckContestant(pair.first);

    // Check group validity
    BAL::Verify(getTable<PollingGroups>(GLOBAL.value).contains(groupId),
                "No such polling group: " + std::to_string(groupId.value));

    // Check voter eligibility
    GroupAccounts group = getTable<GroupAccounts>(groupId);
    const auto& voterEntry = group.getId(voter, "Unable to set decision: voter is not a member of polling group");

    // Get contest
    Contests contests = getTable<Contests>(groupId);
    const auto& contest = contests.getId(contestId, "Unable to set decision: contest not found. "
                                                    "Please check contest ID");
    if (abstainVote || abstainWeight)
        BAL::Verify(contest.tags.count(NO_ABSTAIN_TAG) == 0,
                    "Unable to set decision: decision abstains, but contest does not permit abstain votes");
    bool noSplit = contest.tags.count(NO_SPLIT_TAG);
    // If vote splitting is prohibited, check opinions specify exactly one candidate
    if (noSplit) {
        BAL::Verify(opinions.writeInOpinions.size() + opinions.contestantOpinions.size() == 1,
                    "Unable to set decision: contest type requires decision to specify exactly one opinion");
        BAL::Verify(abstainWeight == 0,
                    "Unable to set decision: decision partially abstains, contest prohibits vote splitting");
    }

    // Check that we're in contest period
    Timestamp now = currentTime();
    BAL::Verify(now >= contest.begin && now <= contest.end,
                "Unable to set decision: time is not during contest period");

    // Check that all referenced contestants exist and belong to this contest
    uint64_t totalOpinions = 0;
    Contestants contestants = getTable<Contestants>(groupId);
    for (const auto& pair : opinions.contestantOpinions) {
        BAL::Verify(pair.second > 0, "The voting weight for a decision should be positive");
        const auto& contestant = contestants.getId(pair.first,
                                                   "Unable to set decision: opinion contestant does not exist");
        BAL::Verify(contestant.contest == contestId,
                    "Unable to set decision: opinion contestant belongs to different contest");
        totalOpinions += pair.second;
    }

    // Create opinions structure for decision by converting explicit write-in contestants to IDs
    Opinions storedOpinions(opinions.contestantOpinions.begin(), opinions.contestantOpinions.end());
    for (const auto& pair : opinions.writeInOpinions) {
        // Check negative votes before retaining the write-in
        // Votes for write-ins should be positive to avoid zero-vote spam
        BAL::Verify(pair.second > 0, "The voting weight for write-ins should be positive");
        auto writeInId = RetainWriteIn(*this, groupId, contestId, pair.first);
        storedOpinions[writeInId] = pair.second;
        totalOpinions += pair.second;
    }
    // Check sum of opinions equals voter's weight
    if (abstainVote) {
        // Full abstention
        // Verification of zero opinions was already checked above
        // BAL::Verify(opinions.writeInOpinions.empty() && opinions.contestantOpinions.empty() ...
    } else if (abstainWeight)
        // Partial abstention
        BAL::Verify((totalOpinions + abstainWeight) == voterEntry.weight,
                    "Unable to set decision: sum of opinions and abstaining weight does not equal voter's weight");
    else
        // No abstention
        BAL::Verify(totalOpinions == voterEntry.weight,
                    "Unable to set decision: sum of opinions does not equal voter's weight");

    // Finally, update the database for the new decision
    // If opinions is empty and decision is not an abstain vote, delete existing decision
    Decisions decisions = getTable<Decisions>(groupId);
    auto decisionsByContest = decisions.getSecondaryIndex<BY_CONTEST>();
    if (!abstainVote && storedOpinions.empty()) {
        auto& decision = decisionsByContest.get(MakeCompositeKey(contestId, voter),
                                          "Unable to delete decision: existing decision not found");
        UpdateJournal(*this, DECISIONS, groupId, decision.id, ModificationType::deleteRow);
        decisionsByContest.erase(decision);
    } else {
        // We are storing a decision; does one already exist?
        auto itr = decisionsByContest.find(MakeCompositeKey(contestId, voter));
        if (itr == decisionsByContest.end()) {
            // No, so create a new one
            auto id = decisions.nextId();
            decisions.create([id, &contestId, voter, now, &storedOpinions, &tags](Decision& decision) {
                decision.id = id;
                decision.contest = contestId;
                decision.voter = voter;
                decision.timestamp = now;
                decision.opinions = std::move(storedOpinions);
                decision.tags = std::move(tags);
            });
            UpdateJournal(*this, DECISIONS, groupId, id, ModificationType::addRow);
        } else {
            // Yes, so update it in place, but first release any write-ins it might have
            for (const auto& pair : itr->opinions)
                if (std::holds_alternative<WriteInId>(pair.first))
                    ReleaseWriteIn(*this, groupId, std::get<WriteInId>(pair.first));
            decisionsByContest.modify(itr, [now, &storedOpinions, &tags](Decision& decision) {
                decision.timestamp = now;
                decision.opinions = std::move(storedOpinions);
                decision.tags = std::move(tags);
            });
            UpdateJournal(*this, DECISIONS, groupId, itr->id, ModificationType::modifyRow);
        }
    }
}
