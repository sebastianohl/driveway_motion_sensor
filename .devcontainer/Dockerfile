ARG VARIANT="bullseye"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util screen
RUN export DEBIAN_FRONTEND=noninteractive && apt-get remove --purge -y python python2.7 && apt-get autoremove --purge -y
RUN ln -s /usr/bin/python3 /usr/bin/python
#RUN ln -s /usr/bin/pip3 /usr/bin/pip

RUN mkdir -p /esp
RUN cd /esp && git clone -b v4.4 --recursive https://github.com/espressif/esp-idf.git 
RUN su - vscode -c "cd /esp/esp-idf && ./install.sh esp32"
RUN echo ". /esp/esp-idf/export.sh" >> /home/vscode/.profile
RUN cd /esp/esp-idf/components && git clone -b v2.0.0 https://github.com/espressif/esp32-camera.git

RUN export DEBIAN_FRONTEND=noninteractive && apt-get install -y usbutils clang-format-11
RUN su - vscode -c "pip uninstall -y cryptography"
RUN su - vscode -c "pip install cryptography==3.4.8"



