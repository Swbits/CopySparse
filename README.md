# CopySparse

Copy windows sparse file and keep timestamps.

Only allocate data part size on disk, maintaining sparseness, 
set NTFS sparse file attribute FSCTL_SET_SPARSE,
keep (Created, Accessed, Modified) timestamps unchanged.


```
Z:\Release>CopySparse.exe
CopySparse : Copy sparse file  v1.18
Usage: CopySparse srcFile destFile [-overwrite]
Tips: for /F %i in ('dir /b Temp\*.part') do CopySparse Temp\%i Backup\%i

Z:\Release>
```

2018/8/17
