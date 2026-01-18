echo "Counting from 1 to 10:\n"

var $i = 0
var $sum = 32
$sum = $sum + while $i < 10 {
    echo ($i + 1)
    $i = $i + 1
    $i
}

echo "\nSum: \($sum)\nDone!"
