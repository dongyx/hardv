#!/bin/sh

# Print a blank line before quizzing a card, except the first quizzed card
! [ "$HARDV_FIRST" ] && printf '\n'

printf 'Q: %s\n' "$HARDV_Q"	# Print the question
printf '(recite) A: '		# Print the prompt
read a				# Read the user-input line to variable a
if [ "$a" = "!" ]; then
	exit 2;			# Skip this card
fi
if [ "$a" = "$HARDV_A" ]; then
	echo 'Correct!'
	exit 0;			# the user is able to recall
fi
printf 'Wrong! The answer is: %s\n' "$HARDV_A"
exit 1;				# the user is unable to recallxit 1;
