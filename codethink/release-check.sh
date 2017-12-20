#!/bin/bash
#
# Release checking script, to run the useful release checks on a branch
#
# Copyright 2016 Codethink Ltd.
#	Ben Dooks <ben.dooks@codethink.co.uk>

# usage:
#	-s,--start <sha>	start revision
#	-e,--end <sha>		end revision
#	-b,--branch <rev>	check branch <rev>
#	-f,--from <rev|sha>	branched from <rev|sha>
#	-v,--vebose		set verbose mode

verbose=0
stat=1

while [ "$#" -ge 1 ]
do
    key=$1
    shift
    case $key in
	-s|--start)
	    start=$1
	    shift
	;;
	-e|--end)
	    end=$1
	    shift
	    ;;
	-b|--branch)
	    start=`git merge-base HEAD $1`
	    end=$1
	    shift
	    ;;
	-f|--from)
	    start=`git merge-base $1 HEAD`
	    end=`git show --format=%H HEAD | head -1`
	    shift
	    ;;
	*)
	    echo "Unknown argument $key"
	    exit 2
	    ;;
    esac
done

if [ ! -n "$start" ]; then
    if [ ! -n "$1" ]; then
	echo "Need start revision"
	exit 1
    fi

    if [ ! -n "$2" ]; then
	echo "Need end revision"
	exit 1
    fi

    start=$1
    end=$2
fi

if [ ! -n "$end" ]; then
    echo "Assuming testing to current HEAD"
    end=HEAD
fi

echo "Change stats for $start to $end"
git diff $start..$end | diffstat -p1
echo

echo "Testing '$start' to '$end'"
echo

files=`git diff $start..$end | diffstat -p1 -l | awk '{ print $start }'`
for file in $files; do
    if [ $verbose -eq 1 ] || [ $stat -eq 1 ]; then
	echo "Checking file $file"
    fi
done
if [ $verbose -eq 1 ]; then
    echo
fi

objs=""
cobjs=""
dobjs=""
for file in $files; do
    cobj=""
    sobj=""
    dobj=""
    case $file in
	*.c)
	    sobj=${file/.c/.o}
	    cobj=${file/.c/.o}
	    ;;
	*.h)
	    sobj=""
	    ;;
	*.dts)
	    sobj=${file/.dts/.dtb}
	    dobj=${file/.dts/.dtb}
	    ;;
	*.dtsi)
	    sobj=""
	    ;;
	*.sh)
	    sobj=""
	    ;;	
	*.txt)
	    sobj=$file
	    ;;
	*Kconfig)
	    sobj=""
	    ;;
	*Kconfig.debug)
	    sobj=""
	    ;;
	*_defconfig)
	    sobj=""
	    ;;
	*Makefile)
	    sobj=""
	    ;;
	*)
	    echo "unknown file type for $file"
	    exit 1
	    ;;
    esac
    objs+=$sobj
    objs+=" "
    cobjs+=$cobj
    cobjs+=" "
    dobjs+=$dobj
    dobjs+=" "
done

echo "Checking .o files for warnings:"
for obj in $cobjs; do
    cobj=${obj/.o/.c}
    echo "Check $cobj"
    rm $obj 2>/dev/null
    make $obj 2>&1 | awk -v cobj=$cobj -F':' '{ if ($start == cobj) { print $0;} }'
    echo
done

# we assume we're going to be builiding ARM
if [ ! -n "$ARCH" ]; then
    echo "No \$ARCH set, setting ARCH=arm"
    export ARCH=arm
fi

if [ ! -n "$CROSS_COMPILE" ]; then
    echo "No \$CROSS_COMPILE set, setting to default arm-gcc"
    export CROSS_COMPILE=arm-linux-gnueabihf5-
fi

if [ -n "$dobjs" ]; then
    echo "Checking .dtb files build"
    echo
    for dobj in $dobjs; do
	if [ -e $dobj ]; then
	    rm $dobj
	fi
	make $(basename $dobj)
    done

    echo
fi

echo "Static analysis with checkpatch"
echo
for obj in $cobjs; do     
    cobj=${obj/.o/.c}
    if [[ $cobj == "kernel/printk/printk.c" ||
	 $cobj == "arch/arm/kernel/traps.c" ||
	 $cobj == "init/main.c" ]]; then
	echo "Ignoring $cobj"
    else
	echo "Check $cobj"
	make $obj 2>&1 > /dev/null
	make C=2 $obj 2>&1 | awk -v cobj=$cobj 'BEGIN { m = 0; } { if ($start == "CHECK" && $end == cobj) { m = 1; } if (m == 1) { print $0;} }'
    fi
    echo
done

echo "Running diffs through checkpatch"
git diff $start..$end | ./scripts/checkpatch.pl -
echo "Done"


