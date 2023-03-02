#!/bin/sh

echo 191 > /sys/class/gpio/export && sleep 1

echo out > /sys/class/gpio/gpio191/direction

echo 1 > /sys/class/gpio/gpio191/value && sleep 1

echo 0 > /sys/class/gpio/gpio191/value && sleep 1

echo 1 > /sys/class/gpio/gpio191/value

