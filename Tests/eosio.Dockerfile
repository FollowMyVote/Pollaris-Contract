FROM centos:7
LABEL maintainer="Nathan Hourt <nathan@followmyvote.com>"

RUN /bin/bash -c "yum update; \
    yum upgrade -y; \
    yum install -y vim zsh git; \
    chsh -s /bin/zsh root; \
    mkdir /src; \
    cd /src; \
    git clone --recursive --depth=1 https://github.com/eosio/eos; \
    cd eos; \
    ./scripts/eosio_build.sh -i /opt/eosio -y; \
    ./scripts/eosio_install.sh; \
    cd /root; \
    rm -rf /src /opt/eosio/src; \
    echo 'bindkey -v' > /root/.zshrc; \
    mkdir /config;"

COPY logging.json /config

ENV PATH="$PATH:/opt/eosio/bin"
SHELL ["/bin/zsh"]

EXPOSE 8888
ENTRYPOINT ["/opt/eosio/bin/nodeos", "-e", "-p", "eosio", "-l", "/config/logging.json", "--plugin", "eosio::http_plugin", "--plugin", "eosio::chain_api_plugin", "--contracts-console", "--signature-provider", "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV=KEY:5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3", "--http-server-address", "0.0.0.0:8888"]
