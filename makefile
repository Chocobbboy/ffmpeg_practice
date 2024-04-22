make:
	gcc UseFFMPEG.c -lavcodec -lavutil

clean:
	rm -rf a.out
