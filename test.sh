#!/bin/bash

#globals
ret=0
server="./server"
client="./client"
serverOut=testServerOutput.txt
clientOut=testClientOutput.txt
successFile=testSuccess.txt
IPADDRESS=localhost
PORT=2200

# --- helper functions ---
function run(){
    echo -e "\n--- Running $1 ---"
    $1 #execute
    #check errors
    tmp=$?
    if [ $tmp -ne 0 ]; then
        echo "Test $1 FAILED with exit code $tmp"
        ret=$tmp
    else
        echo "Test $1 PASSED"
    fi
    echo "" #newline
    return $tmp
}

function checkConnection() {
    for i in {1..10}; do
        sleep 0.5
        if netstat -tuln 2>/dev/null | grep -q ":$PORT "; then
            return 0
        fi
    done
    return 1
}

function start_server() {
    echo -en "Starting server: \t"
    $server $PORT > $serverOut 2>&1 &
    server_pid=$!
    if checkConnection; then
        echo "OK (PID: $server_pid)"
        return 0
    else
        echo "ERROR: Could not start server"
        cat $serverOut
        return 1
    fi
}

function stop_server() {
    if [ ! -z "$server_pid" ]; then
        echo "Stopping server (PID: $server_pid)"
        kill $server_pid
        wait $server_pid 2>/dev/null
    fi
}

# --- TESTCASES ---
function interactive_testcase() {
    echo "Running interactive test case"
    rm -f $serverOut $successFile
    echo "Rule added" > $successFile

    echo -en "Starting interactive server: \t"
    echo "A 147.188.193.15 22" | $server -i > $serverOut 2>&1
    
    echo -en "Server result: \t"
    if diff $serverOut $successFile >/dev/null 2>&1; then
        echo "OK"
        return 0
    else
        echo "ERROR: Server returned invalid result"
        echo "Expected:"
        cat $successFile
        echo "Got:"
        cat $serverOut
        return 1
    fi
}

function basic_testcase(){
    echo "Running basic testcase"
    rm -f $clientOut $successFile
    echo "Rule added" > $successFile

    command="A 147.188.192.41 443"
    echo -en "Executing client: \t"
    $client $IPADDRESS $PORT "$command" > $clientOut 2>&1
    
    echo -en "Client result: \t"
    if diff $clientOut $successFile >/dev/null 2>&1; then
        echo "OK"
        return 0
    else
        echo "ERROR: Client returned invalid result"
        echo "Expected:"
        cat $successFile
        echo "Got:"
        cat $clientOut
        return 1
    fi
}

function test_invalid_inputs() {
    echo "Running invalid input tests"
    
    echo -en "Testing invalid IP: \t"
    result=$($client $IPADDRESS $PORT "A 256.256.256.256 80")
    if [[ "$result" == *"Invalid rule"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Testing invalid port: \t"
    result=$($client $IPADDRESS $PORT "A 192.168.1.1 65536")
    if [[ "$result" == *"Invalid rule"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Testing invalid command: \t"
    result=$($client $IPADDRESS $PORT "X")
    if [[ "$result" == *"Illegal request"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    return 0
}

function test_rule_operations() {
    echo "Running rule operations test"
    
    echo -en "Adding rule"
    result=$($client $IPADDRESS $PORT "A 192.168.1.1 80")
    if [[ "$result" == *"Rule added"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Testing duplicate rule: \t"
    result=$($client $IPADDRESS $PORT "A 192.168.1.1 80")
    if [[ "$result" == *"Rule added"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Deleting rule: \t"
    result=$($client $IPADDRESS $PORT "D 192.168.1.1 80")
    if [[ "$result" == *"Rule deleted"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Deleting non-existent rule: \t"
    result=$($client $IPADDRESS $PORT "D 192.168.1.2 80")
    if [[ "$result" == *"Rule not found"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    return 0
}

function test_connection_checking() {
    echo "Running connection checking test"
    
    $client $IPADDRESS $PORT "A 192.168.1.0-192.168.1.255 80-90" > /dev/null
    
    echo -en "Testing valid connection: \t"
    result=$($client $IPADDRESS $PORT "C 192.168.1.100 85")
    if [[ "$result" == *"Connection accepted"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Testing invalid connection: \t"
    result=$($client $IPADDRESS $PORT "C 192.168.2.1 85")
    if [[ "$result" == *"Connection rejected"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    return 0
}

function test_ip_range_rules() {
    echo "Running IP range rules test"
    
    $client $IPADDRESS $PORT "A 192.168.1.0-192.168.1.255 80" > /dev/null
    
    echo -en "Testing connection within range: \t"
    result=$($client $IPADDRESS $PORT "C 192.168.1.50 80")
    if [[ "$result" == *"Connection accepted"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Testing connection outside range: \t"
    result=$($client $IPADDRESS $PORT "C 192.168.2.50 80")
    if [[ "$result" == *"Connection rejected"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    return 0
}

function test_port_range_rules() {
    echo "Running port range rules test"
    
    $client $IPADDRESS $PORT "A 192.168.1.0-192.168.1.255 80-90" > /dev/null
    
    echo -en "Testing connection within port range: \t"
    result=$($client $IPADDRESS $PORT "C 192.168.1.50 85")
    if [[ "$result" == *"Connection accepted"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    echo -en "Testing connection outside port range: \t"
    result=$($client $IPADDRESS $PORT "C 192.168.1.50 91")
    if [[ "$result" == *"Connection rejected"* ]]; then
        echo "OK"
    else
        echo "FAILED (Got: $result)"
        return 1
    fi
    
    return 0
}

function test_concurrent_connections() {
    echo "Running concurrent connections test"
    
    for i in {1..5}; do
        $client $IPADDRESS $PORT "A 192.168.1.$i 80" > /dev/null &
    done
    
    echo -en "Testing multiple connections: \t"
    for i in {1..5}; do
        result=$($client $IPADDRESS $PORT "C 192.168.1.$i 80")
        if [[ "$result" == *"Connection accepted"* ]]; then
            echo "Connection for 192.168.1.$i accepted"
        else
            echo "FAILED for 192.168.1.$i (Got: $result)"
            return 1
        fi
    done
    
    return 0
}

# --- execution ---
start_server || exit 1

run interactive_testcase
run basic_testcase
run test_invalid_inputs
run test_rule_operations
run test_connection_checking
run test_ip_range_rules
run test_port_range_rules
run test_concurrent_connections

stop_server

if [ $ret != 0 ]; then
    echo "Some tests failed"
else
    echo "All tests succeeded"
fi

exit $ret