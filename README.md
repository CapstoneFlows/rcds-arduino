# rcds-arduino

CarCounting is the firmware used for the RCDS.

Below is the set of instructions and responses for the firmware, in order of how you will see them. C means a command, R means a response from the device. Anything with <command>_ACK is an acknowledgment that it received the command, and will come after any issued command.

Regarding time:
R: ASCII Character 7 (BEL) repeated
   Device is looking for time sync
C: T<Unix time>
   Sets the device time
R: TIME_ACK

Regarding system variables:
R: NEED_VARS
   Indicates that the device does not have a copy of device variables (ID, LOC, DIR, COMMENT).
C: ID=<device ID>
R: ID_ACK
C: LOC=<location>
R: LOC_ACK
C: DIR=<direction>
R: DIR_ACK
C: COMMENT=<comment>
R: COMMENT_ACK

Regarding control:
R: READY
   Device is ready to start, stop, return, and reset. Will only come if NEED_VARS did not, or if all the variables (ID, LOC, DIR, COMMENT) were set after NEED_VARS.
C: START_RUNNING
R: START_ACK
C: STOP_RUNNING
R: STOP_ACK

Regarding data return:
C: RETURN_DATA
R: RETURN_ACK
R: NOT_STOPPED
   Means that you did not stop the program prior to calling for return. Comes instead of BEGIN_TRANSFER.
R: BEGIN_TRANSFER
Indicates the beginning of all file transfers.
R: BEGIN_FILE=<filename>
   Indicates the beginning of a file transfer, plus the filename.
R: END_FILE
   Indicates the end of a file transfer.
R: END_TRANSFER
   Indicates the end of all file transfers.

Regarding device reset:
C: RESET_DEVICE
   Will reset variables and wipe the SD card.
R: RESET_ACK
R: NOT_STOPPED
Same as with return, but with reset instead. Comes instead of RESET_COMPLETE.
R: RESET_COMPLETE
   Only after this is the device considered to be reset.

Error conditions:
R: SD_INIT_ERROR
The SD card failed to initialize during init; usually means no SD card in device.
R: SD_VAR_ERROR
The SD card failed to save the device variables set during init.
R: SD_FILE_ERROR
The SD card failed to open and load the data file to write to during init.
R: SD_SWAP_ERROR
The SD card ran into an issue swapping the data files during runtime.
R: SD_WRITE_ERROR
The SD card ran into an issue when trying to write data to the file during runtime.
R: SD_RESET_ERROR
The SD card ran into an issue when wiping the SD card during reset. May require reformatting.
R: SD_REINIT_ERROR
The SD card was unable to reinitialize after wiping during reset. May require reformatting.