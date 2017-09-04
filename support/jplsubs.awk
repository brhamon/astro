BEGIN {
	outfile="testeph_new.f"
}
{
	if ($0 ~ /^[ ]{6}SUBROUTINE FSIZER1\>/) {
		outfile="/dev/null"
	} else if ($0 ~ /^[ ]{6}SUBROUTINE PLEPH\>/) {
		outfile="jplsubs.f"
	}
	print > outfile
}
# vim:set ts=4 sw=4 sts=4 cindent noexpandtab:
