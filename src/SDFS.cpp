/*
 SDFS.cpp - file system wrapper for SdFat
 Copyright (c) 2019 Earle F. Philhower, III. All rights reserved.

 Based on spiffs_api.cpp which is:
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
#include "SDFS.h"
#include "SDFSFormatter.h"
#include <FS.h>

//using namespace fs;


#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SDFS)
fs::FS SDFS = fs::FS(fs::FSImplPtr(new sdfs::SDFSImpl()));
#endif

namespace sdfs {


fs::FileImplPtr SDFSImpl::open(const char* path, OpenMode openMode, AccessMode accessMode)
{
    DEBUGV("SDFSImpl::open() path=[%s]\n", path); 
    if (!_mounted) {
        DEBUGV("SDFSImpl::open() called on unmounted FS\n");
        return fs::FileImplPtr();
    }
    if (!path || !path[0]) {
        DEBUGV("SDFSImpl::open() called with invalid filename\n");
        return fs::FileImplPtr();
    }
    int flags = _getFlags(openMode, accessMode);
    if ((openMode && OM_CREATE) && strchr(path, '/')) {
        DEBUGV("SDFSImpl::open() with OM_CREATE\n");
        // For file creation, silently make subdirs as needed.  If any fail,
        // it will be caught by the real file open later on
        char *pathStr = strdup(path);
        if (pathStr) {
            // Make dirs up to the final fnamepart
            char *ptr = strrchr(pathStr, '/');
            if (ptr && ptr != pathStr) { // Don't try to make root dir!
                *ptr = 0;
                _fs.mkdir(pathStr, true);
            }
        }
        free(pathStr);
    }
    DEBUGV("SDFSImpl::open() path=[%s] flags=%d\n", path, flags);
    ::File fd = _fs.open(path, flags);
    if (!fd) {
        DEBUGV("SDFSImpl::open() fail: fd=%p path=`%s` flags=%d openMode=%d accessMode=%d error=%d",
               &fd, path, flags, openMode, accessMode, _fs.sdErrorCode());
        return fs::FileImplPtr();
    }
    DEBUGV("SDFSImpl::open() ok\n");
    auto sharedFd = std::make_shared<::File>(fd);
    return std::make_shared<SDFSFileImpl>(this, sharedFd, path);
}

fs::FileImplPtr SDFSImpl::open(sdfs::SDFSDirImpl * dir, uint32_t dirIndex, OpenMode openMode, AccessMode accessMode)
{
    if (!_mounted) {
        DEBUGV("SDFSImpl::open() called on unmounted FS\n");
        return fs::FileImplPtr();
    }
    int flags = _getFlags(openMode, accessMode);
    ::File fd;
    fd.open(dir->_dir.get(), dirIndex, flags);
    if (!fd) {
        DEBUGV("SDFSImpl::open(dirIndex) fail: fd=%p dirIddex=%d flags=%d openMode=%d accessMode=%d error=%d",
               &fd, dirIndex, flags, openMode, accessMode, _fs.sdErrorCode());
        return fs::FileImplPtr();
    }
    auto sharedFd = std::make_shared<::File>(fd);
    return std::make_shared<SDFSFileImpl>(this, sharedFd, dir->fileName());
}

fs::DirImplPtr SDFSImpl::openDir(const char* path)
{
    DEBUGV("SDFSImpl::openDir() path=[%s]\n", path);
    if (!_mounted) {
        return fs::DirImplPtr();
    }
    char *pathStr = strdup(path); // Allow edits on our scratch copy
    if (!pathStr) {
        // OOM
        return fs::DirImplPtr();
    }
    // Get rid of any trailing slashes
    while (strlen(pathStr) && (pathStr[strlen(pathStr)-1]=='/')) {
        pathStr[strlen(pathStr)-1] = 0;
    }
    // At this point we have a name of "/blah/blah/blah" or "blah" or ""
    // If that references a directory, just open it and we're done.
    ::File dirFile;
    const char *filter = "";
    if (!pathStr[0]) {

        dirFile = _fs.open("/", O_RDONLY);
        filter = "";
    } else if (_fs.exists(pathStr)) {
        dirFile = _fs.open(pathStr, O_RDONLY);
        if (dirFile.isDir()) {
            // Easy peasy, path specifies an existing dir!
            filter = "";
        } else {
            dirFile.close();
            // This is a file, so open the containing dir
            char *ptr = strrchr(pathStr, '/');
            if (!ptr) {
                // No slashes, open the root dir
                dirFile = _fs.open("/", O_RDONLY);
                filter = pathStr;
            } else {
                // We've got slashes, open the dir one up
                *ptr = 0; // Remove slash, truncare string
                dirFile = _fs.open(pathStr, O_RDONLY);
                filter = ptr + 1;
            }
        }
    } else {
        // Name doesn't exist, so use the parent dir of whatever was sent in
        // This is a file, so open the containing dir
        char *ptr = strrchr(pathStr, '/');
        if (!ptr) {
            // No slashes, open the root dir
            dirFile = _fs.open("/", O_RDONLY);
            filter = pathStr;
        } else {
            // We've got slashes, open the dir one up
            *ptr = 0; // Remove slash, truncare string
            dirFile = _fs.open(pathStr, O_RDONLY);
            filter = ptr + 1;
        }
    }
    if (!dirFile) {
        DEBUGV("SDFSImpl::openDir failed: path=`%s`\n", path);
        return fs::DirImplPtr();
    }
    DEBUGV("SDFSImpl::openDir ok: path=`%s` filter='%s'\n", path, filter);
    auto sharedDir = std::make_shared<::File>(dirFile);
    auto ret = std::make_shared<SDFSDirImpl>(filter, this, sharedDir, pathStr);
    free(pathStr);
    return ret;
}

bool SDFSImpl::format() {
    if (_mounted) {
        return false;
    }
    SDFSFormatter formatter;
    bool ret = formatter.format(&_fs, _cfg._sdioConfig, _cfg._spiConfig);
    return ret;
}


}; // namespace sdfs

