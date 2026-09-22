/*
 * Fadal's UEFI loader uses the distribution GNU-EFI script for the full
 * PE/COFF section layout. This wrapper keeps the expected source file in the
 * repository while allowing the Makefile's portable GNU-EFI path discovery
 * to select the installed script.
 */
INCLUDE "/usr/lib/elf_x86_64_efi.lds"
