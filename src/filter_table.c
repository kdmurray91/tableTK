/*
 * ============================================================================
 *
 *       Filename:  filter_table.c
 *
 *    Description:  filterTable: Filters tabular data quickly
 *
 *        Version:  1.0
 *        Created:  14/03/14 14:11:29
 *       Revision:  none
 *        License:  GPLv3+
 *       Compiler:  gcc 4.7+ or clang 3.2+
 *
 *         Author:  Kevin Murray, spam@kdmurray.id.au
 *
 * ============================================================================
 */
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "kdm.h"
#include "ktable.h"

typedef struct _ft {
    cell_t threshold;
} ft_t;

static inline void
ft_median (table_t *tab, char *line, cell_t *cells, size_t count)
{
    cell_t med = median(cells, count, tab->mode);
    switch(tab->mode) {
        case U64:
            if (med.u >= ((ft_t *)tab->data)->threshold.u) fprintf(tab->outfp, "%s", line);
            break;
        case I64:
            if (med.i >= ((ft_t *)tab->data)->threshold.i) fprintf(tab->outfp, "%s", line);
            break;
        case D64:
            if (med.d >= ((ft_t *)tab->data)->threshold.d) fprintf(tab->outfp, "%s", line);
            break;
    }
}

static inline void
ft_num_nonzero (table_t *tab, char *line, cell_t *cells, size_t count)
{
    size_t iii = 0;
    size_t passes = 0;
    switch(tab->mode) {
        case U64:
            while ((iii < count) && (passes < ((ft_t *)tab->data)->threshold.u)) {
                if (cells[iii++].u > 0ull) passes++;
            }
            if (passes >= ((ft_t *)tab->data)->threshold.u) fprintf(tab->outfp, "%s", line);
            break;
        case I64:
            while ((iii < count) && (passes < ((ft_t *)tab->data)->threshold.i)) {
                if (cells[iii++].i > 0ll) passes++;
            }
            if (passes >= ((ft_t *)tab->data)->threshold.i) fprintf(tab->outfp, "%s", line);
            break;
        case D64:
            while ((iii < count) && (passes < ((ft_t *)tab->data)->threshold.d)) {
                if (cells[iii++].d > 0.0L) passes++;
            }
            if (passes >= ((ft_t *)tab->data)->threshold.d) fprintf(tab->outfp, "%s", line);
            break;
    }
}

int
filter_table(table_t *tab)
{
    iter_table(tab);
    return 1;
}

void
print_usage()
{
    fprintf(stderr, "filterTable\n\n");
    fprintf(stderr, "Filter a large table row-wise.\n\n");
    fprintf(stderr, "USAGE:\n\n");
    fprintf(stderr, "filterTable [-r ROWS -c COLS -i INFILE -o OUTFILE -s SEP] -m | -z THRESH\n");
    fprintf(stderr, "filterTable -h\n\n");
    fprintf(stderr, "OPTIONS:\n");
    fprintf(stderr, "\t-m THRESH\tUse median method of filtering, with threshold THRESH.\n");
    fprintf(stderr, "\t-z THRESH\tUse number of non-zero cells to filter, with threshold THRESH.\n");
    fprintf(stderr, "\t-r ROWS\t\tSkip ROWS rows from start of table.\n");
    fprintf(stderr, "\t-c COLS\t\tSkip COLS columns from start of each row.\n");
    fprintf(stderr, "\t-s SEP\t\tUse string SEP as field seperator, not \"\\t\".\n");
    fprintf(stderr, "\t-i INFILE\tInput from INFILE, not stdin (or '-' for stdin).\n");
    fprintf(stderr, "\t-o OUTFILE\tOutput to OUTFILE, not stdout (or '-' for stdout).\n");
    fprintf(stderr, "\t-h \t\tPrint this help message.\n");
}

int
print_header (table_t *tab, char *hdr)
{
    fprintf(tab->outfp, "%s", hdr);
    return 1;
}

int
parse_args (int argc, char *argv[], table_t *tab)
{
    assert(tab);
    tab->data = km_calloc(1, sizeof(ft_t), &km_onerr_print_exit);
    unsigned char haveflags = 0;
    /*
        1 1 1 1 1 1 1 1
            | | | | | \- method
            | | | | \--- out fname
            | | | \----- in fname
            | | \------- cols to skip
            | \--------- rows to skip
            \----------- Field sep
    */
    char c = '\0';
    while((c = getopt(argc, argv, "m:z:r:c:o:i:s:h")) >= 0) {
        switch (c) {
            case 'm':
                haveflags |= 1;
                tab->row_fn = &ft_median;
                strtocellt(&(((ft_t *)tab->data)->threshold), optarg, NULL, U64);
                break;
            case 'z':
                haveflags |= 1;
                tab->row_fn = &ft_num_nonzero;
                strtocellt(&(((ft_t *)tab->data)->threshold), optarg, NULL, U64);
                break;
            case 'o':
                haveflags |= 2;
                tab->outfname = strdup(optarg);
                break;
            case 'i':
                haveflags |= 4;
                tab->fname = strdup(optarg);
                break;
            case 'c':
                haveflags |= 8;
                tab->skipcol = atol(optarg);
                break;
            case 'r':
                haveflags |= 16;
                tab->skiprow = atol(optarg);
                break;
            case 's':
                haveflags |= 32;
                tab->sep = strdup(optarg);
                break;
            case 'h':
                print_usage();
                destroy_table_t(tab);
                exit(EXIT_SUCCESS);
        }
    }
    if (tab->sep == NULL) {
        tab->sep = strdup("\t");
    }
    /* Setup input fp */
    if ((!(haveflags & 4)) || tab->fname == NULL || \
            strncmp(tab->fname, "-", 1) == 0) {
        tab->fp = fdopen(fileno(stdin), "r");
        tab->fname = strdup("stdin");
        haveflags |= 4;
    } else {
        tab->fp = fopen(tab->fname, "r");
    }
    if (tab->fp == NULL) {
        fprintf(stderr, "Could not open file '%s'\n%s\n", tab->fname,
                strerror(errno));
        return 0;
    }
    /* Setup output fp */
    if ((!(haveflags & 2)) || tab->outfname == NULL || \
            strncmp(tab->outfname, "-", 1) == 0) {
        tab->outfp = fdopen(fileno(stdout), "w");
        tab->outfname = strdup("stdout");
        haveflags |= 2;
    } else {
        tab->outfp = fopen(tab->outfname, "w");
    }
    if (tab->outfp == NULL) {
        fprintf(stderr, "Could not open file '%s'\n%s\n", tab->outfname,
                strerror(errno));
        return 0;
    }
    if ((haveflags & 7) != 7) {
        fprintf(stderr, "[parse_args] Required arguments missing\n");
        return 0;
    }
    return 1; /* Successful */
}


/*
 * ===  FUNCTION  ======================================================================
 *         Name:  main
 * =====================================================================================
 */
int
main (int argc, char *argv[])
{
    if (argc == 1) {
        print_usage();
        exit(EXIT_SUCCESS);
    }
    table_t *tab = km_calloc(1, sizeof(*tab), &km_onerr_print_exit);
    tab->skipped_row_fn = &print_header;
    tab->skipped_col_fn = NULL;
    if (!parse_args(argc, argv, tab)) {
        destroy_table_t(tab);
        fprintf(stderr, "Cannot parse arguments.\n");
        print_usage();
        exit(EXIT_FAILURE);
    }
    if (!filter_table(tab)) {
        destroy_table_t(tab);
        fprintf(stderr, "Error during table filtering.\n");
        exit(EXIT_FAILURE);
    }
    destroy_table_t(tab);
    return EXIT_SUCCESS;
} /* ----------  end of function main  ---------- */
