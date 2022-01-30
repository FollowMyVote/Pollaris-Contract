FROM ubuntu:18.04
LABEL maintainer="Nathan Hourt <nathan@followmyvote.com>"

RUN /bin/bash -c "apt-get update; \
    apt-get -y upgrade; \
    apt-get -y install zsh vim git sudo; \
    chsh -s /bin/zsh root; \
    mkdir /src; \
    cd /src; \
    git clone --recursive --depth=1 https://github.com/eosio/eosio.cdt; \
    cd eosio.cdt; \
    yes 1 | ./scripts/eosiocdt_build.sh; \
    ./scripts/eosiocdt_install.sh; \
    cd /root; \
    rm -rf /src; \
    echo 'bindkey -v' > /root/.zshrc;"

ENV PATH="$PATH:/root/opt/eosio.cdt/bin"
SHELL ["/bin/zsh"]
ENTRYPOINT /bin/zsh
