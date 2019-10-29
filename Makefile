all: threadtest

tsan: CFLAGS = -fsanitize=thread
tsan: threadtest

clean:
	rm -f threadtest

.PHONY: all tsan dist clean
