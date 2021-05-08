gcc -nostdlib -nostartfiles -ffreestanding -mconsole -fno-stack-check -fno-stack-protector -mno-stack-arg-probe --entry=_wWinMain@16 -Os main.c -luser32 -lkernel32 -lshlwapi -lshell32

strip -s a.exe