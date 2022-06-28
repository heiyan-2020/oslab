comp: old_main
	@./old_main $(mainargs)
mid: mid_main
	@./mid_main $(mainargs)
double: double_main
	@./double_main $(mainargs)
fast: fast_main
	@./fast_main $(mainargs)
chunk: chunk_main
	@./chunk_main $(mainargs)
chunk_main: main.c pmm.c chunk.c
	@gcc -g main.c pmm.c chunk.c -lpthread -o chunk_main
fast_main: main.c pmm.c listImpl.c
	@gcc -g main.c pmm.c double-fast.c -lpthread -o fast_main
double_main: main.c pmm.c listImpl.c
	@gcc -g main.c pmm.c double.c -lpthread -o double_main
mid_main: main.c pmm.c listImpl.c
	@gcc -g main.c pmm.c inter.c -lpthread -o mid_main
old_main: main.c pmm.c listImpl.c
	@gcc -g main.c pmm.c oldImpl.c -lpthread -o old_main
gdb: main
	@gdb main
	@rm main
run: main
	@./main $(mainargs)
	@rm main
main: main.c pmm.c listImpl.c
	@gcc -g main.c pmm.c listImpl.c -lpthread -o main
