#########################
# Configure build
#########################
CPP_COMPILER=gcc
ASM_COMPILER=yasm

#
#  optimize
#  level 3
#         \
CC_FLAGS=-O3 -s -fomit-frame-pointer -DUNIX #-ffast-math
LD_FLAGS=-lm
#	  /
#      math
#
# NOTE on -ffast-math
#
# First, breaks strict IEEE compliance, e.g. allows re-ordering of
# instructions to a mathematical equivalent, which may not be IEEE
# floating-point equivalent.
#
# Second, disables setting errno after single-instruction math functions,
# avoiding a write to a thread-local variable (can produce 100% speedup on
# certain architectures).
#
# Third, assumes finite math only, meaning no checks for NaN (or 0) are
# made where they would normally be. It is assumed these values will never
# appear.
#
# Fourth, enables reciprocal approximations for division and reciprocal
# square root.
#
# Fifth, disables signed zero (even if the compile target supports it)
# and rounding math, which enables optimizations e.g. constant folding.
#
# Sixth, generates code assuming no hardware interrupts occur in math
# due to signal()/trap(). If these cannot be disabled on the compile
# target and consequently occur, they will not be handled.
#

#########################
# Configure files
#########################

PLM_SOURCES=src/main.c \
	    src/util.c \
	    src/coder.c \
	    src/mixer.c \
	    src/predictor.c \
	    src/context.c \
	    src/model.c \


PLM_OBJECTS=$(PLM_SOURCES:.c=.o)
ASM_SOURCES=asm/paq7asm-x86_64.asm
ASM_OBJECTS=$(ASM_SOURCES:.asm=.o)

#########################
# Configure rules
#########################

all: plm

test: plm
	rm -f csf.txt.plm csf.out
	./plm -c dat/csf.txt -o csf.txt.plm
	./plm -d csf.txt.plm -o csf.out
	diff csf.out dat/csf.txt
	rm csf.out csf.txt.plm

plm: $(PLM_SOURCES) asm
	$(CPP_COMPILER) $(CC_FLAGS) $(PLM_SOURCES) $(ASM_OBJECTS) -o plm

asm: $(ASM_SOURCES)
	$(ASM_COMPILER) $(ASM_SOURCES) -f elf -m amd64

install:
	cp plm /usr/local/bin

clean:
	rm -f $(PLM_OBJECTS) $(ASM_OBJECTS) plm gmon.out
