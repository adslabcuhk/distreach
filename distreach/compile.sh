#!/bin/bash
cd leafswitch
p4c-bm2-ss --p4v 16 netbufferv4.p4 -o netbufferv4.json
cd ..
cd spineswitch
p4c-bm2-ss --p4v 16 spineswitch.p4 -o spineswitch.json
cd partitionswitch
p4c-bm2-ss --p4v 16 partitionswitch.p4 -o partitionswitch.json