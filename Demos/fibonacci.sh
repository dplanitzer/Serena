var $n = 0
let $limit = 25

echo "The first \($limit) Fibonacci numbers are:"

while ($n < $limit) {
    var $a = 0
    var $b = 1
    var $i = 0

    while ($i < $n) {
        let $t = $a + $b
        $a = $b
        $b = $t
        $i = $i + 1
    }

    echo "\($a)"
    $n = $n + 1
}
