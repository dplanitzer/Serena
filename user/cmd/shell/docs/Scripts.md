# Scripts

A script is just a text file which contain a sequence of Serena shell commands and keywords. The following text shows an example of a simple script that declares a variables, increments it in a loop and prints out the current value of the variable at each loop step:

```
$var $i = 0

while $i < 10 {
    echo $i

    $i = $i + 1
}
```

You run a script like this:

```
shell my_script.sh
```

Assuming that the script is named 'my_script.sh' and is either stored in the current working directory or in one of the directories that are listed in the search path.

Every script is associated with its own independent script scope. All variables that are declared on the top-level of a script file are declared in this script scope. Thus all variables that you declare in a script ceases to exist after the script terminates.
