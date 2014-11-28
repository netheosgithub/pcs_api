#!/bin/sh
find libpcs_api/ -name "*.cc" -o -name "*.h" | xargs cpplint.py --root=cpp/libpcs_api

