/*
 SDFSFormatter.cpp - Formatter for SdFat SD cards
 Copyright (c) 2019 Earle F. Philhower, III. All rights reserved.

 A C++ implementation of the SdFat/examples/SdFormatter sketch:
 | Copyright (c) 2011-2018 Bill Greiman

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

#ifndef _SDFSFORMATTER_H
#define _SDFSFORMATTER_H

#include "SDFS.h"
#include <FS.h>

namespace sdfs {

class SDFSFormatter {
private:
    // Taken from main FS object
    SdCard *card;
    cache_t *cache;

    uint32_t cardSizeSectors;
    uint32_t cardCapacityMB;

public:
    bool format(SdFat *_fs, SdioConfig * sdio, SdSpiConfig * spi) {
        ExFatFormatter exFatFormatter;
        FatFormatter fatFormatter;
        SdCardFactory cardFactory;
        uint8_t  sectorBuffer[512];

        card  = _fs->card();
        cache = _fs->cacheClear();

        if (sdio) {
           if (!( card = cardFactory.newCard(*sdio)) || card->errorCode()) {
               return false;
            }
        } else if (spi) {
           if (!( card = cardFactory.newCard(*spi)) || card->errorCode()) {
               return false;
            }

        }
        cardSizeSectors = card->sectorCount();
        if (cardSizeSectors == 0) {
            return false;
        }

        cardCapacityMB = (cardSizeSectors)*512LL/ 1048576;

        // Format exFAT if larger than 32GB.
        bool rtn = cardSizeSectors > 67108864 ?
        exFatFormatter.format(card, sectorBuffer, &Serial) :
        fatFormatter.format(card, sectorBuffer, &Serial);
	return rtn;
    }
  #define ERASE_SIZE 262144L
  bool erase(SdFat *_fs, SdioConfig * sdio, SdSpiConfig * spi) {
      uint32_t firstBlock = 0;
      uint32_t lastBlock;
      uint16_t n = 0;
      uint8_t  sectorBuffer[512];
      SdCardFactory cardFactory;
      //card  = _fs->card();
      cache = _fs->cacheClear();

        if (sdio) {
           if (!( card = cardFactory.newCard(*sdio)) || card->errorCode()) {
               return false;
            }
        } else if (spi) {
           if (!( card = cardFactory.newCard(*spi)) || card->errorCode()) {
               return false;
            }

        }
      cardSizeSectors = card->sectorCount();

      do {
        lastBlock = firstBlock + ERASE_SIZE - 1;
        if (lastBlock >= cardSizeSectors) {
          lastBlock = cardSizeSectors - 1;
        }
        if (!card->erase(firstBlock, lastBlock)) {
          return false;
        }
        if ((n++)%64 == 63) {
           yield();
        }
        firstBlock += ERASE_SIZE;
      } while (firstBlock < cardSizeSectors);

      if (!card->readSector(0, sectorBuffer)) {
         return false;
      }
      return true;
    }
}; // class SDFSFormatter

}; // namespace sdfs


#endif // _SDFSFORMATTER_H
