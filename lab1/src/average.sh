sum=0
count=$#

for arg in "$@"; do
  sum=$(echo "$sum + $arg" | bc)
done
average=$(echo "scale=2; $sum / $count" | bc)
echo "Количество аргументов: $count"
echo "Среднее арифметическое: $average"
