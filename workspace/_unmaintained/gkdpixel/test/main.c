#include <stdio.h>

enum {
	FE_OPT_SCALING,
	FE_OPT_COUNT,
};

typedef struct Option {
	char* key;
	char* name;
} Option;
typedef struct OptionList {
	int count;
	Option* options;
} OptionList;

static struct Config {
	OptionList frontend;
} config = {
	.frontend = { // (OptionList)
		.count = FE_OPT_COUNT,
		.options = (Option[]){
			[FE_OPT_SCALING] = {
				.key	= "minarch_screen_scaling",
			},
			[FE_OPT_COUNT] = {NULL}
		}
	}
};

// (OptionList){
// 		.count = FE_OPT_COUNT,
// 		.options =(Option[]){
			// [FE_OPT_SCALING] = {
			// 	.key	= "minarch_screen_scaling",
			// },
			// [FE_OPT_COUNT] = {NULL}
// 		}
// 	}

int main (int argc, char *argv[]) {
	printf("%p\n", config.frontend);
	return 0;
}
