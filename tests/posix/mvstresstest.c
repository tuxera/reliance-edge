/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2019 Datalight, Inc.
                       All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
/** @file
    @brief Stress test that can exercise multiple Reliance Edge volumes at once.
*/
#include <stdlib.h>

#include <redfs.h>
#include <redposix.h>
#include <redvolume.h>
#include <redtests.h>
#include <redtestutils.h>
#include <redgetopt.h>
#include <redtoolcmn.h>

#if MVSTRESSTEST_SUPPORTED


#define TEST_NAME "MultiVolStressTest"

/*  Default test parameters
*/
#define DEFAULT_FILESPERMP  (4U)
#define DEFAULT_FILESIZE    (256U * 1024U)
#define DEFAULT_MAXOPSIZE   (16U * 1024U)
#define DEFAULT_ITERATIONS  (10000U)
#define DEFAULT_SEED        (1U)


typedef enum
{
    TESTOP_INVALID,
    TESTOP_READ,
    TESTOP_WRITE,
    TESTOP_TRUNCATE,
    TESTOP_REMOUNT,
    TESTOP_TP
} MVSTESTOP;

typedef struct
{
    MVSTESTOP   op;
    uint32_t    ulFreq;
} MVSTESTOPFREQ;

typedef struct
{
    char       *pszPath;    /* Absolute path to the file */
    int32_t     iFildes;    /* Open file descriptor */
    uint8_t    *pbMirror;   /* In-memory copy of the file data */
    uint32_t    ulFileSize; /* Current file size */
} MVSFILEINFO;

typedef struct
{
    uint8_t         bVolNum;            /* Volume number */
    const char     *pszVolume;          /* Volume name */
    MVSFILEINFO    *pFiles;             /* Array of files on the volume */
    MVSFILEINFO    *pFilesTransacted;   /* pFiles at the time of the last TP */
    uint32_t        ulOrigTransMask;    /* Original transaction mask */
} MVSVOLINFO;

typedef struct
{
    MVSTRESSTESTPARAM   param;      /* Test parameters */
    MVSVOLINFO         *pVolData;   /* Array of volume data */
    uint8_t            *pbOpBuffer; /* Buffer for read/write I/O */
} MVSTRESSTESTINFO;


static void ShowHelp(const char *pszProgramName);
static bool VolumesAreValid(const MVSTRESSTESTINFO *pTI);
static REDSTATUS TestExecute(MVSTRESSTESTINFO *pTI);
static REDSTATUS TestSetup(MVSTRESSTESTINFO *pTI);
static void TestCleanup(MVSTRESSTESTINFO *pTI);
static void TestEchoParameters(const MVSTRESSTESTINFO *pTI);
static int DoTransaction(MVSTRESSTESTINFO *pTI, MVSVOLINFO *pVol);


/** @brief Parse parameters for mvstress.

    @param argc     The number of arguments from the command line.
    @param argv     The vector of arguments from the command line.
    @param pParam   Populated with the mvstress arguments.

    @return The result of parsing the parameters.
*/
PARAMSTATUS MultiVolStressTestParseParams(
    int                 argc,
    char               *argv[],
    MVSTRESSTESTPARAM  *pParam)
{
    uint32_t            ulDevCount = 0U;
    uint32_t            i;
    int32_t             c;
    const REDOPTION     aLongopts[] =
    {
        { "file-count", red_required_argument, NULL, 'f' },
        { "file-size", red_required_argument, NULL, 'z' },
        { "buffer-size", red_required_argument, NULL, 'b' },
        { "iterations", red_required_argument, NULL, 'n' },
        { "seed", red_required_argument, NULL, 's' },
        { "dev", red_required_argument, NULL, 'D' },
        { "help", red_no_argument, NULL, 'H' },
        { NULL }
    };

    /*  If the test is run without arguments, treat that as a help request.
    */
    if(argc == 1)
    {
        goto Help;
    }

    MultiVolStressTestDefaultParams(pParam);
    RedGetoptReset();
    while((c = RedGetoptLong(argc, argv, "f:z:b:n:s:D:H", aLongopts, NULL)) != -1)
    {
        switch(c)
        {
            case 'f': /* --files */
            {
                pParam->ulFilesPerVol = RedAtoI(red_optarg);
                if(pParam->ulFilesPerVol == 0U)
                {
                    RedPrintf("Bad argument to --files: \"%s\" (must exceed zero)\n", red_optarg);
                    goto BadOpt;
                }

                break;
            }
            case 'z': /* --fsize */
            {
                const char *pszTmp;

                pszTmp = RedSizeToUL(red_optarg, &pParam->ulMaxFileSize);
                if((pszTmp == NULL) || (pszTmp[0U] != '\0'))
                {
                    RedPrintf("\"%s\" is not a valid size number\n", red_optarg);
                    goto BadOpt;
                }

                if(pParam->ulMaxFileSize == 0U)
                {
                    RedPrintf("Bad argument to --fsize: \"%s\" (must exceed zero)\n", red_optarg);
                    goto BadOpt;
                }

                break;
            }
            case 'b': /* --buffer-size */
            {
                const char *pszTmp = red_optarg;

                pszTmp = RedSizeToUL(pszTmp, &pParam->ulMaxOpSize);
                if((pszTmp == NULL) || (pszTmp[0U] != '\0'))
                {
                    RedPrintf("\"%s\" is not a valid size number\n", red_optarg);
                    goto BadOpt;
                }

                if(pParam->ulMaxOpSize == 0U)
                {
                    RedPrintf("Bad argument to --buffer-size: \"%s\" (must exceed zero)\n", red_optarg);
                    goto BadOpt;
                }

                break;
            }
            case 'n': /* --iterations */
            {
                pParam->ulIterations = RedAtoI(red_optarg);
                break;
            }
            case 's': /* --seed */
            {
                const char *pszStr;

                pszStr = RedNtoUL(red_optarg, &pParam->ulSeed);
                if((pszStr == NULL) || (pszStr[0U] != '\0'))
                {
                    RedPrintf("\"%s\" is not a valid number\n", red_optarg);
                    goto BadOpt;
                }

                if(pParam->ulSeed == 0U)
                {
                    RedPrintf("A random seed value of 0 will use the current time.\n\n");
                    pParam->ulSeed = RedOsClockGetTime();
                }

                break;
            }
            case 'D': /* --dev */
            {
                if(ulDevCount >= ARRAY_SIZE(pParam->apszDevices))
                {
                    RedPrintf("Cannot have more device arguments than the volume count\n");
                    goto BadOpt;
                }

                /*  Unlike most other tests, this one allows the device argument
                    to be specified more than once.  The convention is that the
                    devices are associated to volumes in the same order that
                    they are specified on the command line.
                */
                pParam->apszDevices[ulDevCount] = red_optarg;
                ulDevCount++;
                break;
            }
            case 'H': /* --help */
                goto Help;
            case '?': /* Unknown or ambiguous option */
            case ':': /* Option missing required argument */
            default:
                goto BadOpt;
        }
    }

    /*  RedGetoptLong() has permuted argv to move all non-option arguments to
        the end.  We expect to find at least one Reliance Edge volume.
    */
    pParam->ulVolumeCount = (uint32_t)(argc - red_optind);

    if(pParam->ulVolumeCount < 1U)
    {
        RedPrintf("Missing volume ID argument\n");
        goto BadOpt;
    }

    if(pParam->ulVolumeCount > ARRAY_SIZE(pParam->apszVolumes))
    {
        RedPrintf("Number of volume ID arguments exceeds volume count\n");
        goto BadOpt;
    }

    for(i = 0U; i < pParam->ulVolumeCount; i++)
    {
        const char *pszVolumeID = argv[(uint32_t)red_optind + i];
        uint8_t     bVolNum;

        bVolNum = RedFindVolumeNumber(pszVolumeID);
        if(bVolNum == REDCONF_VOLUME_COUNT)
        {
            RedPrintf("Error: \"%s\" is not a valid volume identifier.\n", pszVolumeID);
            goto BadOpt;
        }

        pParam->abVolNum[i] = bVolNum;
        pParam->apszVolumes[i] = gaRedVolConf[bVolNum].pszPathPrefix;
    }

    return PARAMSTATUS_OK;

  BadOpt:

    RedPrintf("Invalid command line arguments\n");
    ShowHelp(argv[0U]);
    return PARAMSTATUS_BAD;

  Help:

    ShowHelp(argv[0U]);
    return PARAMSTATUS_HELP;
}


/** @brief Set default MultiFsTest parameters.

    @param pParam   Populated with the default MultiFsTest parameters.
*/
void MultiVolStressTestDefaultParams(
    MVSTRESSTESTPARAM  *pParam)
{
    RedMemSet(pParam, 0, sizeof(*pParam));
    pParam->ulFilesPerVol = DEFAULT_FILESPERMP;
    pParam->ulMaxFileSize = DEFAULT_FILESIZE;
    pParam->ulMaxOpSize = DEFAULT_MAXOPSIZE;
    pParam->ulIterations = DEFAULT_ITERATIONS;
    pParam->ulSeed = DEFAULT_SEED;
}


/** @brief Start the MultiFsTest.

    @param pParam   MultiFsTest, either from MultiFsTestParseParams() or
                    constructed programatically.

    @return Zero on success, otherwise nonzero.
*/
int MultiVolStressTestStart(
    const MVSTRESSTESTPARAM    *pParam)
{
    MVSTRESSTESTINFO            ti = {0U};
    REDSTATUS                   ret;

    ti.param = *pParam;

    if(VolumesAreValid(&ti))
    {
        ret = TestExecute(&ti);
    }
    else
    {
        ret = -RED_EINVAL;
    }

    return (int)-ret;
}


/** @brief Print a help message.

    @param pszProgramName   The invocation name of the test.
*/
static void ShowHelp(
    const char *pszProgramName)
{
    RedPrintf("usage: %s VolumeID [AdditionalVolumeIDs...] [Options]\n", pszProgramName);
    RedPrintf("Stress test which can exercise multiple volumes.\n\n");
    RedPrintf("Where:\n");
    RedPrintf("  VolumeID\n");
    RedPrintf("      A volume number (e.g., 2) or a volume path prefix (e.g., VOL1: or /data)\n");
    RedPrintf("      of the volume to test.\n");
    RedPrintf("  AdditionalVolumeIDs\n");
    RedPrintf("      Optional additional VolumeIDs to test multiple volumes.\n");
    RedPrintf("And 'Options' are any of the following:\n");
    RedPrintf("  --file-count=n, -f n\n");
    RedPrintf("      The number of files to use on each volume (default %u).\n", DEFAULT_FILESPERMP);
    RedPrintf("  --file-size=size, -z size\n");
    RedPrintf("      The size of each file during the test (default %uKB).\n", DEFAULT_FILESIZE / 1024U);
    RedPrintf("  --buffer-size=size, -b size\n");
    RedPrintf("      The buffer size to allocate, which will be the maximum size for read and\n");
    RedPrintf("      write operations (default %uKB).\n", DEFAULT_MAXOPSIZE / 1024U);
    RedPrintf("  --iterations=count, -n count\n");
    RedPrintf("      Specifies the number of test iterations to run (default %u).\n", DEFAULT_ITERATIONS);
    RedPrintf("  --seed=n, -s n\n");
    RedPrintf("      Specifies the random seed to use (default is %u; 0 to use timestamp).\n", DEFAULT_SEED);
    RedPrintf("  --dev=devname, -D devname\n");
    RedPrintf("      Specifies device names for the test volumes.  Because this is a multivolume\n");
    RedPrintf("      test, this parameter may be specified multiple times: the device names are\n");
    RedPrintf("      associated with the volumes in the order they are given on the command line.\n");
    RedPrintf("      For example, the first device name is associated with the first volume ID,\n");
    RedPrintf("      the second device name with the second volume ID, etc.  Device names are\n");
    RedPrintf("      typically only meaningful when running the test on a host machine.  This can\n");
    RedPrintf("      be \"ram\" to test on a RAM disk, the path and name of a file disk (e.g.,\n");
    RedPrintf("      red.bin); or an OS-specific reference to a device (on Windows, a drive\n");
    RedPrintf("      letter like G: or a device name like \\\\.\\PhysicalDrive7; on Linux, a\n");
    RedPrintf("      device file like /dev/sdb).\n");
    RedPrintf("  --help, -H\n");
    RedPrintf("      Prints this usage text and exits.\n\n");
    RedPrintf("Warning: This test will format all test volumes -- destroying all existing data.\n\n");
}


/** @brief Validate the volume IDs provided to the test.

    @param pTI  Test context information.

    @return Whether the volume IDs are valid.
*/
static bool VolumesAreValid(
    const MVSTRESSTESTINFO *pTI)
{
    uint32_t                i;
    uint32_t                j;
    bool                    fValid = true;

    for(i = 0U; (i < pTI->param.ulVolumeCount) && fValid; i++)
    {
        const char *pszVol = pTI->param.apszVolumes[i];
        uint8_t     bVolNum;

        bVolNum = RedFindVolumeNumber(pszVol);
        if(bVolNum == REDCONF_VOLUME_COUNT)
        {
            RedPrintf("Error: \"%s\" is not a valid volume identifier.\n", pszVol);
            fValid = false;
        }
        else if(bVolNum != pTI->param.abVolNum[i])
        {
            RedPrintf("Error: \"%s\" is volume #%u, not volume #%u.\n",
                pszVol, (unsigned)bVolNum, (unsigned)pTI->param.abVolNum[i]);
            fValid = false;
        }
        else
        {
            /*  Check for duplicate volumes.
            */
            for(j = 0U; (j < i) && fValid; j++)
            {
                if(bVolNum == pTI->param.abVolNum[j])
                {
                    RedPrintf("Error: Volume #%u (\"%s\") specified more than once\n",
                        (unsigned)bVolNum, gaRedVolConf[bVolNum].pszPathPrefix);
                    fValid = false;
                }
            }
        }
    }

    return fValid;
}


/** @brief Run mvstress.

    @param pTI  Test context information.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS TestExecute(
    MVSTRESSTESTINFO *pTI)
{
    static const MVSTESTOPFREQ aOpFreqs[] =
    {
        { TESTOP_WRITE, 1000U },
        { TESTOP_READ, 1000U },
        { TESTOP_TRUNCATE, 10U },
        { TESTOP_TP, 10U },
        { TESTOP_REMOUNT, 10U }
    };

    uint32_t    ulSeed = pTI->param.ulSeed;
    uint32_t    ulIter;
    uint32_t    ulOpFreqTotal;
    uint32_t    i;
    int32_t     iErr;
    REDSTATUS   ret;

    RedPrintf(TEST_NAME " setting up...\n");

    ret = TestSetup(pTI);
    if(ret != 0)
    {
        goto Out;
    }

    TestEchoParameters(pTI);

    ulOpFreqTotal = 0U;
    for(i = 0U; i < ARRAY_SIZE(aOpFreqs); i++)
    {
        ulOpFreqTotal += aOpFreqs[i].ulFreq;
    }

    RedPrintf(TEST_NAME " running...\n");
    RedPrintf("iter\top\toffset\tlen\tpath\n");

    /*  On each iteration, pick a random mount point and a random file therein
        and do a random thing.
    */
    for(ulIter = 0U; ulIter < pTI->param.ulIterations; ulIter++)
    {
        MVSVOLINFO     *pVol = &pTI->pVolData[RedRand32(&ulSeed) % pTI->param.ulVolumeCount];
        MVSFILEINFO    *pFile = &pVol->pFiles[RedRand32(&ulSeed) % pTI->param.ulFilesPerVol];
        uint32_t        ulRandOp;
        uint32_t        ulOpFreqSum;
        MVSTESTOP       op;

        /*  Select a random operation, taking into account the operation
            frequency.
        */
        ulRandOp = RedRand32(&ulSeed) % ulOpFreqTotal;
        ulOpFreqSum = 0U;
        for(i = 0U; i < ARRAY_SIZE(aOpFreqs); i++)
        {
            ulOpFreqSum += aOpFreqs[i].ulFreq;
            if(ulOpFreqSum > ulRandOp)
            {
                op = aOpFreqs[i].op;
                break;
            }
        }

        switch(op)
        {
            case TESTOP_READ:
            {
                uint32_t ulReadLen = RedRand32(&ulSeed) % (pTI->param.ulMaxOpSize + 1U);
                uint32_t ulOffset = (pFile->ulFileSize == 0U) ? 0U : (RedRand32(&ulSeed) % pFile->ulFileSize);
                uint32_t ulLenExpect = REDMIN(pFile->ulFileSize - ulOffset, ulReadLen);
                int32_t  iLenActual;
                int64_t  llPos;

                RedPrintf("%lu\tREAD\t%lu\t%lu\t%s\n", (unsigned long)ulIter,
                    (unsigned long)ulOffset, (unsigned long)ulReadLen, pFile->pszPath);

                llPos = red_lseek(pFile->iFildes, ulOffset, RED_SEEK_SET);
                if((uint32_t)llPos != ulOffset)
                {
                    if(llPos < 0)
                    {
                        ret = -red_errno;
                    }
                    else
                    {
                        ret = -RED_EINVAL;
                    }

                    RedPrintf("red_seek() to offset %lu failed with errno %d\n", (unsigned long)ulOffset, (int)-ret);
                    goto Cleanup;
                }

                iLenActual = red_read(pFile->iFildes, pTI->pbOpBuffer, ulReadLen);
                if((uint32_t)iLenActual != ulLenExpect)
                {
                    if(iLenActual < 0)
                    {
                        ret = -red_errno;
                        RedPrintf("red_read() failed with errno %d\n", (int)-ret);
                    }
                    else
                    {
                        ret = -RED_EINVAL;
                        RedPrintf("Unexpected short read of file \"%s\": expected %lu bytes, received %lu\n",
                            (unsigned long)pFile->pszPath, (unsigned long)ulLenExpect, (unsigned long)iLenActual);
                    }

                    goto Cleanup;
                }

                /*  Make sure the read returned the expected data.
                */
                if(RedMemCmp(pTI->pbOpBuffer, &pFile->pbMirror[ulOffset], ulLenExpect) != 0)
                {
                    for(i = 0U; i < ulLenExpect; i++)
                    {
                        if(pTI->pbOpBuffer[i] != pFile->pbMirror[ulOffset + i])
                        {
                            RedPrintf("Mismatch reading file \"%s\" offset 0x%lx len 0x%lx\n",
                                (unsigned long)pFile->pszPath, (unsigned long)ulOffset, (unsigned long)ulLenExpect);
                            RedPrintf("Failed at buffer offset 0x%lx, file offset 0x%lx (block 0x%lx, off 0x%lx)\n",
                                (unsigned long)i, (unsigned long)(ulOffset + i),
                                (unsigned long)((ulOffset + i) >> BLOCK_SIZE_P2),
                                (unsigned long)((ulOffset + i) & (REDCONF_BLOCK_SIZE - 1U)));
                            RedPrintf("Found byte 0x%02x, expected byte 0x%02x\n",
                                (unsigned)pTI->pbOpBuffer[i], (unsigned)pFile->pbMirror[ulOffset + i]);
                            ret = -RED_EIO;
                            goto Cleanup;
                        }
                    }
                }

                break;
            }
            case TESTOP_WRITE:
            {
                uint32_t ulOffset = RedRand32(&ulSeed) % (pTI->param.ulMaxFileSize + 1U);
                uint32_t ulWriteLen = RedRand32(&ulSeed) % (REDMIN(pTI->param.ulMaxOpSize, pTI->param.ulMaxFileSize - ulOffset) + 1U);
                uint8_t  bOpByte = (uint8_t)(ulIter & 0xFFU);
                uint32_t ulBufferIndex = 0U;
                int32_t  iLenActual;
                int64_t  llPos;

                RedPrintf("%lu\tWRITE\t%lu\t%lu\t%s\n", (unsigned long)ulIter,
                    (unsigned long)ulOffset, (unsigned long)ulWriteLen, pFile->pszPath);

                llPos = red_lseek(pFile->iFildes, ulOffset, RED_SEEK_SET);
                if((uint32_t)llPos != ulOffset)
                {
                    if(llPos < 0)
                    {
                        ret = -red_errno;
                    }
                    else
                    {
                        ret = -RED_EINVAL;
                    }

                    RedPrintf("red_seek() to offset %lu failed with errno %d\n", (unsigned long)ulOffset, (int)-ret);
                    goto Cleanup;
                }

                if(pFile->ulFileSize < ulOffset)
                {
                    RedMemSet(&pFile->pbMirror[pFile->ulFileSize], 0U, ulOffset - pFile->ulFileSize);
                }

                RedMemSet(pTI->pbOpBuffer, bOpByte, ulWriteLen);

                iLenActual = red_write(pFile->iFildes, pTI->pbOpBuffer, ulWriteLen);
                if((iLenActual < 0) && (red_errno == RED_ENOSPC))
                {
                    iLenActual = 0;
                }

                if((iLenActual >= 0) && ((uint32_t)iLenActual < ulWriteLen))
                {
                    RedMemCpy(&pFile->pbMirror[ulOffset], pTI->pbOpBuffer, iLenActual);

                    if((pFile->ulFileSize < (ulOffset + iLenActual)) && (iLenActual > 0))
                    {
                        pFile->ulFileSize = ulOffset + iLenActual;
                    }

                    iErr = DoTransaction(pTI, pVol);
                    if(iErr != 0)
                    {
                        ret = -red_errno;
                        RedPrintf("red_transact(\"%s\") failed with errno %d\n", pVol->pszVolume, (int)-ret);
                        goto Cleanup;
                    }

                    ulWriteLen -= iLenActual;
                    ulOffset += iLenActual;
                    ulBufferIndex = iLenActual;

                    iLenActual = red_write(pFile->iFildes, &pTI->pbOpBuffer[ulBufferIndex], ulWriteLen);
                }

                if(iLenActual < 0)
                {
                    /*  ENOSPC conditions might happen, not really an error.
                    */
                    if(red_errno != RED_ENOSPC)
                    {
                        ret = -red_errno;
                        RedPrintf("red_read() failed with errno %d\n", (int)-ret);
                        goto Cleanup;
                    }
                }

                /*  Update the in-memory copy of the file so we can validate
                    the file contents after a read.
                */
                RedMemCpy(&pFile->pbMirror[ulOffset], &pTI->pbOpBuffer[ulBufferIndex], iLenActual);

                if((pFile->ulFileSize < (ulOffset + iLenActual)) && (iLenActual > 0))
                {
                    pFile->ulFileSize = ulOffset + iLenActual;
                }

                break;
            }
            case TESTOP_TRUNCATE:
            {
                uint32_t ulOldFileSize = pFile->ulFileSize;
                uint32_t ulNewFileSize = RedRand32(&ulSeed) % (pTI->param.ulMaxFileSize + 1U);

                RedPrintf("%lu\tTRUNC\t%lu\t\t%s\n", (unsigned long)ulIter, (unsigned long)ulNewFileSize, pFile->pszPath);

                iErr = red_ftruncate(pFile->iFildes, ulNewFileSize);
                if((iErr != 0) && (red_errno == RED_ENOSPC))
                {
                    iErr = DoTransaction(pTI, pVol);
                    if(iErr != 0)
                    {
                        ret = -red_errno;
                        RedPrintf("red_transact(\"%s\") failed with errno %d\n", pVol->pszVolume, (int)-ret);
                        goto Cleanup;
                    }

                    iErr = red_ftruncate(pFile->iFildes, ulNewFileSize);

                    /*  After a transaction, a truncate which shrinks the file
                        size should succeed.
                    */
                    if((iErr != 0) && ((red_errno != RED_ENOSPC) || (ulNewFileSize < ulOldFileSize)))
                    {
                        ret = -red_errno;
                        RedPrintf("red_ftruncate() failed with errno %d\n", (int)-ret);
                        goto Cleanup;
                    }
                }

                if(ret == 0)
                {
                    if(ulNewFileSize > ulOldFileSize)
                    {
                        RedMemSet(&pFile->pbMirror[ulOldFileSize], 0U, ulNewFileSize - ulOldFileSize);
                    }

                    pFile->ulFileSize = ulNewFileSize;
                }
                else if(ret == -RED_ENOSPC)
                {
                    ret = 0;
                }
                else
                {
                    RedPrintf("red_ftruncate() failed with errno %d\n", (int)-ret);
                    goto Cleanup;
                }

                break;
            }
            case TESTOP_REMOUNT:
            {
                RedPrintf("%lu\tREMOUNT\t\t\t%s\n", (unsigned long)ulIter, pVol->pszVolume);

                /*  Reliance Edge needs its handles to be closed before it can
                    be unmounted.
                */
                for(i = 0U; i < pTI->param.ulFilesPerVol; i++)
                {
                    pFile = &pVol->pFiles[i];

                    (void)red_close(pFile->iFildes);
                    pFile->iFildes = -1;
                }

                iErr = red_umount(pVol->pszVolume);
                if(iErr != 0)
                {
                    ret = -red_errno;
                    RedPrintf("red_umount(\"%s\") failed with errno %d\n", pVol->pszVolume, (int)-ret);
                    goto Cleanup;
                }

                iErr = red_mount(pVol->pszVolume);
                if(iErr != 0)
                {
                    ret = -red_errno;
                    RedPrintf("red_mount(\"%s\") failed with errno %d\n", pVol->pszVolume, (int)-ret);
                    goto Cleanup;
                }

                /*  Reopen the files that we closed.
                */
                for(i = 0U; i < pTI->param.ulFilesPerVol; i++)
                {
                    pFile = &pVol->pFiles[i];
                    pFile->iFildes = red_open(pFile->pszPath, RED_O_RDWR);
                    if(pFile->iFildes < 0)
                    {
                        ret = -red_errno;
                        RedPrintf("red_open(\"%s\") failed with errno %d\n", pFile->pszPath, (int)-ret);
                        goto Cleanup;
                    }

                    /*  Revert to the transacted state.
                    */
                    pVol->pFiles[i].ulFileSize = pVol->pFilesTransacted[i].ulFileSize;
                    RedMemCpy(pVol->pFiles[i].pbMirror, pVol->pFilesTransacted[i].pbMirror, pVol->pFiles[i].ulFileSize);
                }

                break;
            }
            case TESTOP_TP:
            {
                RedPrintf("%lu\tTP\t\t\t%s\n", (unsigned long)ulIter, pVol->pszVolume);

                iErr = DoTransaction(pTI, pVol);
                if(iErr != 0)
                {
                    ret = -red_errno;
                    RedPrintf("red_transact(\"%s\") failed with errno %d\n", pVol->pszVolume, (int)-ret);
                    goto Cleanup;
                }

                break;
            }
            default:
            {
                RedPrintf("Reached unreachable code!\n");
                ret = -RED_EFUBAR;
                goto Out;
            }
        }
    }

  Cleanup:

    TestCleanup(pTI);

  Out:

    if(ret == 0)
    {
        RedPrintf(TEST_NAME " passed\n");
    }
    else
    {
        RedPrintf(TEST_NAME " FAILED with error %d\n", (int)ret);
    }

    return ret;
}


/** @brief Setup the test, allocating memory and opening/creating files.

    @param pTI  Test context information.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS TestSetup(
    MVSTRESSTESTINFO   *pTI)
{
    uint32_t            i;
    uint32_t            j;
    int32_t             iErr;
    REDSTATUS           ret = 0;

    iErr = red_init();
    if(iErr != 0)
    {
        ret = -red_errno;
        RedPrintf("red_init() failed with errno %d\n", (int)-ret);
        goto ErrorOut;
    }

    pTI->pbOpBuffer = malloc(pTI->param.ulMaxOpSize);
    if(pTI->pbOpBuffer == NULL)
    {
        goto MallocError;
    }

    pTI->pVolData = calloc(pTI->param.ulVolumeCount, sizeof(*pTI->pVolData));
    if(pTI->pVolData == NULL)
    {
        goto MallocError;
    }

    for(i = 0U; i < pTI->param.ulVolumeCount; i++)
    {
        MVSVOLINFO *pVol = &pTI->pVolData[i];

        pVol->bVolNum = pTI->param.abVolNum[i];
        pVol->pszVolume = gaRedVolConf[pVol->bVolNum].pszPathPrefix;

        /*  Volume might not be mounted, so don't check the error.
        */
        (void)red_umount(pVol->pszVolume);

        iErr = red_format(pVol->pszVolume);
        if(iErr != 0)
        {
            ret = -red_errno;
            RedPrintf("red_format(\"%s\") failed with errno %d\n",  pVol->pszVolume, (int)-ret);
            goto ErrorOut;
        }

        iErr = red_mount(pVol->pszVolume);
        if(iErr != 0)
        {
            ret = -red_errno;
            RedPrintf("red_mount(\"%s\") failed with errno %d\n",  pVol->pszVolume, (int)-ret);
            goto ErrorOut;
        }

        iErr = red_gettransmask(pVol->pszVolume, &pVol->ulOrigTransMask);
        if(iErr != 0)
        {
            ret = -red_errno;
            RedPrintf("red_gettransmask(\"%s\") failed with errno %d\n",  pVol->pszVolume, (int)-ret);
            goto ErrorOut;
        }

        iErr = red_settransmask(pVol->pszVolume, RED_TRANSACT_MANUAL);
        if(iErr != 0)
        {
            ret = -red_errno;
            RedPrintf("red_settransmask(\"%s\") failed with errno %d\n",  pVol->pszVolume, (int)-ret);
            goto ErrorOut;
        }

        pVol->pFiles = calloc(pTI->param.ulFilesPerVol, sizeof(*pVol->pFiles));
        if(pVol->pFiles == NULL)
        {
            goto MallocError;
        }

        for(j = 0U; j < pTI->param.ulFilesPerVol; j++)
        {
            MVSFILEINFO    *pFile = &pVol->pFiles[j];
            const char      szFilePrefix[] = "MVST_";
            char            szFileNumStr[16U];
            uint32_t        ulPathLen;

            pFile->pbMirror = calloc(1U, pTI->param.ulMaxFileSize);
            if(pFile->pbMirror == NULL)
            {
                goto MallocError;
            }

            (void)RedSNPrintf(szFileNumStr, sizeof(szFileNumStr), "%lu", (unsigned long)j);

            ulPathLen = RedStrLen(pVol->pszVolume);
            ulPathLen++; /* path separator */
            ulPathLen += RedStrLen(szFilePrefix) + RedStrLen(szFileNumStr);
            ulPathLen++; /* NUL terminator */

            pFile->pszPath = calloc(1U, ulPathLen);
            if(pFile->pszPath == NULL)
            {
                goto MallocError;
            }

            (void)RedSNPrintf(pFile->pszPath, ulPathLen, "%s/%s%s",
                pVol->pszVolume, szFilePrefix, szFileNumStr);

            pFile->iFildes = red_open(pFile->pszPath, RED_O_CREAT | RED_O_EXCL | RED_O_RDWR);
            if(pFile->iFildes < 0)
            {
                ret = -red_errno;
                RedPrintf("red_open(\"%s\") failed with errno %d\n", pFile->pszPath, (int)-ret);
                goto ErrorOut;
            }
        }

        pVol->pFilesTransacted = calloc(pTI->param.ulFilesPerVol, sizeof(*pVol->pFilesTransacted));
        if(pVol->pFilesTransacted == NULL)
        {
            goto MallocError;
        }

        for(j = 0U; j < pTI->param.ulFilesPerVol; j++)
        {
            pVol->pFilesTransacted[j] = pVol->pFiles[j];

            /*  The transacted file structure needs its own copy of the mirror
                buffer since the data may be different.
            */
            pVol->pFilesTransacted[j].pbMirror = calloc(1U, pTI->param.ulMaxFileSize);
            if(pVol->pFilesTransacted[j].pbMirror == NULL)
            {
                goto MallocError;
            }
        }

        /*  Transact so that the remount test case doesn't revert the creation
            of the test files.
        */
        iErr = DoTransaction(pTI, pVol);
        if(iErr != 0)
        {
            ret = -red_errno;
            RedPrintf("red_settransmask(\"%s\") failed with errno %d\n", pVol->pszVolume, (int)-ret);
            goto ErrorOut;
        }
    }

    return ret;

  MallocError:
    RedPrintf("Failed to allocate memory during test initialization\n");
    ret = -RED_ENOMEM;

  ErrorOut:
    TestCleanup(pTI);
    return ret;
}


/** @brief Cleanup the test, freeing memory and closing/unlining files.

    @param pTI  Test context information.
*/
static void TestCleanup(
    MVSTRESSTESTINFO *pTI)
{
    if(pTI->pVolData != NULL)
    {
        uint32_t i;
        uint32_t j;

        for(i = 0U; i < pTI->param.ulVolumeCount; i++)
        {
            if(pTI->pVolData[i].pFiles != NULL)
            {
                for(j = 0U; j < pTI->param.ulFilesPerVol; j++)
                {
                    MVSFILEINFO *pFile = &pTI->pVolData[i].pFiles[j];

                    if(pFile->pbMirror != NULL)
                    {
                        free(pFile->pbMirror);
                        pFile->pbMirror = NULL;
                    }

                    if((pFile->iFildes != -1) && (pFile->iFildes != 0))
                    {
                        (void)red_close(pFile->iFildes);
                        pFile->iFildes = -1;
                    }

                    if(pFile->pszPath != NULL)
                    {
                        /*  File might or mightn't exist, depending on whether
                            TestSetup() finished or not.
                        */
                        (void)red_unlink(pFile->pszPath);

                        free(pFile->pszPath);
                        pFile->pszPath = NULL;
                    }
                }

                free(pTI->pVolData[i].pFiles);
                pTI->pVolData[i].pFiles = NULL;
            }

            if(pTI->pVolData[i].pFilesTransacted != NULL)
            {
                for(j = 0U; j < pTI->param.ulFilesPerVol; j++)
                {
                    if(pTI->pVolData[i].pFilesTransacted[j].pbMirror != NULL)
                    {
                        free(pTI->pVolData[i].pFilesTransacted[j].pbMirror);
                        pTI->pVolData[i].pFilesTransacted[j].pbMirror = NULL;
                    }
                }

                free(pTI->pVolData[i].pFilesTransacted);
                pTI->pVolData[i].pFilesTransacted = NULL;
            }

            if(pTI->pVolData[i].ulOrigTransMask != 0U)
            {
                (void)red_settransmask(pTI->pVolData[i].pszVolume, pTI->pVolData[i].ulOrigTransMask);
            }
        }

        free(pTI->pVolData);
        pTI->pVolData = NULL;
    }

    if(pTI->pbOpBuffer != NULL)
    {
        free(pTI->pbOpBuffer);
        pTI->pbOpBuffer = NULL;
    }
}


/** @brief Echo the test parameters to the console.

    @param pTI  Test context information.
*/
static void TestEchoParameters(
    const MVSTRESSTESTINFO *pTI)
{
    uint32_t                i;

    RedPrintf("Test Parameters:\n");
    RedPrintf("    Volumes =\n");
    for(i = 0U; i < pTI->param.ulVolumeCount; i++)
    {
        RedPrintf("        %lu: \"%s\"\tVol#%u\n", (unsigned long)i,
            pTI->pVolData[i].pszVolume, (unsigned)pTI->pVolData[i].bVolNum);
    }
    RedPrintf("    Files Per Mount Point   = %lu\n", pTI->param.ulFilesPerVol);
    RedPrintf("    Max File Size           = %lu\n", pTI->param.ulMaxFileSize);
    RedPrintf("    Max Read/Write I/O Size = %lu\n", pTI->param.ulMaxOpSize);
    RedPrintf("    Test Iteration Count    = %lu\n", pTI->param.ulIterations);
    RedPrintf("    RNG Seed                = %lu\n", pTI->param.ulSeed);
}


static int DoTransaction(
    MVSTRESSTESTINFO   *pTI,
    MVSVOLINFO         *pVol)
{
    int32_t             iErr;

    iErr = red_transact(pVol->pszVolume);
    if(iErr == 0)
    {
        uint32_t i;

        for(i = 0U; i < pTI->param.ulFilesPerVol; i++)
        {
            pVol->pFilesTransacted[i].ulFileSize = pVol->pFiles[i].ulFileSize;
            RedMemCpy(pVol->pFilesTransacted[i].pbMirror, pVol->pFiles[i].pbMirror, pVol->pFiles[i].ulFileSize);
        }
    }

    return iErr;
}

#endif /* MVSTRESSTEST_SUPPORTED */

