// csparse.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "csparse.h"
#include "stdio.h"
#define _WIN32_WINNT   0x0500
#include <Winioctl.h>
#include <Windows.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// The one and only application object

CWinApp theApp;

using namespace std;

const int MaxRanges = 4096;
int BufferSize = 512*1024*1024;   // 512M
char * FileBuffer = NULL;

int CopyPartFile(CFile & src,CFile & dest,FILE_ALLOCATED_RANGE_BUFFER & farb)
{
int i,n,rem;
UINT nBytesRead;

	// Seek to file offset 
	SetFilePointer((HANDLE)src.m_hFile, farb.FileOffset.LowPart, &farb.FileOffset.HighPart, FILE_BEGIN);   
	SetFilePointer((HANDLE)dest.m_hFile, farb.FileOffset.LowPart, &farb.FileOffset.HighPart, FILE_BEGIN);   

	n = farb.Length.QuadPart / BufferSize;
	rem = farb.Length.QuadPart - BufferSize * n;

	for(i=0;i<n;i++)
	{
		nBytesRead = src.Read(FileBuffer,BufferSize);
		dest.Write(FileBuffer,BufferSize);
	}

	if(rem)
	{
		nBytesRead = src.Read(FileBuffer,rem);
		dest.Write(FileBuffer,rem);
	}

	return 0;
}

char * commaNumStr(char * dest,__int64 num);

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		//cerr << _T("Fatal Error: MFC initialization failed") << endl;
		nRetCode = 1;
		return nRetCode;
	}

  /* Check available memory */
  MEMORYSTATUS stat;
  GlobalMemoryStatus (&stat);
/*
#define WIDTH 7
char *divisor = "K";
#define DIV 1024

  printf ("The MemoryStatus structure is %ld bytes long.\n",
          stat.dwLength);
  printf ("It should be %d.\n", sizeof (stat));
  printf ("%ld percent of memory is in use.\n",
          stat.dwMemoryLoad);
  printf ("There are %*ld total %sbytes of physical memory.\n",
          WIDTH, stat.dwTotalPhys/DIV, divisor);
  printf ("There are %*ld free %sbytes of physical memory.\n",
          WIDTH, stat.dwAvailPhys/DIV, divisor);
  printf ("There are %*ld total %sbytes of paging file.\n",
          WIDTH, stat.dwTotalPageFile/DIV, divisor);
  printf ("There are %*ld free %sbytes of paging file.\n",
          WIDTH, stat.dwAvailPageFile/DIV, divisor);
  printf ("There are %*lx total %sbytes of virtual memory.\n",
          WIDTH, stat.dwTotalVirtual/DIV, divisor);
  printf ("There are %*lx free %sbytes of virtual memory.\n",
          WIDTH, stat.dwAvailVirtual/DIV, divisor);
*/

	while( BufferSize > (stat.dwAvailPhys*2)/3)
	{
		BufferSize >>= 1;
	}

	CString srcName,destName;
	if( argc < 3 )
	{
		printf("CopySparse : Copy sparse file  v1.18\n");
		printf("Usage: CopySparse srcFile destFile [-overwrite]\n");
		printf("Tips: for /F %%i in ('dir /b Temp\\*.part') do CopySparse Temp\\%%i Backup\\%%i\n");
		return 0;
	}
	
	srcName = argv[1];
	destName = argv[2];

	CFile src,dest;
	CFileException fexpSrc;
	CFileException fexpDest;
	DWORD s, dwReturnedBytes = 0;

	if( srcName.CompareNoCase( destName ) == 0 )
	{
		printf("Source and destination file are identical.\n");
		return 1;
	}

	if (! src.Open(srcName, CFile::modeRead|CFile::shareDenyNone , &fexpSrc))
	{
		printf("Open src file %s error.\n",srcName);
		return 1;
	}

	if( argc < 4 || (argc == 4 && argv[3][1] != 'o') )
	{
		if( dest.Open(destName,CFile::modeRead, &fexpDest) )
		{
			printf("Destination file exists, use -o option to overwrite.\n");
			return 2;
		}
	}

	if (!dest.Open(destName,CFile::modeCreate|CFile::modeReadWrite|CFile::shareDenyWrite,&fexpDest))
	{
		printf("Create dest file %s error.\n",destName);
		return 3;
	}
	else
	{
		if (!DeviceIoControl((HANDLE)dest.m_hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwReturnedBytes, NULL))
		{
			// ERROR_INVALID_FUNCTION returned by WinXP when attempting to create a sparse file on a FAT32 partition
			DWORD dwError = GetLastError();
			//if (dwError != ERROR_INVALID_FUNCTION)
				printf(_T("Failed to apply NTFS sparse file attribute to file \"%s\" %d"), destName, dwError);
		}
	}


FILE_ALLOCATED_RANGE_BUFFER farb;
farb.FileOffset.QuadPart = 0;
farb.Length.QuadPart = 0xaabbccddeeff;
DWORD dwMaxSize = sizeof(farb);
FILE_ALLOCATED_RANGE_BUFFER * prgfarb = new FILE_ALLOCATED_RANGE_BUFFER[MaxRanges];
	dwMaxSize = sizeof(FILE_ALLOCATED_RANGE_BUFFER[MaxRanges]);

	s = DeviceIoControl((HANDLE)src.m_hFile,
		FSCTL_QUERY_ALLOCATED_RANGES,   
		&farb, sizeof(farb), prgfarb,
		dwMaxSize, &dwReturnedBytes, NULL);

	DWORD compSize;
	DWORD tick1,tick2,diff;
	char num1[20],num2[20];
	if( s )
	{
		FileBuffer = new char[BufferSize];

		printf("Copy sparse file \"%s\" to \"%s\"\n",srcName, destName);
		compSize = GetCompressedFileSize(srcName,NULL);
		printf("Size on disk = ");
		if( compSize > 2*1024*1024 )
			printf("%.2f MBytes\n", float(compSize)/ (1024*1024) );
		else
			printf("%.2f KBytes\n", float(compSize)/ 1024 );

		printf("BufferSize = %d MB\n",BufferSize >> 20);

		int i, n = dwReturnedBytes / sizeof(farb);
		
		if( n >= (MaxRanges-1) )
			printf("Max range reached, file may not finished.\n");

		tick1 = GetTickCount();
		for(i=0;i<n;i++)
		{
			printf("[%d] offset = %s , len = %s\n",i+1,
				commaNumStr(num1,prgfarb[i].FileOffset.QuadPart),
				commaNumStr(num2,prgfarb[i].Length.QuadPart)	);
			CopyPartFile(src,dest,prgfarb[i]);
		}
		tick2 = GetTickCount();

		delete FileBuffer;
	}
	else
	{
	   s = GetLastError();
	   printf("GetLastError = %d\n",s);
	}

	delete prgfarb;
	dest.Close();
	src.Close();

	CFileStatus status1,status2;

	CFile::GetStatus(srcName, status1 );
	CFile::GetStatus(destName, status2 );

	/* Set same file time(Created, Accessed, Modified) as src */
	status2.m_ctime = status1.m_ctime;
	status2.m_atime = status1.m_atime;
	status2.m_mtime = status1.m_mtime;
	CFile::SetStatus(destName, status2 );

	float rate;
	diff = tick2-tick1;
	if( diff > 0 ) {
		rate = float(compSize) * 1000 / float(diff);
	}
	else {
		rate = float(compSize) * 10000;
	}

	printf("Complete. %s / %.3f = ",commaNumStr(num1,compSize),float(diff)/1000);

	if( rate > 2*1024*1024 )
		printf("%.2f M", rate / (1024*1024) );
	else if( rate > 2*1024 )
		printf("%.2f K", rate / (1024) );
	else
		printf("%.2f ", rate );

	printf("Bytes/sec\n");

	return nRetCode;
}


char * commaNumStr(char * dest,__int64 num)
{
	char buf[20];
	int len,q,r,i;
	char * si,* di;

	_ui64toa(num,buf,10);

	len = strlen(buf);
	q = len / 3;
	r = len % 3;

	si = buf;
	di = dest;

	if( r == 0 )	{
		r = 3;
		if( q > 1)
			q --;
	}
	for(i=0; i<r; i++)	{
		*di++ = *si++;
	}

	for(i=0; i<q; i++)	{
		*di++ = ',';
		*di++ = *si++;
		*di++ = *si++;
		*di++ = *si++;
	}

	*di = 0;
	return dest;
}