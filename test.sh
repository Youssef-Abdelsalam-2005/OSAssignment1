#!/bin/bash

#globals
ret=0
server="server"
client="client"
serverOut=testServerOutput.txt
clientOut=testClientOutput.txt
successFile=testSuccess.txt
IPADDRESS=localhost
PORT=2200

# --- helper function ---
function run(){
	echo -e "$1:"
	$1 #execute
	#check errors
	tmp=$?
	if [ $tmp -ne 0 ]; then
		ret=$tmp
	fi
	echo "" #newline
	return $tmp
}

function checkConnection() {
    sleep 0.1
    case `netstat -a -n -p 2>/dev/null| grep $PORT `  in
	*":2200"*"LISTEN"*"server"*)
	    return 1;;
    esac
    sleep 0.2
    case `netstat -a -n -p 2>/dev/null| grep $PORT`  in
	*":2200"*"LISTEN"*"server"*)
	    return 1;;
    esac
    sleep 0.2
    case `netstat -a -n -p 2>/dev/null| grep $PORT`  in
	*":2200"*"LISTEN"*"server"*)
	    return 1;;
    esac
    sleep 0.2
    case `netstat -a -n -p 2>/dev/null| grep $PORT`  in
	*":2200"*"LISTEN"*"server"*)
	    return 1;;
    esac
    sleep 0.2
    case `netstat -a -n -p 2>/dev/null| grep $PORT`  in
	*":2200"*"LISTEN"*"server"*)
	    return 1;;
    esac
    return 0
}

function test_concurrent_connections() {
    local num_connections=$1
    local success_count=0
    
    for ((i=1; i<=num_connections; i++)); do
        ./$client $IPADDRESS $PORT "A 192.168.1.$i 80" > /dev/null 2>&1 &
    done
    wait
    
    # Check if all rules were added
    result=$(./$client $IPADDRESS $PORT "L")
    success_count=$(echo "$result" | grep -c "Rule:")
    
    if [ $success_count -eq $num_connections ]; then
        return 0
    else
        return 1
    fi
}

# --- TESTCASES ---
function interactive_testcase() {
    t="interactive test case"
    # cleanup
    rm -f $serverOut
    rm -f $successFile
    echo "Rule added" > $successFile

    if  [ ! -x ./$server ] ; then
	echo -e "ERROR: Server program not found"
	return -1
    fi
	
    killall $server > /dev/null 2> /dev/null
    # start server
    echo -en "starting server: \t"
    echo "A 147.188.193.15 22" | ./$server -i > $serverOut  2>&1 &
    if [ $? -ne 0 ]
    then
	echo -e "ERROR: could not start interactive server"
	return -1
    else
	echo "OK"
    fi
    killall $server > /dev/null 2> /dev/null
    echo -en "server result:     \t"
    res=`diff $serverOut $successFile 2>&1`
    if [ " $res" != " " ]
    then
	echo "Error: Server returned invalid result"
	return -1
    else
	echo "OK"
    fi
}    

    
function basic_testcase(){
    t="testcase 1"
    #cleanup
    rm -f $serverOut
    rm -f $clientOut
    rm -f $successFile
    echo "Rule added" > $successFile
    killall $server > /dev/null 2> /dev/null

    # start server
    echo -en "starting server: \t"
    ./$server $PORT > $serverOut  2>&1 &
    checkConnection
    if [ $? -ne 1 ]
    then
	echo -e "ERROR: could not start server"
	return -1
    else
	echo "OK"
    fi

    # start client
    command="A 147.188.192.41 443"
    echo -en "executing client: \t"
    ./$client $IPADDRESS $PORT $command > $clientOut 2>/dev/null
    if [ $? -ne 0 ]
    then
	echo -e "Error: Could not execute client"
	killall $server > /dev/null 2> /dev/null
	return -1
    else
	echo "OK"
    fi
    killall $server > /dev/null 2> /dev/null
    if [ ! -r $clientOut ]
    then
	echo "Error: Client produced no output"
	return -1
    fi
    
    echo -en "server result:     \t"
    res=`diff $clientOut $successFile 2>&1`
    if [ " $res" != " " ]
    then
	echo "Error: Server returned invalid result"
	return -1
    else
	echo "OK"
    fi
    return 0
}

function test_invalid_inputs() {
    t="invalid input tests"
    
    # Test invalid IP address format
    echo -en "testing invalid IP: \t"
    result=$(./$client $IPADDRESS $PORT "A 256.256.256.256 80")
    if [[ "$result" == "Invalid rule" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    # Test invalid port number
    echo -en "testing invalid port: \t"
    result=$(./$client $IPADDRESS $PORT "A 192.168.1.1 65536")
    if [[ "$result" == "Invalid rule" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    # Test invalid command
    echo -en "testing invalid command: \t"
    result=$(./$client $IPADDRESS $PORT "X")
    if [[ "$result" == "Illegal request" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    return 0
}

function test_rule_operations() {
    t="rule operations test"
    
    # Add a rule
    echo -en "adding rule: \t"
    ./$client $IPADDRESS $PORT "A 192.168.1.1 80" > /dev/null
    
    # Test duplicate rule
    echo -en "testing duplicate rule: \t"
    result=$(./$client $IPADDRESS $PORT "A 192.168.1.1 80")
    if [[ "$result" == "Rule added" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    # Delete existing rule
    echo -en "deleting rule: \t"
    result=$(./$client $IPADDRESS $PORT "D 192.168.1.1 80")
    if [[ "$result" == "Rule deleted" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    # Delete non-existent rule
    echo -en "deleting non-existent rule: \t"
    result=$(./$client $IPADDRESS $PORT "D 192.168.1.2 80")
    if [[ "$result" == "Rule not found" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    return 0
}

function test_connection_checking() {
    t="connection checking test"
    
    # Add a rule
    ./$client $IPADDRESS $PORT "A 192.168.1.0-192.168.1.255 80-90" > /dev/null
    
    # Test valid connection
    echo -en "testing valid connection: \t"
    result=$(./$client $IPADDRESS $PORT "C 192.168.1.100 85")
    if [[ "$result" == "Connection accepted" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    # Test invalid connection
    echo -en "testing invalid connection: \t"
    result=$(./$client $IPADDRESS $PORT "C 192.168.2.1 85")
    if [[ "$result" == "Connection rejected" ]]; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    return 0
}

function test_performance() {
    t="performance test"
    
    # Test concurrent connections
    echo -en "testing concurrent connections (100): \t"
    if test_concurrent_connections 100; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    # Test response time
    echo -en "testing response time: \t"
    start_time=$(date +%s.%N)
    ./$client $IPADDRESS $PORT "L" > /dev/null
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc)
    
    if (( $(echo "$duration < 1.0" | bc -l) )); then
        echo "OK"
    else
        echo "FAILED (${duration}s)"
        return -1
    fi
    
    return 0
}

function test_memory_leaks() {
    t="memory leak test"
    
    echo -en "testing for memory leaks: \t"
    valgrind --leak-check=full ./$server $PORT &> valgrind_output.txt &
    server_pid=$!
    sleep 1
    
    # Perform some operations
    ./$client $IPADDRESS $PORT "A 192.168.1.1 80" > /dev/null
    ./$client $IPADDRESS $PORT "C 192.168.1.1 80" > /dev/null
    ./$client $IPADDRESS $PORT "D 192.168.1.1 80" > /dev/null
    
    kill $server_pid
    wait $server_pid 2>/dev/null
    
    if grep -q "no leaks are possible" valgrind_output.txt; then
        echo "OK"
    else
        echo "FAILED"
        return -1
    fi
    
    return 0
}

# --- execution ---
run interactive_testcase
run basic_testcase
run test_invalid_inputs
run test_rule_operations
run test_connection_checking
run test_performance
run test_memory_leaks

#cleanup
rm -f valgrind_output.txt
killall $server > /dev/null 2> /dev/null

if [ $ret != 0 ]
then
    echo "Tests failed"
else
    echo "All tests succeeded"
fi
exit $ret
