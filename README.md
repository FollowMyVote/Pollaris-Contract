# Pollaris
This repository contains the smart contract that forms the backend of Pollaris, Follow My Vote's blockchain-based polling system.

### Build Instructions
The contract is built using CMake, and will build for EOSIO if the EOSIO CDT is found by CMake, and/or for Peerplays if the Peerplays libraries are found. By default,
the build system looks for Peerplays in /opt/peerplays, but this can be overridden by passing `-DPEERPLAYS_PATH=/path/to/peerplays` to CMake.

Testing concerns are located in the Tests directory; however, the tests can be run via the build system by building the `test` target.

#### Debugging note
For EOSIO contracts, if the contract is failing to execute and EOSIO is not printing logs, try reimplementing BAL::Verify to avoid calling `eosio::check()` but
instead do `if (!condition) { eosio::print(message); eosio::eosio_exit(0); }` (for debugging only, don't deploy this!). It seems that EOSIO does not always print
debugging information if the contract fails, but only if it succeeds.
