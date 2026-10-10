# Glimmer
A chess engine under 30,000 bytes on Linux
## Build
make
### Design Philosophy

Unlike those minimal engines that pack themselves into self-extracting shell scripts and then call the host’s compiler at runtime, Glimmer is a genuine standalone native binary — pure machine code, nothing more.

Windows is a different story. The PE/COFF format brings so much extra baggage (section alignment padding, runtime initialization overhead…) that any `.exe` would blow past the 30,000-byte limit. That’s why Glimmer stays deliberately Linux-only and ELF-native. No Windows builds are provided or supported.

```bash
$ wc -c Glimmer1.0
24824 Glimmer1.0
```
