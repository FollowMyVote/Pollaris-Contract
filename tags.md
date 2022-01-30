Tags can be used to alter the meaning of contests, decisions, and other records and affect the way the backend and/or frontends process them.

The following tags are recognized by the backend:

 - Contest tags
   - `no-split-vote` Prohibits decisions that split their voting weight between multiple contestants
   - `no-abstain` Prohibits decisions that fully or partially abstain from voting
 - Decision tags
   - `abstain` Declares that the decision votes present/abstain and should not affect the tally
     - Decision must have no opinions
     - Contest must not specify `no-abstain` tag
   - `partial-abstain:###` Declares that the decision votes some of its weight as present/abstain.
     - The `###` must be a number which equals the amount of weight that abstains
     - The total of the abstaining weight and the opinions must equal the voter's weight exactly
     - Contest must not specify `no-abstain` tag nor `no-split-vote` tag