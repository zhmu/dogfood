.. include:: /global.rst

.. Overview

==============
 Introduction
==============

Around the summer of 2009, the `Ananas project <https://www.ananas-os.nl>`_ was
started, with a lofty goal of writing a complete operating system usable on
modern PC hardware. While this turned out to be very educational, it is still
a long way from being in an usable shape: one of the major reasons is the
complexity of current PC hardware and peripherals.

.. _goal:

Project goal
------------

The |dogfood| project embraces a different philosophy: rather than spending all
the time understanding and supporting the intricacies of PC hardware, the plan
is to avoid this complexity altogether. The intended destination platform is a
virtual machine and the most basic of hardware supported. This helps keep the
focus on the actual goal of the project: write a self-hosting operating system
in a decent amount of time.

License
--------

The |dogfood| kernel, header files and accompanying scripts are distributed
using the :ref:`zlib` license.

The C library is a fork of `newlib <https://sourceware.org/newlib/>`_, which
is a conglomeration of several library parts each under their own respective
free software license. All |dogfood|-specific pieces are licensed using the
:ref:`zlib` license.

External code, such as the GNU Compiler Connection (GCC), GNU Binutils,
GNU Coreutils, dash and all others are licensed using their own terms. Any
|dogfood|-specific modifications are licensed using the :ref:`zlib` license
if applicable.

This documentation is licensed under a `Creative Commons Attribution 2.0
Generic License <https://creativecommons.org/licenses/by/2.0/>`_.

.. _zlib:

Zlib license
-------------

.. literalinclude:: ../LICENSE
