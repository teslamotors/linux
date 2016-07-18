======================
RPMB Simulation Device
======================

RPMB partition simulation device is a virtual device that
provides simulation of the RPMB protocol and uses kernel memory
as storage.

This driver cannot promise any real security, it is suitable for testing
of the RPMB subsystem it self and mostly it was found useful for testing of
RPMB applications prior to RPMB key provisioning/programming as
The RPMB key programming can be performed only once in the life time
of the storage device.

Implementation:
---------------

.. kernel-doc:: drivers/char/rpmb/rpmb_sim.c

