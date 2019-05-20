# simple-nano-ethernet
A sketch to control outputs and read inputs of an ethernet connected Arduino Nano

## URL Endpoints:

### /input/get/\<input\>

\<input\> can be any number between 0 and 5.

Reads the value of one of the inputs A0 to A5. Returns "1" if the input is HIGH (5V), "0" if the input is LOW (0V).

### /output/set/\<output\>

\<output\> can be any number between 2 and 9.

Sets \<output\> HIGH.

### /output/toggle/\<output\>

\<output\> can be any number between 2 and 9.

Toggles \<output\>. If HIGH, becomes LOW. If LOW, becomes HIGH.

### /output/set/\<clear\>

\<output\> can be any number between 2 and 9.

Sets \<output\> LOW.

### /output/timedset/\<output\>/\<time\>

\<output\> can be any number between 2 and 9.
\<time\> is a time in milliseconds (will be rounded down to nearest 100ms)

Sets \<output\> HIGH for \<time\> milliseconds, then sets \<output\> LOW.
If output is already HIGH, output will still become LOW after <\time\> milliseconds.

### /output/timedtoggle/\<output\>/\<time\>

\<output\> can be any number between 2 and 9.
\<time\> is a time in milliseconds (will be rounded down to nearest 100ms)

Toggles \<output\> for \<time\> milliseconds.
If HIGH, becomes LOW. If LOW, becomes HIGH, then toggles again.

### /output/timedclear/\<clear\>/\<time\>

\<output\> can be any number between 2 and 9.
\<time\> is a time in milliseconds (will be rounded down to nearest 100ms)

Sets \<output\> LOW for \<time\> milliseconds, then sets \<output\> HIGH.
If output is already LOW, output will still become HIGH after <\time\> milliseconds.
