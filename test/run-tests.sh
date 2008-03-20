#!/bin/bash

# initialize the environment for the tests
export LD_LIBRARY_PATH="`pwd`/../trace"
SAMPLE=sample.out
test=tracetest
lib=nsntrace.so

# run the tests

#########################
# PHP low-level tests
#

echo "Running low-level tracing PHP tests ($lib)..."
OUT=php.out
CHK=php.chk
php -n -d 'extension_dir=../bindings/php/modules' \
       -d "extension=$lib" ./$test.php | ./sorttags > $OUT

echo -n "Verifying PHP test results..."
cat $OUT | sed 's/^.* tracetest: /__TIME__ tracetest: /g' | \
                sed 's/[^ ]*@.*:[0-9]*$/__LOCATION__/g' > $CHK
if cmp $CHK $SAMPLE; then
    echo "OK"
    result=0
else
    echo "FAILED"
    diff $SAMPLE $CHK
    result=1
fi

[ "$result" = "0" ] || exit $result
rm -f $CHK $OUT


#########################
# Python low-level tests
#
echo "Running low-level logging Python tests (_$lib)..."

pylib="`find .. -name _$lib`"
if [ -z "$pylib" ]; then
    echo "Could not find the Python runtime log extension. (_$lib)"
    exit 1
fi
export PYTHONPATH="${pylib%/*}"

OUT=python.out
CHK=puthon.chk
./$test.py | ./sorttags > $OUT

echo -n "Verifying Python test results..."
cat $OUT | sed 's/^.* tracetest: /__TIME__ tracetest: /g' | \
                sed 's/[^ ]*@.*:[0-9]*$/__LOCATION__/g' > $CHK
if cmp $CHK $SAMPLE; then
    echo "OK"
    [ "$result" = "0" ] && result=0
else
    echo "FAILED"
    diff $SAMPLE $CHK
    result=1
fi

[ "$result" = "0" ] || exit $result
rm -f $CHK $OUT


#########################
# C library tests
#
echo "Running low-level C library tests (libtrace.so)..."
OUT=c.out
CHK=c.chk
./$test | ./sorttags > $OUT

echo -n "Verifying C library test results..."
cat $OUT | sed 's/^.* tracetest: /__TIME__ tracetest: /g' | \
           sed 's/[^ ]*@.*:[0-9]*$/__LOCATION__/g' > $CHK
if cmp $CHK $SAMPLE; then
    echo "OK"
    [ "$result" = "0" ] && result=0
else
    echo "FAILED"
    diff $SAMPLE $CHK
    result=1
fi

[ "$result" = "0" ] || exit $result
rm -f $CHK $OUT

exit $result
