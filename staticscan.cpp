// copyright (c) 2017-2019 hors<horsicq@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "staticscan.h"

StaticScan::StaticScan(QObject *parent) : QObject(parent)
{
    bIsStop=false;
    pOptions=nullptr;
    pScanResult=nullptr;
    currentStats=STATS();
    pElapsedTimer=nullptr;
}

void StaticScan::setData(QString sFileName, SpecAbstract::SCAN_OPTIONS *pOptions,SpecAbstract::SCAN_RESULT *pScanResult)
{
    this->sFileName=sFileName;
    this->pOptions=pOptions;
    this->pScanResult=pScanResult;
}

void StaticScan::setData(QString sFileName, SpecAbstract::SCAN_OPTIONS *pOptions)
{
    this->sFileName=sFileName;
    this->pOptions=pOptions;
}

void StaticScan::process()
{
    pElapsedTimer=new QElapsedTimer;
    pElapsedTimer->start();

    currentStats.nTotal=0;
    currentStats.nCurrent=0;

    bIsStop=false;

    if(sFileName!="")
    {
        if(pScanResult)
        {
            currentStats.sStatus=tr("Scan");

            *pScanResult=scanFile(sFileName);

            emit scanResult(*pScanResult);
        }
        else
        {
            currentStats.sStatus=tr("Searching");
            QList<QString> listFiles;
            findFiles(sFileName,&listFiles);
            currentStats.nTotal=listFiles.count();

            for(int i=0;(i<currentStats.nTotal)&&(!bIsStop);i++)
            {
                currentStats.nCurrent=i+1;
                currentStats.sStatus=listFiles.at(i);
                SpecAbstract::SCAN_RESULT _scanResult=scanFile(currentStats.sStatus);

                emit scanResult(_scanResult);
            }
        }
    }

    emit completed(pElapsedTimer->elapsed());
    delete pElapsedTimer;
    pElapsedTimer=nullptr;

    bIsStop=false;
}

void StaticScan::findFiles(QString sFileName,QList<QString> *pListFileNames)
{
    currentStats.nTotal=pListFileNames->count();

    if(!bIsStop)
    {
        QFileInfo fi(sFileName);

        if(fi.isFile())
        {
            pListFileNames->append(fi.absoluteFilePath());
        }
        else if(fi.isDir())
        {
            QDir dir(sFileName);

            QFileInfoList eil=dir.entryInfoList();

            for(int i=0;(i<eil.count())&&(!bIsStop);i++)
            {
                QString sFN=eil.at(i).fileName();
                if((sFN!=".")&&(sFN!=".."))
                {
                    findFiles(eil.at(i).absoluteFilePath(),pListFileNames);
                }
            }
        }
    }
}

void StaticScan::stop()
{
    bIsStop=true;
}

SpecAbstract::SCAN_RESULT StaticScan::processFile(QString sFileName, SpecAbstract::SCAN_OPTIONS *pOptions)
{
    SpecAbstract::SCAN_RESULT scanResult;
    StaticScan scan;
    scan.setData(sFileName,pOptions,&scanResult);
    scan.process();

    return scanResult;
}

QString StaticScan::getEngineVersion()
{
    return SSE_VERSION;
}

StaticScan::STATS StaticScan::getCurrentStats()
{
    if(pElapsedTimer)
    {
        currentStats.nElapsed=pElapsedTimer->elapsed();
    }

    return currentStats;
}

void StaticScan::_process(QIODevice *pDevice,SpecAbstract::SCAN_RESULT *pScanResult,qint64 nOffset,qint64 nSize,SpecAbstract::ID parentId,SpecAbstract::SCAN_OPTIONS *pOptions,int nLevel)
{
    QElapsedTimer scanTimer;
    scanTimer.start();

    if(QString(pDevice->metaObject()->className())=="QFile")
    {
        pScanResult->sFileName=((QFile *)pDevice)->fileName();
    }

    SubDevice sd(pDevice,nOffset,nSize);

    if(sd.open(QIODevice::ReadOnly))
    {
        QSet<QBinary::FT> stTypes=QBinary::getFileTypes(&sd);

        if(stTypes.contains(QBinary::FT_PE32)||stTypes.contains(QBinary::FT_PE64))
        {
            SpecAbstract::PEINFO_STRUCT pe_info=SpecAbstract::getPEInfo(&sd,parentId,pOptions);

            pScanResult->listRecords.append(pe_info.basic_info.listDetects);

            if(pOptions->bScanOverlay)
            {
                if(pe_info.nOverlaySize)
                {
                    SpecAbstract::ID _parentId=pe_info.basic_info.id;
                    _parentId.filepart=SpecAbstract::RECORD_FILEPART_OVERLAY;
                    _process(pDevice,pScanResult,pe_info.nOverlayOffset,pe_info.nOverlaySize,_parentId,pOptions,nLevel++);
                }
            }
        }
        else if(stTypes.contains(QBinary::FT_ELF32)||stTypes.contains(QBinary::FT_ELF64))
        {
            SpecAbstract::ELFINFO_STRUCT elf_info=SpecAbstract::getELFInfo(&sd,parentId,pOptions);

            pScanResult->listRecords.append(elf_info.basic_info.listDetects);
        }
        else if(stTypes.contains(QBinary::FT_MSDOS))
        {
            SpecAbstract::MSDOSINFO_STRUCT msdos_info=SpecAbstract::getMSDOSInfo(&sd,parentId,pOptions);

            pScanResult->listRecords.append(msdos_info.basic_info.listDetects);
        }
        else
        {
            SpecAbstract::BINARYINFO_STRUCT binary_info=SpecAbstract::getBinaryInfo(&sd,parentId,pOptions);

            pScanResult->listRecords.append(binary_info.basic_info.listDetects);
        }

        sd.close();
    }

    if(nLevel==0)
    {
        pScanResult->nScanTime=scanTimer.elapsed();
    }
}

SpecAbstract::SCAN_RESULT StaticScan::scanFile(QString sFileName)
{
    SpecAbstract::SCAN_RESULT result={0};

    QFile file;
    file.setFileName(sFileName);

    if(file.open(QIODevice::ReadOnly))
    {
        SpecAbstract::ID parentId;
        parentId.filetype=SpecAbstract::RECORD_FILETYPE_UNKNOWN;
        parentId.filepart=SpecAbstract::RECORD_FILEPART_HEADER;
        _process(&file,&result,0,file.size(),parentId,pOptions);

        file.close();
    }

    return result;
}
