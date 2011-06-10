/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include "util.h"

#define LEN(x) (sizeof (x) / sizeof *(x))

struct {
	const char *name;
	int sig;
} sigs[] = {
#define SIG(n) { #n, SIG##n }
	SIG(ABRT), SIG(ALRM), SIG(BUS),  SIG(CHLD), SIG(CONT), SIG(FPE),  SIG(HUP),
	SIG(ILL),  SIG(INT),  SIG(KILL), SIG(PIPE), SIG(QUIT), SIG(SEGV), SIG(STOP),
	SIG(TERM), SIG(TSTP), SIG(TTIN), SIG(TTOU), SIG(USR1), SIG(USR2), SIG(URG),
#undef SIG
};

int
main(int argc, char *argv[])
{
	char c, *end;
	int i, sig = SIGTERM;
	pid_t pid;

	while((c = getopt(argc, argv, "s:")) != -1)
		switch(c) {
		case 's':
			for(i = 0; i < LEN(sigs); i++)
				if(!strcasecmp(optarg, sigs[i].name)) {
					sig = sigs[i].sig;
					break;
				}
			if(i == LEN(sigs))
				eprintf("%s: unknown signal\n", optarg);
			break;
		default:
			exit(EXIT_FAILURE);
		}
	for(; optind < argc; optind++) {
		pid = strtol(argv[optind], &end, 0);
		if(*end != '\0')
			eprintf("%s: not a number\n", argv[optind]);
		if(kill(pid, sig) == -1)
			eprintf("kill %d:", pid);
	}
	return EXIT_SUCCESS;
}
