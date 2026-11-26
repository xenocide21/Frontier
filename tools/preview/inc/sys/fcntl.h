/* Host preview: the BIOS file flags (values as in the SDK). */
#pragma once
#define FREAD   0x0001
#define FWRITE  0x0002
#define FCREATE 0x0200
#define FNBLOCKS(n) ((n) << 16)
