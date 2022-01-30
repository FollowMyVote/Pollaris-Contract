#!/bin/bash

IMAGE_PREFIX="containers.followmyvote.com:443/pollaris"

# Set up a temporary directory and unique job ID
TMP=`mktemp -d` || reject "Could not create temporary directory"
ID=`basename $TMP | cut -d . -f 2`
[ "$ID" != "" ] || reject "Could not extract ID from directory name"

# Kudos to https://stackoverflow.com/a/24597941
function reject {
    printf '\n\nTest FAILED: %s\n' "$1" >&2  ## Send message to stderr. Exclude >&2 if you don't want it that way.

    [ "$TMP" != "" ] && printf 'Preserving test data folders in %s\n' "$TMP" >&2
    docker inspect "testnet-$ID" 2>&1 > /dev/null  && printf "Leaving testnet container running: testnet-$ID\n"
    docker inspect "keosd-$ID" 2>&1 > /dev/null  && printf "Leaving keosd container running: keosd-$ID\n"
    # If testnet logging is still going to console, stop it
    jobs %% && kill %1

    exit "${2-1}"  ## Return a code specified by $2 or 1 by default.
}

# Always be proactive at detecting obvious failure modes and giving good errors
docker version > /dev/null || reject "Docker sanity check failed; is docker installed and running? Do we have permission to access it?"

# Pull or build eosio image
if docker inspect "${IMAGE_PREFIX}/eosio-blockchain/master" > /dev/null; then
    echo Using local EOSIO image
else
    if docker login containers.followmyvote.com:443 < /dev/null; then
        echo Pulling EOSIO image
        docker pull "${IMAGE_PREFIX}/eosio-blockchain/master/master"
    fi

    if docker inspect "${IMAGE_PREFIX}/eosio-blockchain/master" > /dev/null; then
        echo EOSIO image pulled OK
    else
        echo Could not pull EOSIO image. Building it instead.
        pushd $TMP
        git clone https://gitlab.followmyvote.com/pollaris/eosio-blockchain || reject "Could not clone EOSIO image repo"
        pushd eosio-blockchain
        docker build -t "${IMAGE_PREFIX}/eosio-blockchain/master" . || reject "Could not build EOSIO image"
        popd
        rm -rf eosio-blockchain
        popd
    fi
fi

# Pull or build eosio-cdt image
if docker inspect "${IMAGE_PREFIX}/eosio-cdt" > /dev/null; then
    echo Using local EOSIO-CDT image
else
    if docker login containers.followmyvote.com:443 < /dev/null; then
        echo Pulling EOSIO-CDT image
        docker pull "${IMAGE_PREFIX}/eosio-cdt/master"
    fi

    if docker inspect "${IMAGE_PREFIX}/eosio-cdt/master" > /dev/null; then
        echo EOSIO-CDT image pulled OK
    else
        echo Could not pull EOSIO-CDT image. Building it instead.
        pushd $TMP
        git clone https://gitlab.followmyvote.com/pollaris/eosio-cdt || reject "Could not clone EOSIO-CDT image repo"
        pushd eosio-cdt
        docker build -t "${IMAGE_PREFIX}/eosio-cdt/master" . || reject "Could not build EOSIO-CDT image"
        popd
        rm -rf eosio-cdt
        popd
    fi
fi

# Build contract into temp dir
[ -f ../Contract/Pollaris.cpp ] || reject "Couldn't find ../Contract/Pollaris.cpp; has it been renamed? Is script running in Tests directory?"
docker run --rm -t -v $PWD/..:/src -v $TMP:/dst --entrypoint eosio-cpp -w /dst "${IMAGE_PREFIX}/eosio-cdt/master" -DBAL_PLATFORM_EOSIO -I /src/Contract -I /src/BAL /src/Contract/Pollaris.cpp /src/Contract/Main.cpp -o /dst/Pollaris.wasm || reject "Could not build contract"
rm -f $TMP/Pollaris.abi
cp tests-only.abi $TMP/ || "Couldn't copy tests-only.abi to test data folder; is it missing? Do we still have permission to write data folder?"

# Start up the blockchain
docker run --rm -d -v $PWD:/config --name "testnet-$ID" "${IMAGE_PREFIX}/eosio-blockchain/master" --max-transaction-time 255 || reject "Could not start testnet"

# Start keosd, put sock in $TMP/keosd/keosd.sock.
# Create keosd directories a bit before, so we can start it as non-root. Otherwise, it creates files we can't delete
# during cleanup.
mkdir -p $TMP/keosd/wallets || reject "Could not create wallet directory (Necessary to start keosd as non-root)"
docker run --rm -d --name "keosd-$ID" --user $UID:$GID --entrypoint keosd -v $TMP/keosd:/keosd \
    -e HOME=/keosd "${IMAGE_PREFIX}/eosio-blockchain/master" --data-dir=/keosd --wallet-dir=wallets || reject "Could not start keosd"

# Set up the wallet via cleos
CLEOS="docker run --rm --entrypoint cleos -v $TMP:/contract --network=container:testnet-$ID ${IMAGE_PREFIX}/eosio-blockchain/master --wallet-url unix:///contract/keosd/keosd.sock"
WIF=`$CLEOS wallet create --to-console | tail -n1 | sed 's/"//g'`
[ "$WIF" != "" ] || reject "Could not create wallet (WIF is '$WIF')"
$CLEOS wallet import --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3 || reject "Could not import blockchain key to wallet"
CONTRACT_PUBKEY=`$CLEOS wallet create_key | cut -d\" -f2`
[ "$CONTRACT_PUBKEY" != "" ] || reject "Could not create key for contract account"

# Create the contract
$CLEOS create account eosio pollaris $CONTRACT_PUBKEY || reject "Could not create contract account"
$CLEOS set contract pollaris /contract Pollaris.wasm tests-only.abi || reject "Could not set contract code"

# Show backend logging during contract execution
docker logs -f testnet-$ID --tail 0 &
jobs %% || printf "Unable to display logging from back-end during contract execution; attempting to continue anyways"

# Run the run_tests action
$CLEOS push action --max-cpu-usage-ms 255 --json-file /contract/contract-run.json pollaris tests.run '{}' -p pollaris@active || reject "Unable to successfully execute contract test"

# Shut down the daemons and clean up
docker stop "testnet-$ID" || printf "Unable to shut down testnet container: testnet-$ID"
docker stop "keosd-$ID" || printf "Unable to shut down keosd container: testnet-$ID"
rm -rf $TMP || reject "Unable to clean up data folder: $TMP"

# Report status
echo "Test PASSED"
exit 0
