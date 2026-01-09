/**CXXFile****************************************************************

  FileName    [abcMapName.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    []

  Author      [Bingjin Han]
  
  Affiliation []

  Date        []

  Revision    []

***********************************************************************/

#include "base/abc/abc.h"
#include "misc/nm/nm.h"
#include "misc/vec/vecPtr.h"
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <misc/util/abc_global.h>
#include <string.h>
#include <unordered_map>
#include <string>

ABC_NAMESPACE_IMPL_START



static char * Abc_CecNormalizeNameRange( char * pBegin, char * pEnd )
{
    char * pRes;
    int i = 0;
    while ( pBegin < pEnd && isspace((unsigned char)*pBegin) )
        pBegin++;
    while ( pEnd > pBegin && isspace((unsigned char)pEnd[-1]) )
        pEnd--;
    if ( pBegin < pEnd && *pBegin == '\\' )
        pBegin++;
    pRes = ABC_ALLOC( char, (pEnd - pBegin) + 1 );
    for ( ; pBegin < pEnd; pBegin++ )
        if ( !isspace((unsigned char)*pBegin) )
            pRes[i++] = *pBegin;
    pRes[i] = '\0';
    return pRes;
}

static char * Abc_CecFindToken( char * pLine, const char * pTok, char ** ppTokEnd )
{
    size_t tok_len = strlen( pTok );
    char * p = pLine;
    while ( *p )
    {
        char * pStart;
        while ( *p && isspace((unsigned char)*p) )
            p++;
        if ( *p == '\0' )
            break;
        pStart = p;
        while ( *p && !isspace((unsigned char)*p) )
            p++;
        if ( (size_t)(p - pStart) == tok_len && !strncmp( pStart, pTok, tok_len ) )
        {
            if ( ppTokEnd )
                *ppTokEnd = p;
            return pStart;
        }
    }
    return NULL;
}

static int Abc_CecParseYosysMatch( char * pLine, char ** ppName1, char ** ppName2 )
{
    char * pGoldTok, * pGoldEnd, * pGateTok, * pGateEndTok;
    char * pGoldName, * pGateName, * pGateEnd;
    pGoldTok = Abc_CecFindToken( pLine, "gold", &pGoldEnd );
    if ( pGoldTok == NULL )
        return 0;
    pGateTok = Abc_CecFindToken( pGoldEnd, "gate", &pGateEndTok );
    if ( pGateTok == NULL )
        return 0;
    pGoldName = pGoldEnd;
    while ( *pGoldName && isspace((unsigned char)*pGoldName) )
        pGoldName++;
    pGateName = pGateEndTok;
    while ( *pGateName && isspace((unsigned char)*pGateName) )
        pGateName++;
    pGateEnd = strstr( pGateName, "," );
    if ( pGateEnd == NULL )
        pGateEnd = pLine + strlen(pLine);
    *ppName1 = Abc_CecNormalizeNameRange( pGateName, pGateEnd );
    *ppName2 = Abc_CecNormalizeNameRange( pGoldName, pGateTok );
    if ( (*ppName1)[0] == '\0' || (*ppName2)[0] == '\0' )
    {
        ABC_FREE( *ppName1 );
        ABC_FREE( *ppName2 );
        return 0;
    }
    return 1;
}

static int Abc_CecParsePair( char * pLine, char ** ppName1, char ** ppName2 )
{
    char * pStart;
    char * pMid;
    char * pEnd;
    pStart = pLine;
    while ( *pStart && isspace((unsigned char)*pStart) )
        pStart++;
    if ( *pStart == '\0' || *pStart == '#' )
        return 0;
    pMid = pStart;
    while ( *pMid && !isspace((unsigned char)*pMid) )
        pMid++;
    if ( *pMid == '\0' )
        return 0;
    *pMid = '\0';
    pMid++;
    while ( *pMid && isspace((unsigned char)*pMid) )
        pMid++;
    if ( *pMid == '\0' )
        return 0;
    pEnd = pMid;
    while ( *pEnd && !isspace((unsigned char)*pEnd) )
        pEnd++;
    *pEnd = '\0';
    *ppName1 = Abc_CecNormalizeNameRange( pStart, pStart + strlen(pStart) );
    *ppName2 = Abc_CecNormalizeNameRange( pMid, pMid + strlen(pMid) );
    if ( (*ppName1)[0] == '\0' || (*ppName2)[0] == '\0' )
    {
        ABC_FREE( *ppName1 );
        ABC_FREE( *ppName2 );
        return 0;
    }
    return 1;
}

static Vec_Ptr_t * Abc_CecNameMapRead( char * pFileName, int fVerbose )
{
    FILE * pFile;
    Vec_Ptr_t * vPairs;
    char Buffer[4096];
    pFile = fopen( pFileName, "r" );
    if ( pFile == NULL )
    {
        Abc_Print( -1, "Cannot open name map file \"%s\".\n", pFileName );
        return NULL;
    }
    vPairs = Vec_PtrAlloc( 100 );
    while ( fgets( Buffer, sizeof(Buffer), pFile ) )
    {
        char * pName1 = NULL;
        char * pName2 = NULL;
        if ( Abc_CecParseYosysMatch( Buffer, &pName1, &pName2 ) ||
             (strstr( Buffer, "Matched signal" ) == NULL &&
              Abc_CecParsePair( Buffer, &pName1, &pName2 )) )
        {
            Vec_PtrPush( vPairs, pName1 );
            Vec_PtrPush( vPairs, pName2 );
        }
    }
    fclose( pFile );

    // for(int i=0; i<Vec_PtrSize(vPairs); i+=2) {
    //     Abc_Print(1, "%s <--> %s\n", (char*)Vec_PtrEntry(vPairs, i),
    //                                (char*)Vec_PtrEntry(vPairs, i+1));
    // }

    if ( Vec_PtrSize(vPairs) == 0 )
    {
        Abc_Print( 0, "Name map file \"%s\" has no usable entries.\n", pFileName );
        //Vec_PtrFree( vPairs );
        return vPairs;
    }
    if ( fVerbose )
        Abc_Print( 0, "Read %d name mappings from \"%s\".\n", Vec_PtrSize(vPairs)/2, pFileName );
    return vPairs;
}

static void Abc_CecNameMapFree( Vec_Ptr_t * vPairs )
{
    char * pName;
    int i;
    Vec_PtrForEachEntry( char *, vPairs, pName, i )
        ABC_FREE( pName );
    Vec_PtrFree( vPairs );
}

static void print_info(Abc_Ntk_t* pNtk1, Abc_Ntk_t* pNtk2){
    Abc_Obj_t * pNode;
    int j=0;
    Abc_NtkForEachNode(pNtk1, pNode, j) {
        Abc_Print(1, "pNtk1.pNode = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachNode(pNtk2, pNode, j) {
        Abc_Print(1, "pNtk2.pNode = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachLatch(pNtk1, pNode, j) {
        Abc_Print(1, "pNtk1.pLatch = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachLatch(pNtk2, pNode, j) {
        Abc_Print(1, "pNtk2.pLatch = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachCi(pNtk1, pNode, j) {
        Abc_Print(1, "pNtk1.Ci = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachCi(pNtk2, pNode, j) {
        Abc_Print(1, "pNtk2.Ci = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachCo(pNtk1, pNode, j) {
        Abc_Print(1, "pNtk1.Co = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachCo(pNtk2, pNode, j) {
        Abc_Print(1, "pNtk2.Co = %s\n", Abc_ObjName(pNode));
    }

    Abc_NtkForEachPo(pNtk1, pNode, j) {
        Abc_Print(1, "pNtk1.Po = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachPo(pNtk2, pNode, j) {
        Abc_Print(1, "pNtk2.Po = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachPi(pNtk1, pNode, j) {
        Abc_Print(1, "pNtk1.Pi = %s\n", Abc_ObjName(pNode));
    }
    Abc_NtkForEachPi(pNtk2, pNode, j) {
        Abc_Print(1, "pNtk2.Pi = %s\n", Abc_ObjName(pNode));
    }
}

extern "C" int Abc_CecApplyNameMap( Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, char * pFileName, int fVerbose )
{
    Vec_Ptr_t * vPairs = Abc_CecNameMapRead( pFileName, fVerbose );
    if(!vPairs) return -1;
    char buffer[1000];
    // print_info(pNtk1, pNtk2);

    int i, nApplied = 0, nMiss1 = 0, nMiss2 = 0, nConflicts = 0;

    std::unordered_map<std::string, std::string> name_map; 
    for ( i = 0; i < Vec_PtrSize(vPairs); i += 2 ) { 
        std::string name1 = (char *)Vec_PtrEntry( vPairs, i );
        std::string name2 = (char *)Vec_PtrEntry( vPairs, i+1 );
        if(name1 != name2){
            name_map[name1] = name2;
            // Abc_Print(-2, "%s <----> %s\n", name1.c_str(),name2.c_str());
        }
            
    }

    Abc_Obj_t *pObj1, *pObj2;
    Abc_NtkForEachCi(pNtk1, pObj1, i) {
        std::string name1 = std::string((char*)Abc_ObjName(pObj1));
        if(!name_map.count(std::string(name1))) {
            nMiss1++;
            continue;
        }
                    
        auto name2 = name_map[std::string(name1)];
        sprintf(buffer,"%s", name2.c_str());
        pObj2 = Abc_NtkFindCi(pNtk2, buffer);
        
        if(!pObj2) { //find!
            nMiss2++;
            continue;
        } 
        
        auto pName2 = Abc_ObjName(pObj2);
        if ( Nm_ManFindIdByName( pNtk1->pManName, pName2, pObj1->Type ) >= 0 &&
             Nm_ManFindIdByName( pNtk1->pManName, pName2, pObj1->Type ) != pObj1->Id )
        {
            nConflicts++;
            Abc_Print( 0, "Name map: \"%s\" already exists in network1.\n", pName2 );
            continue;
        }
        Nm_ManDeleteIdName( pNtk1->pManName, pObj1->Id );
        Abc_ObjAssignName(pObj1, pName2, NULL);
        nApplied++;
    }

    Abc_NtkForEachCo(pNtk1, pObj1, i) {
        std::string name1 = std::string((char*)Abc_ObjName(pObj1));
        if(!name_map.count(std::string(name1))) {
            nMiss1++;
            continue;
        }
                    
        auto name2 = name_map[std::string(name1)];
        sprintf(buffer,"%s", name2.c_str());
        pObj2 = Abc_NtkFindCo(pNtk2, buffer);
        
        if(!pObj2) { //find!
            nMiss2++;
            continue;
        } 
        
        auto pName2 = Abc_ObjName(pObj2);
        if ( Nm_ManFindIdByName( pNtk1->pManName, pName2, pObj1->Type ) >= 0 &&
             Nm_ManFindIdByName( pNtk1->pManName, pName2, pObj1->Type ) != pObj1->Id )
        {
            nConflicts++;
            Abc_Print( 0, "Name map: \"%s\" already exists in network1.\n", pName2 );
            continue;
        }
        Nm_ManDeleteIdName( pNtk1->pManName, pObj1->Id );
        Abc_ObjAssignName(pObj1, pName2, NULL);
        nApplied++;
    }

    if ( fVerbose || nMiss1 || nMiss2 || nConflicts )
        Abc_Print( 0, "Name map: applied %d, missing1 %d, missing2 %d, conflicts %d.\n",
            nApplied, nMiss1, nMiss2, nConflicts );
    Abc_CecNameMapFree(vPairs);
    return nApplied;
}


ABC_NAMESPACE_IMPL_END
