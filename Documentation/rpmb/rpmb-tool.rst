==========
RPMB Tool
==========

There is a sample rpmb tool under tools/rpmb/ directory that exercises
the RPMB devices via RPMB character devices interface (/dev/rpmbX)

.. code-block:: none

        rpmb [-v] [-r|-s] <command> <args>

        rpmb program-key [-<RPMB_DEVICE> <KEY_FILE>
        rpmb write-counter <RPMB_DEVICE> [KEY_FILE]
        rpmb write-blocks <RPMB_DEVICE> <address> <block_count> <DATA_FILE> <KEY_FILE>
        rpmb read-blocks <RPMB_DEVICE> <address> <blocks count> <OUTPUT_FILE> [KEY_FILE]

        rpmb -v/--verbose:  runs in verbose mode
        rpmb -s/--sequence: use RPMB_IOC_SEQ_CMD
        rpmb -r/--request:  use RPMB_IOC_REQ_CMD
