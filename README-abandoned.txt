The state of this repo is the result of the rapid implementation of a bunch of changes to both ESP and Flipper apps trying to identify the cause of occasionally-corrupted WiFi packets.
My next plan on this front is to ask Copilot if it can help track it down.

Issue: About 1% of the time Wendigo packets received by Flipper representing AP and STA devices will be corrupted. The most readily-observable change is that the MAC is replaced by the first 6 bytes of the PREAMBLE for that packet type - AP Preamble for AP device packets, STA Preamble for STA device packets.
Because it's an occasional problem it has been incredibly difficult to track down.
Analysing packets leaving ESP32 with a logic analyser i haven't observed the issue appearing in the transmitted packet - although it's possible that the packets I watched simply weren't affected by the issue.
The issue can be seen in wendigo_scan_handle_rx_data_cb() - The UART receive callback.
This leads me to believe that it's a result of the approach being taken to remove bytes from the buffer as they're used, and probably a concurrency issue.

This repo has a number of changes unrelated to this investigation that will be very useful to keep, but the quick & dirty approach of tackling the changes makes me want to throw all changes away and re-implement.
Changes to carry over:
* Use of FURI_LOG_(I|W|E|T|D)
* log_packet() function to dump an entire packet/buffer
* Extract packet from buffer within callback and provide the packet to parseBuffer(), rather than having those functions use buffer directly
* Inconsistent use of int32/uint32 for time calculations

