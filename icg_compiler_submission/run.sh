#!/bin/bash
# ============================================================
# run.sh - test harness for the RISC-V compiler
#
# Usage:   ./run.sh [compiler] [test-directory] [output-directory]
# Default: ./run.sh ./rvcc test output
#
# Test files are classified by their file name:
#   t*.cpp / t*.c   valid programs: the compiler must succeed and write an
#                   assembly file (<name>.s) and an IR file (<name>.ir).
#                   If the RARS simulator is available the assembly is run
#                   and its output is compared with test/<name>.out
#                   (optional test/<name>.in is fed to stdin and
#                   test/<name>.args holds the command-line arguments).
#   e*.cpp / e*.c   programs with deliberate errors: the compiler must
#                   reject them and must NOT leave an assembly file.
#
# Simulator: RARS 1.6 (https://github.com/TheThirdOne/rars), run as
#   java -jar rars.jar rv64 nc me <file>.s
# Point RARS_JAR at the jar if it is not ./rars.jar or ~/rars.jar.
#
# Directives inside a source file (as comments):
#   // EXPECT-EXIT: N          expected compiler exit status
#   // EXPECT-IR: text         'text' must appear in the IR
#   // EXPECT-COUNT: text N    'text' must appear exactly N times in the IR
#   // EXPECT-NOT-IR: text     'text' must NOT appear in the IR
#   // EXPECT-ERR: text        'text' must appear in the diagnostics
#   // EXPECT-NO-ERR: text     'text' must NOT appear in the diagnostics
#   // EXPECT-NOIR              no output file may be produced
# ============================================================

EXE="${1:-./rvcc}"
TESTDIR="${2:-test}"
OUTDIR="${3:-output}"
WORK="$(mktemp -d)"

if [ ! -x "$EXE" ]; then
    echo "Executable '$EXE' not found - run 'make' first."
    exit 2
fi
mkdir -p "$OUTDIR"

JAVA_CMD=""
if command -v java >/dev/null 2>&1 && java -version >/dev/null 2>&1; then
    JAVA_CMD="java"
else
    for j in /opt/homebrew/opt/openjdk/bin/java /usr/local/opt/openjdk/bin/java "$JAVA_HOME/bin/java" /Library/Java/Home/bin/java; do
        if [ -n "$j" ] && [ -x "$j" ] && "$j" -version >/dev/null 2>&1; then
            JAVA_CMD="$j"
            break
        fi
    done
fi

RARS=""
for c in "$RARS_JAR" ./rars.jar "$HOME/rars.jar"; do
    if [ -n "$c" ] && [ -f "$c" ]; then RARS="$c"; break; fi
done
if [ -n "$RARS" ] && [ -z "$JAVA_CMD" ]; then RARS=""; fi
RARS_ABS=""
if [ -n "$RARS" ]; then
    RARS_ABS="$(cd "$(dirname "$RARS")" && pwd)/$(basename "$RARS")"
else
    echo "Note: Working Java runtime or RARS simulator not found; programs are compiled but not executed."
fi

pass=0
fail=0
total=0

count_in() { # count_in <file> <text>  -> number of exact occurrences
    grep -oF -- "$2" "$1" 2>/dev/null | wc -l | tr -d ' '
}

fails=""

run_test() {
    local src="$1" name base out outlog errlog
    name=$(basename "$src")
    base="${name%.*}"
    out="$OUTDIR/$base.ir"
    asm="$OUTDIR/$base.s"
    outlog="$WORK/$base.out"
    errlog="$WORK/$base.err"
    rm -f "$out" "$asm"
    total=$((total + 1))

    # ----- collect directives from the source -----
    local expect_exit="" expect_noir="" irs="" errs="" noirs="" noerrs="" counts=""
    local line
    while IFS= read -r line; do
        case "$line" in
            *EXPECT-EXIT:*)   expect_exit="${line##*EXPECT-EXIT: }";;
            *EXPECT-NOIR*)    expect_noir=1;;
            *EXPECT-IR:*)     irs="$irs
${line#*EXPECT-IR: }";;
            *EXPECT-COUNT:*)  counts="$counts
${line#*EXPECT-COUNT: }";;
            *EXPECT-NOT-IR:*) noirs="$noirs
${line#*EXPECT-NOT-IR: }";;
            *EXPECT-ERR:*)    errs="$errs
${line#*EXPECT-ERR: }";;
            *EXPECT-NO-ERR:*) noerrs="$noerrs
${line#*EXPECT-NO-ERR: }";;
        esac
    done < "$src"

    echo "------------------------------------------------------------"
    echo "TEST: $name"
    "$EXE" -ir "$src" "$OUTDIR/$base.s" >"$outlog" 2>"$errlog"
    local rc=$?

    # ----- exit status -----
    if [ -n "$expect_exit" ]; then
        if [ "$rc" -ne "$expect_exit" ]; then
            note_fail "exit status $rc, expected $expect_exit"
        fi
    elif [[ "$name" == e* ]]; then
        if [ "$rc" -ne 1 ]; then
            note_fail "exit status $rc, expected 1 (program must be rejected)"
        fi
    else
        if [ "$rc" -ne 0 ]; then
            note_fail "exit status $rc, expected 0 (program must be accepted)"
        fi
    fi

    # ----- output files -----
    if [[ "$name" == e* && -z "$expect_exit" ]] || [ -n "$expect_noir" ]; then
        if [ -f "$asm" ]; then
            note_fail "assembly file '$asm' was produced but must not be"
        fi
    elif [ -z "$expect_noir" ]; then
        if [ ! -f "$asm" ] || [ ! -s "$asm" ]; then
            note_fail "no assembly file produced"
        fi
        if [ ! -f "$out" ] || [ ! -s "$out" ]; then
            note_fail "no IR file produced"
        fi
    fi

    # ----- execution on the simulator -----
    if [ -n "$RARS_ABS" ] && [[ "$name" == t* ]] && [ -s "$asm" ] && [ -f "$TESTDIR/$base.out" ]; then
        local args="" input=/dev/null asm_abs
        [ -f "$TESTDIR/$base.args" ] && args=$(cat "$TESTDIR/$base.args")
        [ -f "$TESTDIR/$base.in" ] && input="$TESTDIR/$base.in"
        asm_abs="$(cd "$(dirname "$asm")" && pwd)/$(basename "$asm")"
        if [ -n "$args" ]; then
            (cd "$WORK" && "$JAVA_CMD" -jar "$RARS_ABS" rv64 nc me "$asm_abs" pa $args) < "$input" > "$OUTDIR/$base.run" 2>"$OUTDIR/$base.rars.err"
        else
            (cd "$WORK" && "$JAVA_CMD" -jar "$RARS_ABS" rv64 nc me "$asm_abs") < "$input" > "$OUTDIR/$base.run" 2>"$OUTDIR/$base.rars.err"
        fi
        if ! diff -q "$TESTDIR/$base.out" "$OUTDIR/$base.run" >/dev/null 2>&1; then
            note_fail "simulator output differs from $TESTDIR/$base.out (see $OUTDIR/$base.run)"
        fi
    fi

    # ----- IR content checks (only if the IR exists) -----
    if [ -f "$out" ] && { [ -n "$irs" ] || [ -n "$noirs" ] || [ -n "$counts" ]; }; then
        local t
        while IFS= read -r t; do
            [ -z "$t" ] && continue
            grep -qF -- "$t" "$out" || note_fail "IR does not contain: '$t'"
        done <<< "$irs"
        while IFS= read -r t; do
            [ -z "$t" ] && continue
            grep -qF -- "$t" "$out" && note_fail "IR unexpectedly contains: '$t'"
        done <<< "$noirs"
        while IFS= read -r t; do
            [ -z "$t" ] && continue
            local text n got
            text="${t% [0-9]*}"
            n="${t##* }"
            got=$(count_in "$out" "$text")
            if [ "$got" -ne "$n" ]; then
                note_fail "IR contains '$text' $got time(s), expected $n"
            fi
        done <<< "$counts"
    fi

    # ----- diagnostic checks -----
    local t
    while IFS= read -r t; do
        [ -z "$t" ] && continue
        grep -qF -- "$t" "$outlog" "$errlog" || note_fail "diagnostics do not contain: '$t'"
    done <<< "$errs"
    while IFS= read -r t; do
        [ -z "$t" ] && continue
        grep -qF -- "$t" "$outlog" "$errlog" && note_fail "diagnostics unexpectedly contain: '$t'"
    done <<< "$noerrs"

    if [ -n "$fails" ]; then
        echo ">> FAIL"
        printf '%s\n' "$fails" | sed 's/^/     /'
        fail=$((fail + 1))
    else
        echo ">> PASS"
        pass=$((pass + 1))
    fi
}

note_fail() {
    fails="${fails}${1}
"
}

shopt -s nullglob
files=( "$TESTDIR"/*.cpp "$TESTDIR"/*.c )
sorted=()
while IFS= read -r f; do sorted+=("$f"); done < <(printf '%s\n' "${files[@]}" | sort)

for src in "${sorted[@]}"; do
    [ -e "$src" ] || continue
    fails=""
    run_test "$src"
done

rm -rf "$WORK"
echo "============================================================"
echo "Passed: $pass   Failed: $fail   Total: $total"
[ "$fail" -eq 0 ]
