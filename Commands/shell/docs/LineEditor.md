# Line Editor

You enter shell commands in the shell's line editor. The line editor allows you to enter, edit and recall previously entered commands. Please note that the line editor is currently limited to override/replace mode.

To enter a command, type it in the currently active line and hit the return/enter key. This triggers the execution of the command and it adds the command to the command history. A previously entered command may be recalled by pressing the cursor-up key. You may scroll through all commands in the command history by pressing the cursor-up and cursor-down keys repeatedly.

The line editor supports a number of editing functions which are described in the following table:

|Keys to Press | Description                                                    |
|--------------|----------------------------------------------------------------|
| Ctrl-a       | Move cursor to the start of the line                           |
| Ctrl-e       | Move cursor to the end of the line                             |
| Ctrl-b       | Move cursor one character to the left                          |
| Ctrl-f       | Move cursor one character to the right                         |
| Ctrl-l       | Clear screen and restart input                                 |
| Ctrl-d       | Delete the character under the cursor                          |
| Ctrl-i       | Toggles insert (default) and override mode                     |
| Ctrl-p       | Replace the input with the next older entry from the history   |
| Ctrl-n       | Replace the input with the next younger entry from the history |
| Cursor Left  | Move cursor one character to the left                          |
| Cursor Right | Move cursor one character to the right                         |
| Cursor Up    | Replace the input with the next older entry from the history   |
| Cursor Down  | Replace the input with the next younger entry from the history |
| Home         | Move cursor to the start of the line                           |
| End          | Move cursor to the end of the line                             |
| Backspace    | Delete the character to the left of the cursor                 |
| Delete       | Delete the character under the cursor                          |
| Insert       | Toggles insert (default) and override mode                     |

Not all key combinations are support on all platforms. Eg the Home and End keys are not available on standard Amiga keyboards.


