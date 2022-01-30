FROM containers.followmyvote.com:443/pollaris/peerplays-node/master:latest
LABEL maintainer="Nathaniel Hourt <nathan@followmyvote.com>"

RUN mkdir /Node/src
COPY --chown=node:node . /Node/src/pollaris

RUN /bin/bash -ec "mkdir /Node/src/build; \
    cd /Node/src/build; \
    cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja ../pollaris; \
    ninja pollarisContractPeerplays; \
    mkdir /Node/plugins; \
    cp BAL/Peerplays/libpollarisContractPeerplays.so /Node/plugins; \
    cd /Node; \
    rm -rf /Node/src;"
