#!/usr/bin/env bash

set -euo pipefail

function run_test(){
  local message="$1"
  local suite=$2

  # Only keep lines that are the executed tests
  # Remove timing information (to keep the log reproducible)
  echo "$message with pattern $suite"
  set +e
  # Do not quote suite, otherwise, it executes "" for the empty string
  ./test_setup $suite | grep " OK " | cut -d "[" -f1
  set -e
}

echo "# Happy Path Tests"

run_test "Executing all" ""

run_test "Executing at first level" "/perf"

run_test "Executing at second level" "/perf/symmetric"

run_test "Executing at third level" "/perf/symmetric/sha2"

run_test "Executing at leaf level" "/perf/symmetric/sha2/rand"

echo "# Not Happy Path Tests"

run_test "Executing something that doesn't exist" "/xx"

run_test "Executing something that doesn't exist (2)" "/xx/rand"

echo "Script finished successfully"