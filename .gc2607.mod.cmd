savedcmd_gc2607.mod := printf '%s\n'   gc2607.o | awk '!x[$$0]++ { print("./"$$0) }' > gc2607.mod
