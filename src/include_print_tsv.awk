BEGIN{
	FS="\t"
	ww[2]=40
	ww[3]=20
}
{
	for(i=1; i<=NF; i++){
		f=$i
		if (f=="")	f=ff[i]
		ff[i]=f
		if (i==3){
			key=$1" "$2
			count[key]++
			f=f" ("count[key]")"
		}
		l=length(f)
		if (!(l<ww[i])) ww[i]=l
		{printf "%"(ww[i]+1)"s | ", f}
	}
	print ""
}
