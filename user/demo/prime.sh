var $n = 0
let $limit = 100

echo "The prime numbers < \($limit), are:"

while $n < $limit {
    if $n > 1 {
        let $m = $n / 2
        var $i = 2
        var $isPrime = true

        while $i <= $m {
            if $n % $i == 0 {
                $isPrime = false
                break
            }
            $i = $i + 1
        }

        if $isPrime {
            echo $n
        }
    }

    $n = $n + 1
}