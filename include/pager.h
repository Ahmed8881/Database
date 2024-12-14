#ifndef PAGER_H
#define PAGER_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// struct for pages
#define TABLE_MAX_PAGES 100
typedef struct
{
   int file_descriptor;  // basically number return by os when file is opened that
                         // if read or write to file
   uint32_t file_length; // length of file
   uint16_t num_pages;   // number of pages in file
   void *
       pages[TABLE_MAX_PAGES]; // array of pointrs where each pointer refers to a
                               // page and which takes data from disk as needed
} Pager;
Pager *pager_open(const char *file_name);
void *get_page(Pager *pager, uint32_t page_num);
void pager_flush(Pager *pager, uint32_t page_num);

#endif // PAGER_H