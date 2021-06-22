#!/bin/bash
git ls-files -o --exclude build | grep config.h\$ | xargs rm
