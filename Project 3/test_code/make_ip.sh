echo "name1 name2" > test

for i in {1..1000}; do
	echo -e $((1000 - $i))" "$i >> test
done
