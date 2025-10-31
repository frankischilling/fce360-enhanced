#include <xtl.h>
HRESULT ATG_LoadFile( const CHAR* strFileName, VOID** ppFileData, DWORD* pdwFileSize );

size_t LoadFile(char * rbuffer, char *filepath, size_t length, bool silent){
	DWORD FileSize;
	if(SUCCEEDED(ATG_LoadFile(filepath,(void**)rbuffer,&FileSize)))
	{
		return FileSize;
	}
	else
	{
		return 0;
	}
};
size_t LoadFile(char * filepath, bool silent);

//--------------------------------------------------------------------------------------
// Name: LoadFile()
// Desc: Helper function to load a file
//--------------------------------------------------------------------------------------
HRESULT ATG_LoadFile( const CHAR* strFileName, VOID** ppFileData, DWORD* pdwFileSize )
{
//    assert( ppFileData );
    if( pdwFileSize )
        *pdwFileSize = 0L;

    // Open the file for reading
    HANDLE hFile = CreateFile( strFileName, GENERIC_READ, 0, NULL,
                               OPEN_EXISTING, 0, NULL );

    if( INVALID_HANDLE_VALUE == hFile )
        return E_HANDLE;

    DWORD dwFileSize = GetFileSize( hFile, NULL );
    VOID* pFileData = malloc( dwFileSize );

    if( NULL == pFileData )
    {
        CloseHandle( hFile );
        return E_OUTOFMEMORY;
    }

    DWORD dwBytesRead;
    if( !ReadFile( hFile, pFileData, dwFileSize, &dwBytesRead, NULL ) )
    {
        CloseHandle( hFile );
        free( pFileData );
        return E_FAIL;
    }

    // Finished reading file
    CloseHandle( hFile );

    if( dwBytesRead != dwFileSize )
    {
        free( pFileData );
        return E_FAIL;
    }

    if( pdwFileSize )
        *pdwFileSize = dwFileSize;
    *ppFileData = pFileData;

    return S_OK;
}

