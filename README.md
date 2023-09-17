# Pollaris
This repository contains the smart contract that forms the backend of Pollaris, Follow My Vote's blockchain-based polling system.

## Building
Building the Pollaris smart contract requires a suitable build environment.  Instructions for creating such a build environment can be found [here](https://github.com/dapp-protocols/Blockchain-Abstraction-Layer/blob/master/DevEnv.md).

### Build Instructions
The contract is built using CMake, and will build for Leap if the Leap CDT is found by CMake.

Testing concerns are located in the Tests directory; however, the tests can be run via the build system by building the `test` target.

#### Debugging note
For Leap contracts, if the contract is failing to execute and the node is not printing logs, try reimplementing BAL::Verify to avoid calling `eosio::check()` but
instead do `if (!condition) { eosio::print(message); eosio::eosio_exit(0); }` (for debugging only, don't deploy this!). It seems that Leap does not always print
debugging information if the contract fails, but only if it succeeds.
