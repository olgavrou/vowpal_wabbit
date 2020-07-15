#!/bin/bash
# -- vw daemon test
#
NAME='vw-daemon-test'

export PATH="vowpalwabbit:../build/vowpalwabbit:${PATH}"
# The VW under test
VW=`which vw`

MODEL=$NAME.model
TRAINSET=$NAME.train
PREDREF=$NAME.predref
PREDOUT=$NAME.predict
TRAINSET_JSON="$NAME"_json.train
PREDOUT_JSON="$NAME"_json.predict
NETCAT_STATUS=$NAME.netcat-status
PORT=54248
JSON_PORT=54249

while [ $# -gt 0 ]
do
    case "$1" in
        --foreground)
            Foreground="$1"
            ;;
        *)
            echo "$NAME: unknown argument $1"
            exit 1
            ;;
    esac
    shift
done

# -- make sure we can find vw first
if [ -x "$VW" ]; then
    : cool found vw at: $VW
else
    echo "$NAME: can not find 'vw' in $PATH - sorry"
    exit 1
fi

# -- and netcat
NETCAT=`which netcat`
if [ -x "$NETCAT" ]; then
    : cool found netcat at: $NETCAT
else
    NETCAT=`which nc`
    if [ -x "$NETCAT" ]; then
        : "no netcat but found 'nc' at: $NETCAT"
    else
        echo "$NAME: can not find 'netcat' not 'nc' in $PATH - sorry"
        exit 1
    fi
fi

# -- and pkill
PKILL=`which pkill`
if [ -x "$PKILL" ]; then
    : cool found pkill at: $PKILL
else
    echo "$NAME: can not find 'pkill' in $PATH - sorry"
    exit 1
fi

# A command (+pattern) that is unlikely to match anything but our own test
DaemonCmd="$VW -t -i $MODEL --daemon $Foreground --num_children 1 --quiet --port $PORT"
DaemonCmdJson="$VW -t -i $MODEL --daemon $Foreground --num_children 1 --quiet --port $JSON_PORT --json"
# libtool may wrap vw with '.libs/lt-vw' so we need to be flexible
# on the exact process pattern we try to kill.
DaemonPat=`echo $DaemonCmd | sed 's/^[^ ]*vw /.*vw /'`
DaemonPatJson=`echo $DaemonCmdJson | sed 's/^[^ ]*vw /.*vw /'`

stop_daemon() {
    # Make sure we are not running. May ignore 'error' that we're not
    $PKILL -9 -f "$1" 2>&1 | grep -q 'no process found'

    # relinquish CPU by forcing some context switches to be safe
    # (let existing vw daemon procs die)
    if echo "$1" | grep -q -v -- --foreground; then
        wait
    else
        sleep 0.1
    fi
}

start_daemon() {
    # echo starting daemon
    $1 </dev/null >/dev/null &
    PID=$!
    # give it time to be ready
    if echo "$1" | grep -q -v -- --foreground; then
        wait; wait; wait
    else
        sleep 0.1
    fi
    echo "$PID"
}

cleanup() {
    /bin/rm -f $MODEL $TRAINSET $PREDREF $PREDOUT $NETCAT_STATUS $TRAINSET_JSON $PREDOUT_JSON
    stop_daemon $DaemonPat
    stop_daemon $DaemonPatJson
}

# -- main
cleanup

# prepare training set
cat > $TRAINSET <<EOF
0.55 1 '1| a
0.99 1 '2| b c
EOF

# prepare training set json
cat > $TRAINSET_JSON <<EOF
{"_label":{"Label":1, "Weight":0.55}, "_tag":"'1", "a":true}
{"_label":{"Label":1, "Weight":0.99}, "_tag":"'2", "b":true, "c":true}
EOF

# prepare expected predict output
cat > $PREDREF <<EOF
0.553585 1
0.733882 2
EOF

# Train
$VW -b 10 --quiet -d $TRAINSET -f $MODEL

DaemonPid=`start_daemon "$DaemonCmd"`
DaemonPidJson=`start_daemon "$DaemonCmdJson"`

# Test --foreground argument
PidsAreEqual=false
JsonPidsAreEqual=false
for ProcessPid in $(pgrep -f "$DaemonPat" 2>&1)
do
    if [ $DaemonPid -eq $ProcessPid ]; then
        PidsAreEqual=true
    fi
done
for ProcessPidJson in $(pgrep -f "$DaemonPatJson" 2>&1)
do
    if [ $DaemonPidJson -eq $ProcessPidJson ]; then
        JsonPidsAreEqual=true
    fi
done

if [ $Foreground ]; then
    if ! $PidsAreEqual ; then
        echo "$NAME FAILED: --foreground, but vw has run in the background"
        stop_daemon $DaemonPat
        exit 1
    fi
    if ! $JsonPidsAreEqual ; then
        echo "$NAME FAILED: --foreground, but vw has run in the background"
        stop_daemon $DaemonPatJson
        exit 1
    fi
else
    if $PidsAreEqual ; then
        echo "$NAME FAILED: vw has not run in the background"
        stop_daemon $DaemonPat
        exit 1
    fi
    if $JsonPidsAreEqual ; then
        echo "$NAME FAILED: vw has not run in the background"
        stop_daemon $DaemonPatJson
        exit 1
    fi
fi

# Test on train-set
# OpenBSD netcat quits immediately after stdin EOF
# nc.traditional does not, so let's use -q 1. -q is not supported on Mac so let's workaround it with -i
DELAY_OPT="-q 1"
if ! $NETCAT $DELAY_OPT localhost $PORT < /dev/null
then
  DELAY_OPT="-i 1"
fi

if ! $NETCAT $DELAY_OPT localhost $JSON_PORT < /dev/null
then
  DELAY_OPT="-i 1"
fi

$NETCAT $DELAY_OPT localhost $PORT < $TRAINSET > $PREDOUT
$NETCAT $DELAY_OPT localhost $JSON_PORT < $TRAINSET_JSON > $PREDOUT_JSON

#wait

# JohnLangford: I'm unable to make the following work on Ubuntu 16.04.  Without -q 1, netcat appears to sometimes early terminate with STATUS an empty string.
# However, GNU netcat does not know -q, so let's do a work-around
#touch $PREDOUT
#( $NETCAT localhost $PORT < $TRAINSET > $PREDOUT; STATUS=$?; echo $STATUS > $NETCAT_STATUS ) &
# Wait until we recieve a prediction from the vw daemon then kill netcat
#until [ `wc -l < $PREDOUT` -eq 2 ]; do
#    if [ -f $NETCAT_STATUS ]; then
#        STATUS=`cat $NETCAT_STATUS`
#        if [ $STATUS -ne 0 ]; then
#            echo "$NAME: netcat failed with status code $STATUS"
#            stop_daemon
#            exit 1
#        fi
#    fi
#done
$PKILL -9 $NETCAT

# We should ignore small (< $Epsilon) floating-point differences (fuzzy compare)
diff <(cut -c-5 $PREDREF) <(cut -c-5 $PREDOUT)
case $? in
    0)  echo "$NAME: OK"
        ;;
    1)  echo "$NAME FAILED: see $PREDREF vs $PREDOUT"
        stop_daemon $DaemonPat
        stop_daemon $DaemonPatJson
        exit 1
        ;;
    *)  echo "$NAME: diff failed - something is fishy"
        stop_daemon $DaemonPat
        stop_daemon $DaemonPatJson
        exit 2
        ;;
esac

# We should ignore small (< $Epsilon) floating-point differences (fuzzy compare)
diff <(cut -c-5 $PREDREF) <(cut -c-5 $PREDOUT_JSON)
case $? in
    0)  echo "$NAME: JSON OK"
        cleanup
        exit 0
        ;;
    1)  echo "$NAME FAILED: see $PREDREF vs $PREDOUT_JSON"
        stop_daemon $DaemonPat
        stop_daemon $DaemonPatJson
        exit 1
        ;;
    *)  echo "$NAME: diff failed - something is fishy"
        stop_daemon $DaemonPat
        stop_daemon $DaemonPatJson
        exit 2
        ;;
esac