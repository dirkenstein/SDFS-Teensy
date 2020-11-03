#ifndef SDFS_H
#define SDFS_H

/*
 SDFS.h - file system wrapper for SdLib
 Copyright (c) 2019 Earle F. Philhower, III.  All rights reserved.

 Based on spiffs_api.h, which is:
 | Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

 This code was influenced by NodeMCU and Sming libraries, and first version of
 Arduino wrapper written by Hristo Gochkov.

 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <limits>
#include <assert.h>
#include "FS.h"
#include "FSImpl.h"
#include <SPI.h>
#include <SdFat.h>
#include <sdios.h>
#include <FS.h>
#define DEBUG
#include <esp_debug.h>
#include <TimeLib.h>

//using namespace fs;

namespace sdfs {

class SDFSFileImpl;
class SDFSDirImpl;

class SDFSConfig : public fs::FSConfig
{
public:
    SDFSConfig() {
        _type = SDFSConfig::fsid::FSId;
        _autoFormat = false;
        _csPin = 10;
	_part = 0;
        _maxSpeed = SPI_FULL_SPEED;
        _spiConfig = new SdSpiConfig(_csPin, _mode, _maxSpeed); 
    }
    SDFSConfig(uint8_t csPin, uint32_t speed) {
        _type = SDFSConfig::fsid::FSId;
        _autoFormat = false;
        _csPin = csPin;
        _maxSpeed = speed;
	_part = 0;
        _spiConfig = new SdSpiConfig(_csPin, _mode, _maxSpeed); 
    }
    SDFSConfig(SdSpiConfig * cfg) {
        _type = SDFSConfig::fsid::FSId;
        _spiConfig = cfg;
    }
    SDFSConfig(SdioConfig * cfg) {
        _type = SDFSConfig::fsid::FSId;
        _sdioConfig = cfg;
    }
    enum fsid { FSId = 0x53444653 };

    SDFSConfig setAutoFormat(bool val = true) {
        _autoFormat = val;
        return *this;
    }
    SDFSConfig setCSPin(uint8_t pin) {
        _csPin = pin;
        delete _spiConfig;
        _spiConfig = new SdSpiConfig(_csPin, _mode, _maxSpeed); 
        return *this;
    }
    SDFSConfig setSPI(uint32_t speed) {
        _maxSpeed = speed;
        delete _spiConfig;
        _spiConfig = new SdSpiConfig(_csPin, _mode, _maxSpeed); 
        return *this;
    }
    SDFSConfig setPart(uint8_t part) {
        _part = part;
        return *this;
    }
    
    // Inherit _type and _autoFormat
    uint8_t     _csPin;
    uint8_t     _part;
    uint32_t _maxSpeed;
    uint8_t _mode = SHARED_SPI;
    SdSpiConfig  *  _spiConfig = NULL;
    SdioConfig  * _sdioConfig = NULL;
};

class SDFSImpl : public fs::FSImpl
{
public:
    SDFSImpl() : _mounted(false)
    {
    }

    fs::FileImplPtr open(const char* path, OpenMode openMode, AccessMode accessMode) override;
    fs::FileImplPtr SDFSImpl::open(SDFSDirImpl *  dir, uint32_t dirIndex, OpenMode openMode, AccessMode accessMode);

    bool exists(const char* path) override {
        return _mounted ? _fs.exists(path) : false;
    }

    fs::DirImplPtr openDir(const char* path) override;

    bool rename(const char* pathFrom, const char* pathTo) override {
        return _mounted ? _fs.rename(pathFrom, pathTo) : false;
    }

    bool info64(fs::FSInfo64& info) override {
        if (!_mounted) {
            DEBUGV("SDFS::info: FS not mounted\n");
            return false;
        }
        info.maxOpenFiles = 999; // TODO - not valid
        info.blockSize = _fs.sectorsPerCluster() * 512;
        info.pageSize = 0; // TODO ?
        info.maxPathLength = 255; // TODO ?
        info.totalBytes =_fs.clusterCount() * _fs.sectorsPerCluster() *512LL;
        info.usedBytes = info.totalBytes - (_fs.freeClusterCount() * _fs.sectorsPerCluster() * 512LL);
        return true;
    }

    bool info(fs::FSInfo& info) override {
        fs::FSInfo64 i;
        if (!info64(i)) {
            return false;
        }
        info.blockSize     = i.blockSize;
        info.pageSize      = i.pageSize;
        info.maxOpenFiles  = i.maxOpenFiles;
        info.maxPathLength = i.maxPathLength;
#ifdef DEBUG_ESP_PORT
        if (i.totalBytes > (uint64_t)SIZE_MAX) {
            // This catches both total and used cases, since used must always be < total.
            DEBUG_ESP_PORT.printf_P(PSTR("WARNING: SD card size overflow (%lld>= 4GB).  Please update source to use info64().\n"), i.totalBytes);
        }
#endif
        info.totalBytes    = (size_t)i.totalBytes;
        info.usedBytes     = (size_t)i.usedBytes;
        return true;
    }

    bool remove(const char* path) override {
        return _mounted ? _fs.remove(path) : false;
    }

    bool mkdir(const char* path) override {
        return _mounted ? _fs.mkdir(path) : false;
    }

    bool rmdir(const char* path) override {
        return _mounted ?_fs.rmdir(path) : false;
    }

    bool setConfig(const fs::FSConfig &cfg) override
    {
        if ((cfg._type != SDFSConfig::fsid::FSId) || _mounted) {
            DEBUGV("SDFS::setConfig: invalid config or already mounted\n");
            return false;
        }
	_cfg = *static_cast<const SDFSConfig *>(&cfg);
        return true;
    }

    bool begin() override {
        if (_mounted) {
            end();
        }
        if(_cfg._sdioConfig) {
            _mounted = _fs.begin(*_cfg._sdioConfig);
            if (!_mounted && _cfg._autoFormat) {
                format();
                _mounted = _fs.begin(*_cfg._sdioConfig);
            }
        } else if (_cfg._spiConfig) {
            _mounted = _fs.begin(*_cfg._spiConfig);
            if (!_mounted && _cfg._autoFormat) {
                format();
                _mounted = _fs.begin(*_cfg._spiConfig);
            }
        }
	FsDateTime::setCallback(dateTimeCB);
        return _mounted;
    }

    void end() override {
        _mounted = false;
        // TODO
    }

    bool format() override;

    // The following are not common FS interfaces, but are needed only to
    // support the older SD.h exports
    uint8_t type() {
        return _fs.card()->type();
    }
    uint8_t fatType() {
        return _fs.vol()->fatType();
    }
    size_t blocksPerCluster() {
        return _fs.sectorsPerCluster();
    }
    size_t totalClusters() {
        return _fs.clusterCount();
    }
    size_t totalBlocks() {
        return (totalClusters() / blocksPerCluster());
    }
    size_t clusterSize() {
        return blocksPerCluster() * 512; // 512b block size
    }
    size_t size() {
        return (clusterSize() * totalClusters());
    }

    // Helper function, takes FAT and makes standard time_t
    static time_t FatToTimeT(uint16_t d, uint16_t t) {
        TimeElements te;
        memset(&te, 0, sizeof(TimeElements));
        te.Second  = (((int)t) <<  1) & 0x3e;
        te.Minute  = (((int)t) >>  5) & 0x3f;
        te.Hour = (((int)t) >> 11) & 0x1f;
        te.Day = (int)(d & 0x1f);
        te.Month  = ((int)(d >> 5) & 0x0f) - 1;
        te.Year = ((int)(d >> 9) & 0x7f) + 10;
        return makeTime(te);
    }
    static time_t FatToTimeT(uint8_t * d, uint8_t * t) {
        TimeElements te;
        memset(&te, 0, sizeof(TimeElements));
        uint16_t dd, td;
        dd = d[1] << 8 | d[0];
        td = t[1] << 8 | t[0];
        te.Second  = (((int)td) <<  1) & 0x3e;
        te.Minute  = (((int)td) >>  5) & 0x3f;
        te.Hour = (((int)td) >> 11) & 0x1f;
        te.Day = (int)(dd & 0x1f);
        te.Month  = ((int)(dd >> 5) & 0x0f) - 1;
        te.Year = ((int)(dd >> 9) & 0x7f) + 10;
        return makeTime(te);
     }

    // Because SdFat has a single, global setting for this we can only use a
    // static member of our class to return the time/date.  However, since
    // this is static, we can't see the time callback variable.  Punt for now,
    // using time(NULL) as the best we can do.
    //static void dateTimeCB(uint16_t *dosYear, uint16_t *dosTime) {
    //    time_t now = time(nullptr);
    //    *dosYear = ((year(t) - 1980) << 9) | ((month(t) + 1) << 5) | day(t);
    //    *dosTime = (hour(t) << 11) | (min(t) << 5) | sec(t);
    //}
    // Call back for file timestamps.  Only called for file create and sync().
   static void dateTimeCB(uint16_t* date, uint16_t* time, uint8_t* ms10) {
      time_t t = now(); 
      // Return date using FS_DATE macro to format fields.
      *date = FS_DATE(year(t), month(t), day(t));

      // Return time using FS_TIME macro to format fields.
      *time = FS_TIME(hour(t), minute(t), second(t));
  
      // Return low time bits in units of 10 ms.
      *ms10 = second(t) & 1 ? 100 : 0;
    }
    bool sync(fs::FileMap &openFiles)
    {
     fs::FileMap::iterator itr;
     for (itr = openFiles.begin(); itr != openFiles.end(); ++itr) {
	fs::File* filp = itr->second;
        Serial.printf("flushing %s\n", filp->fullName());
        filp->flush();
      }
      return true;
    }


protected:
    friend class SDFileImpl;
    friend class SDFSDirImpl;

    SdFat* getFs()
    {
        return &_fs;
    }

    static oflag_t _getFlags(OpenMode openMode, AccessMode accessMode) {
        oflag_t mode = 0;
        if (openMode & OM_CREATE) {
            mode |= O_CREAT;
        }
        if (openMode & OM_APPEND) {
            mode |= O_AT_END;
        }
        if (openMode & OM_TRUNCATE) {
            mode |= O_TRUNC;
        }
        if (accessMode & AM_READ) {
            mode |= O_READ;
        }
        if (accessMode & AM_WRITE) {
            mode |= O_WRITE;
        }
        return mode;
    }

    SdFat _fs;
    SDFSConfig   _cfg;
    bool         _mounted;
};


class SDFSFileImpl : public fs::FileImpl
{
public:
    SDFSFileImpl(SDFSImpl *fs, std::shared_ptr<::File> fd, const char *name)
        : _fs(fs), _fd(fd), _opened(true)
    {
        _name = std::shared_ptr<char>(new char[strlen(name) + 1], std::default_delete<char[]>());
        strcpy(_name.get(), name);
    }

    ~SDFSFileImpl() override
    {
        flush();
        close();
    }

    size_t write(const uint8_t *buf, size_t size) override
    {
        return _opened ? _fd->write(buf, size) : -1;
    }

    size_t read(uint8_t* buf, size_t size) override
    {
        DEBUGV("SDFSFileImpl::read open=%d\n", _opened);
        return _opened ? _fd->read(buf, size) : -1;
    }

    void flush() override
    {
        if (_opened) {
            _fd->flush();
            _fd->sync();
        }
    }

    bool seek(uint32_t pos, fs::SeekMode mode) override
    {
        if (!_opened) {
            return false;
        }
        switch (mode) {
            case fs::SeekSet:
                return _fd->seekSet(pos);
            case fs::SeekEnd:
                return _fd->seekEnd(-pos); // TODO again, odd from POSIX
            case fs::SeekCur:
                return _fd->seekCur(pos);
            default:
                // Should not be hit, we've got an invalid seek mode
                DEBUGV("SDFSFileImpl::seek: invalid seek mode %d\n", mode);
		assert((mode==fs::SeekSet) || (mode==fs::SeekEnd) || (mode==fs::SeekCur)); // Will fail and give meaningful assert message
		return false;
        }
    }

    size_t position() const override
    {
        return _opened ? _fd->curPosition() : 0;
    }

    size_t size() const override
    {
        return _opened ? _fd->fileSize() : 0;
    }

    bool truncate(uint32_t size) override
    {
        if (!_opened) {
            DEBUGV("SDFSFileImpl::truncate: file not opened\n");
            return false;
        }
        return _fd->truncate(size);
    }

    void close() override
    {
        if (_opened) {
            _fd->close();
            _opened = false;
        }
    }

    const char* name() const override
    {
        if (!_opened) {
            DEBUGV("SDFSFileImpl::name: file not opened\n");
            return nullptr;
        } else {
            const char *p = _name.get();
            const char *slash = strrchr(p, '/');
            // For names w/o any path elements, return directly
            // If there are slashes, return name after the last slash
            // (note that strrchr will return the address of the slash,
            // so need to increment to ckip it)
            return (slash && slash[1]) ? slash + 1 : p;
        }
    }

    const char* fullName() const override
    {
        DEBUGV("fullName() opened %d [%s]\n", _opened, _name.get());
        return _opened ? _name.get() : nullptr;
    }

    bool isFile() const override
    {
        return _opened ? _fd->isFile() : false;;
    }

    bool isDirectory() const override
    {
        return _opened ? _fd->isDirectory() : false;
    }

    time_t getLastWrite() override {
        time_t ftime = 0;
        if (_opened && _fd) {
            DirFat_t tmp;
            if (_fd.get()->dirEntry(&tmp)) {
                ftime = SDFSImpl::FatToTimeT(tmp.modifyDate, tmp.modifyTime);
            }
        }
        return ftime;
    }

    time_t getCreationTime() override {
        time_t ftime = 0;
        if (_opened && _fd) {
            DirFat_t tmp;
            if (_fd.get()->dirEntry(&tmp)) {
                ftime = SDFSImpl::FatToTimeT(tmp.createDate, tmp.createTime);
            }
        }
        return ftime;
    }



protected:
    SDFSImpl*                     _fs;
    std::shared_ptr<::File>  _fd;
    std::shared_ptr<char>         _name;
    bool                          _opened;
};

class SDFSDirImpl : public fs::DirImpl
{
public:
    SDFSDirImpl(const String& pattern, SDFSImpl* fs, std::shared_ptr<::File> dir, const char *dirPath = nullptr)
        : _pattern(pattern), _fs(fs), _dir(dir), _valid(false), _dirPath(nullptr), _dirIndex(-1)
    {
        if (dirPath) {
            _dirPath = std::shared_ptr<char>(new char[strlen(dirPath) + 1], std::default_delete<char[]>());
            strcpy(_dirPath.get(), dirPath);
        }
    }

    ~SDFSDirImpl() override
    {
        _dir->close();
    }

    fs::FileImplPtr openFile(OpenMode openMode, AccessMode accessMode) override
    {
        if (!_valid) {
            return fs::FileImplPtr();
        }
        // MAX_PATH on FAT32 is potentially 260 bytes per most implementations
        char tmpName[260];
        snprintf(tmpName, sizeof(tmpName), "%s%s%s", _dirPath.get() ? _dirPath.get() : "", _dirPath.get()&&_dirPath.get()[0]?"/":"", _lfn);
        DEBUGV("openFile() dirPath=[%s] lfn=[%s]\n", _dirPath.get(), _lfn, tmpName);
        //return _fs->open((const char *)tmpName, openMode, accessMode);
        if(_dirIndex != -1) {
           return _fs->open(this, _dirIndex, openMode, accessMode);
        } else {
           return _fs->open((const char *)tmpName, openMode, accessMode);
        }

    }

    const char* fileName() override
    {
        if (!_valid) {
            DEBUGV("SDFSDirImpl::fileName: directory not valid\n");
            return nullptr;
        }
        return (const char*) _lfn; //_dirent.name;
    }

    size_t fileSize() override
    {
        if (!_valid) {
            return 0;
        }

        return _size;
    }

    time_t fileTime() override
    {
        if (!_valid) {
            return 0;
        }

        return _time;
    }

    time_t fileCreationTime() override
    {
        if (!_valid) {
            return 0;
        }

        return _creation;
    }

    bool isFile() const override
    {
        return _valid ? _isFile : false;
    }

    bool isDirectory() const override
    {
        return _valid ? _isDirectory : false;
    }

    bool next() override
    {
        const int n = _pattern.length();
        DEBUGV("next() pattern=[%s] len %d\n", _pattern.c_str(), n);
        do {
            ::File file;
             char dname[64];
             _dir.get()->getName(dname, 64);
             DEBUGV("next() dirname = [%s]", dname);         
            file.openNext(_dir.get(), O_READ);
            if (file) {
                _valid = 1;
                _size = file.fileSize();
                _isFile = file.isFile();
                _isDirectory = file.isDirectory();
                _isHidden = file.isHidden();
                _dirIndex = file.dirIndex();
                DirFat_t tmp;
                if (file.dirEntry(&tmp)) {
                    _time = SDFSImpl::FatToTimeT(tmp.modifyDate, tmp.modifyTime);
                    _creation = SDFSImpl::FatToTimeT(tmp.createDate, tmp.createTime);
		} else {
                    _time = 0;
                    _creation = 0;
               }
               if(file.isLFN()) {
                file.getName(_lfn, sizeof(_lfn));
               } else {
		 file.getSFN(_lfn);
               }
                DEBUGV("next() Sze=%d dir=%d isLFN=%d hidden=%d LFN=[%s] SFN=[%s]\n", _size, _isDirectory, file.isLFN(),_isHidden,_lfn,tmp.name);
                file.close();
            } else {
                DEBUGV("next() file not open\n");
                _valid = 0;
            }
        } while(_valid && strncmp((const char*) _lfn, _pattern.c_str(), n) != 0);
        DEBUGV("_next done vlaid=%d\n",_valid);
        return _valid;
    }

    bool rewind() override
    {
        _valid = false;
        _dir->rewind();
        return true;
    }
protected:
    friend class SDFSImpl;
    String                       _pattern;
    SDFSImpl*                    _fs;
    std::shared_ptr<::File>      _dir;
    bool                         _valid;
    char                         _lfn[64];
    time_t                       _time;
    time_t                       _creation;
    std::shared_ptr<char>        _dirPath;
    uint32_t                     _size;
    bool                         _isFile;
    bool                         _isDirectory;
    bool                         _isHidden;
    int                          _dirIndex;
};

}; // namespace sdfs

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SDFS)
extern fs::FS SDFS;
using sdfs::SDFSConfig;
#endif

#endif // SDFS.h
