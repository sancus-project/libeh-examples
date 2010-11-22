#!/bin/sh

split_lines() {
	local len="${1:-60}"

       	fmt -w60 | tr '\n' '|' | sed -e 's,|$,,' -e 's,|, \\\n\t,g'
}

list() {
	ls -1 "$@" | sort -V | tr '\n' ' ' | split_lines 60
}

cat <<EOT | tee Makefile.am
AM_CFLAGS = \$(CWARNFLAGS) \$(LIBEH_CFLAGS)
AM_LDFLAGS = \$(LIBEH_LIBS)

bin_PROGRAMS = \\
	$(ls -1 *.c | sed -e 's,\.c,,g' | sort -V | tr '\n' ' ' | split_lines 60)
EOT
for x in *.c; do
	[ -f "$x" ] || continue
	y=$(echo ${x%.c} | tr '.' '_')
	cat <<-EOT

	${y}_SOURCES = $x
	EOT
done | tee -a Makefile.am
