#!/bin/bash

IMAGE_PREFIX="containers.followmyvote.com:443/pollaris"

function tolower {
    echo "$1" | tr '[:upper:]' '[:lower:]'
}

# Set up a temporary directory and unique job ID
TMP=`mktemp -d` || reject "Could not create temporary directory"
ID=`basename $TMP | cut -d . -f 2`
[ "$ID" != "" ] || reject "Could not extract ID from directory name"

CONTRACT_IMAGE="pollaris-test-$(tolower $ID)"
NETWORK_NAME="pollaris-testnet-$ID"

# Kudos to https://stackoverflow.com/a/24597941
function reject {
    echo -e "\n\n\n#######\n\nTest FAILED: $1\n\n#######\n\n\n" >&2  ## Send message to stderr. Exclude >&2 if you don't want it that way.

    [ "$TMP" != "" ] && printf 'Preserving test data folders in %s\n' "$TMP" >&2
    docker inspect $CONTRACT_IMAGE > /dev/null && printf 'Preserving contract image %s\n' "$CONTRACT_IMAGE" >&2

    # Shut down containers
    docker stop "contract-$ID" "bp-$ID" "wallet-$ID"
    
    # Delete testnet docker network
    docker network inspect $NETWORK_NAME > /dev/null && docker network rm $NETWORK_NAME

    exit "${2-1}"  ## Return a code specified by $2 or 1 by default.
}

# Always be proactive at detecting obvious failure modes and giving good errors
docker version > /dev/null || reject "Docker sanity check failed; is docker installed and running? Do we have permission to access it?"
[ -n `which curl` ] || reject "Curl must be available on the host system"

# Pull or build an image
function prepareImage {
    IMAGE=$1
    REPO=$2

    [[ -n "$IMAGE" ]] || reject "Script error: prepareImage called without image argument"

    # Check if image is already available locally.
    if docker inspect "$IMAGE" > /dev/null; then
        echo Using local $IMAGE image
        return
    fi

    # Image is not available. Try to pull it.
    if docker login containers.followmyvote.com:443 < /dev/null; then
        echo Pulling $IMAGE image
        docker pull "$IMAGE"
    fi

    # Do we have it OK?
    if docker inspect "$IMAGE" > /dev/null; then
        echo $IMAGE image pulled OK
        return
    fi

    # Can't pull it either. Do we have the repo?
    [[ -n "$REPO" ]] || reject "Could not prepare image $IMAGE: unable to pull, and no repo given to build it from"

    pushd $TMP
    git clone $REPO image-build || reject "Could not prepare image $IMMAGE: unable to pull, and failed to clone repo"
    pushd image-build
    docker build -t "$IMAGE" . || reject "Could not prepare image $IMAGE: unable to pull, and build failed"
    popd
    rm -rf image-build
    popd
}

# Pull or build images
prepareImage "${IMAGE_PREFIX}/peerplays-dapp-support/master" "https://gitlab.followmyvote.com/pollaris/peerplays-dapp-support"
prepareImage "${IMAGE_PREFIX}/peerplays-node/master" "https://gitlab.followmyvote.com/pollaris/peerplays-node"

# Build contract image
docker build -t $CONTRACT_IMAGE -f ../pollaris-contract.Dockerfile .. || reject "Failed to build contract"

# Create testnet network
docker network create $NETWORK_NAME || reject "Could not create testnet docker network"

# Spin up contract node
mkdir $TMP/contract-node-data-dir || reject "Could not create contract node data dir"
docker run --rm -d --name "contract-$ID" -v "$TMP/contract-node-data-dir:/Node/.config/Follow My Vote/PollarisBackend" --network $NETWORK_NAME --network-alias contract-node $CONTRACT_IMAGE || reject "Could not start contract node"
sleep 1
docker logs "contract-$ID" 2>&1 | grep "Contract Pollaris initialized successfully." || reject "Contract node started, but contract plugin failed to load:\n$(docker logs contract-$ID 2>&1 | grep -A1 'Failed to load plugin: /Node/plugins/libPollarisContractPeerplays.so')"

# Spin up BP node
docker run --rm -d --name "bp-$ID" -v "$TMP/contract-node-data-dir/genesis.json:/genesis.json" --network $NETWORK_NAME --network-alias bp-node "${IMAGE_PREFIX}/peerplays-dapp-support/master" --genesis-json /genesis.json -s contract-node:2776 || reject "Could not start block producer node"

# Read chain configuration parameters out of log
sleep 3
docker logs "contract-$ID" 2> $TMP/contract-node-logs
CONNECTED=`grep "Received next block in chain" $TMP/contract-node-logs`
[ -n "$CONNECTED" ] || reject "Blockchain nodes did not connect"
GENESIS_KEY_WIF=`grep "Configuring genesis with key" $TMP/contract-node-logs | cut -d, -f2 | cut -d'"' -f2`
GENESIS_KEY_PUB=`grep "Configuring genesis with key" $TMP/contract-node-logs | cut -d'"' -f2`
[ -n "$GENESIS_KEY_WIF" ] || reject "Could not read genesis key"
CHAIN_ID=`grep "Chain ID is" $TMP/contract-node-logs | cut -d']' -f 2 | cut -d' ' -f5` 
[ -n "$CHAIN_ID" ] || reject "Could not read chain ID"
echo Chain ID is $CHAIN_ID
rm $TMP/contract-node-logs

# Set up contract account
docker run --rm -d --name "wallet-$ID" --network $NETWORK_NAME --network-alias wallet --entrypoint /opt/peerplays/bin/cli_wallet "${IMAGE_PREFIX}/peerplays-dapp-support/master" --chain-id $CHAIN_ID -s ws://bp-node:8090 -r 0.0.0.0:8091 -d
WALLET_IP=`docker inspect wallet-$ID | grep IPAddress | grep -oE "\b([0-9]{1,3}\.){3}[0-9]{1,3}\b" | cut -d '"' -f 4`
sleep 1

function rpc {
    CALL=$1
    ARGS=$2

    echo $CALL $ARGS >> $TMP/wallet_logs
    ERROR=`curl --data "{\"jsonrpc\": \"2.0\", \"method\": \"$CALL\", \"params\": $ARGS, \"id\": 1}" http://$WALLET_IP:8091/rpc 2>/dev/null | tee -a $TMP/wallet_logs | grep error`
    echo >> $TMP/wallet_logs
    [ -z "$ERROR" ] || reject "RPC Call to $CALL failed: $ERROR"
}

rpc "set_password" '["thisisatest"]'
rpc "unlock" '["thisisatest"]'
rpc "import_key" "[\"init\", \"$GENESIS_KEY_WIF\"]"
rpc "import_balance" "[\"init\", [\"$GENESIS_KEY_WIF\"], true]"
rpc "register_account" "[\"follow-my-vote\", \"$GENESIS_KEY_PUB\", \"$GENESIS_KEY_PUB\", \"init\", \"init\", 100, \"true\"]"
rpc "import_key" "[\"follow-my-vote\", \"$GENESIS_KEY_WIF\"]"
rpc "transfer" '["init", "follow-my-vote", "10000", "TEST", "", "true"]'
rpc "begin_builder_transaction" '[]'
rpc "add_operation_to_builder_transaction" '[0, [35, {"payer": "1.2.9", "required_auths": ["1.2.9"], "data": "636f6e74726163742d616374696f6e2f506f6c6c617269732d312e30732074657374732e72756e"}]]'
rpc "set_fees_on_builder_transaction" '[0, "TEST"]'
rpc "sign_builder_transaction" '[0, true]'

sleep 10
docker logs "contract-$ID" 2> $TMP/contract_logs
FINISHED=`grep "Finished tests" $TMP/contract_logs`
[ -n "$FINISHED" ] || reject "Contract failed to complete its test routine"

docker stop -t 1 "contract-$ID" "bp-$ID" "wallet-$ID"
docker network rm $NETWORK_NAME
docker rmi $CONTRACT_IMAGE
rm -rf $TMP

# Report status
echo "Test PASSED"
exit 0
