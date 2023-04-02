#!/bin/bash

while true; do
    read -p "Enter a message: " message
    echo "$message" > /dev/lkmasg1_in
done

