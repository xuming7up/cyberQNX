ARG IMAGE
FROM $IMAGE

ARG USERNAME=jenkins
ARG USER_UID=1000
ARG USER_GID=0
ARG HOME=/home/jenkins

ENV USER=${USERNAME}
ENV HOME=${HOME}
ENV TERM=xterm-256color

# root user is required for some commands in the following RUN section.
# the container of this image should then be created with user '$USERNAME'.
USER root

# Create the default user with the same UID as the host user. GID is 'root' that we can access all existing files in the container.
# install some usefull applications with apt
# install conan config and login to all remotes
# set the default conan profile for this container
# remove tmp files
# change the owner of the home directory (in the CI the UID for jenkins user is different)
# add the user to the sudo group, as this is required to execute the Axivion scripts
RUN useradd -m --uid ${USER_UID} --gid ${USER_GID} -o -s /bin/bash ${USERNAME} \
    && (apt update || true) \
    && apt install -y --force-yes --no-install-recommends sshpass vim git-lfs libtool autoconf\
    && apt install -y --force-yes libprotobuf-dev protobuf-compiler \
    && chown -R ${USER_UID} ${HOME} \
    && usermod -aG sudo ${USERNAME} \
    && echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers \
    && echo 'source /opt/qnx710/qnxsdp-env.sh' >> ~/.bashrc \
    && chown -R ${USER_UID} /drive
