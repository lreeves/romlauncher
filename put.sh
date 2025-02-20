#!/bin/bash

lftp -e "put romlauncher.nro -o /switch/romlauncher.nro; bye" ftp://192.168.1.28 -p5000
