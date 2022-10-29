set -e

! [ "$HARDV_FIRST" ] && printf '\n'

printf 'Q: %s\n(rewrite) A: ' "$HARDV_Q"
read a

[ "$a" = "!" ] && exit 2;

if [ "$a" = "$HARDV_A" ]; then
	echo Correct!
	exit 0;
fi

printf 'Wrong! The answer is: %s\n' "$HARDV_A"
exit 1;
