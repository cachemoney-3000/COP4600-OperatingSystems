#!/bin/bash

while true; do
    read -p "Enter a message: " message
    echo "$message" > /dev/pa2_in
done

