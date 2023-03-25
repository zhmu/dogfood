FROM dogfood-toolchain:latest
LABEL maintainer "Rink Springer <rink@rink.nu>"

# python for libcxx
# auto* for dash/coreutils
# coreutils: gettext, autopoint, texinfo
# xorriso
# for the bootable iso and e2fsprogs for the filesystem image
# we'll be building stuff locally so
# grub2 build needs python
# dosfstools, mtools to create/fill EFI partition
RUN apt-get update && apt-get upgrade -y && \
    apt-get install --no-install-recommends -y ninja-build \
    gettext autopoint texinfo autoconf automake make \
    gcc build-essential bison flex wget ca-certificates \
    xorriso e2fsprogs python3 qemu-system-x86 \
    dosfstools mtools && \
    rm -rf /var/lib/apt/lists/*

# Install GRUB2 ourselves; trying to install it in a Docker image causes all kinds
# of confusion
RUN     mkdir /tmp/grub2-build && cd /tmp/grub2-build && \
        wget "https://ftp.gnu.org/gnu/grub/grub-2.04.tar.xz" && \
        tar xf grub-2.04.tar.xz && \
        cd grub-2.04 && ./configure --prefix=/usr && \
        make -j16 && make install && \
        cd / && rm -rf /tmp/grub2-build

# Compile CMake ourselves; the version provided in stretch is too old for us (we need
# at least 3.9)
RUN     mkdir /tmp/cmake-build && cd /tmp/cmake-build && \
        wget "https://cmake.org/files/v3.12/cmake-3.12.2.tar.gz" && \
        tar xzf cmake-3.12.2.tar.gz && \
        cd cmake-3.12.2 && ./bootstrap -- -DCMAKE_BUILD_TYPE:STRING=Release && \
        make -j16 && make install && \
        cd / && rm -rf /tmp/cmake-build


# Create an unprivileged build user; we expect that the uid/gid matches the
# work directory on the host system (inspired by
# https://gist.github.com/alkrauss48/2dd9f9d84ed6ebff9240ccfa49a80662)
RUN groupadd -g 1000 -r build && \
    useradd -r -u 1000 -g build -d /home/build -s /sbin/nologin -c "Build user" build
ENV home=/home/build
USER build
