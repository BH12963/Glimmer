# Makefile

#Glimmer 1.0 - a UCI chess engine written in C17
#Copyright (c) 2026 Bui Hoang Bach

#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:

#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.

#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

SRC  ?= Glimmer1.0.c
OUT  ?= Glimmer1.0

CFLAGS  = -O2 -mpopcnt \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -fno-stack-protector -fno-ident \
          -fomit-frame-pointer -fno-pie -no-pie \
          -ffunction-sections -fdata-sections

LDFLAGS = -Wl,--gc-sections -Wl,--build-id=none \
          -Wl,-z,norelro -Wl,-z,noseparate-code \
          -Wl,--hash-style=gnu -s -lm

.PHONY: all clean size

all: $(OUT)

$(OUT): $(SRC)
	gcc $(CFLAGS) -o $@ $< $(LDFLAGS)
	strip -R .comment -R .note.gnu.property -R .note.ABI-tag $@ 2>/dev/null || true
	@ls -l $@ | awk '{print $$5" bytes  "$$9}'

size: $(OUT)
	@ls -l $(OUT) | awk '{print $$5" bytes  "$$9}'

clean:
	rm -f $(OUT)
