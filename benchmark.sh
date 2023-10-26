#/bin/zsh!

filelist="benchmarks.txt"

while IFS= read -r file_name; do
	./blif2bdd < $file_name -g -f -t 0
	./blif2bdd < $file_name -g -f -t 11
	./blif2bdd < $file_name -g -f -t 8
	./blif2bdd < $file_name -g -f -t 4
	./blif2bdd < $file_name -g -f -t 6
	./blif2bdd < $file_name -g -f -t 2
	./blif2bdd < $file_name -g -f -t 7
	./blif2bdd < $file_name -g -f -t 3
	./blif2bdd < $file_name -g -f -t 10
	./blif2bdd < $file_name -g -f -t 9
	./blif2bdd < $file_name -g -f -t 5
	./blif2bdd < $file_name -g -f -t 1
done < "$filelist"
