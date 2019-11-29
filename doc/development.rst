.. include:: /global.rst

============
 Developing
============

|dogfood| is mainly written in C++ and aims to use the latest C++ standard.
This implies that a fairly recent compiler is needed. In order to avoid
unnecessary manual labour, `Docker <https://www.docker.com>`_ containers are
used to take care of dependencies and provide a stable build environment on any
modern Linux system.

Creating the build image
-------------------------

The Docker build container is made of two pieces:

- The toolchain, which consists of the compiler, assembler, linker and other utilities
- The build image, building on top of the toolchain, containing extra utilities and tools

You can build the required build images by executing the following command:

.. code-block:: console

  ~/dogfood$ ./create-docker-containers.sh

Which will yield a Docker container ``dogfood-buildimage`` with all the
necessary prerequisites.

Building the distribution
--------------------------

Once you have created the prerequisite the container, you enter it using the
following command:

.. code-block:: console

  ~/dogfood$ docker run -it -v $PWD:/work --rm dogfood-buildimage

You can then build |dogfood| using the following commands:

.. code-block:: console

  build@docker:/$ cd /work
  build@docker:/work$ ./build.sh

Running the result
-------------------

Upon successful completion, the ``images/`` directory contains the following
files:

- ``ext2.img``: ext2fs filesystem image of the root filesystem
- ``kernel.iso``: bootable ISO image containg the kernel and bootloader
- ``run.sh``: script to launch a `qemu <https://www.qemu.org>`_ emulator with the two images

|dogfood| does not currently support a VGA console; this means that any
interaction is to be performed using the serial port (mapped to the standard in/output
in the sample ``run.sh`` script)

You can start your freshly-built |dogfood|-distribution by executing:

.. code-block:: console

   ./run.sh
