/* ============================================================
   cmdline.h - Header file with shared declarations
   ============================================================ */

#ifndef CMDLINE_H
#define CMDLINE_H

/* Command line parsing functions */
void print_usage(const char* prog);
void print_version(void);
int parse_options(int argc, char** argv, config_t* cfg);


#endif /* CMDLINE_H */
