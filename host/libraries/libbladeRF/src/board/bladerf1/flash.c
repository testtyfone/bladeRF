#include <stdio.h>
#include <string.h>

#include "log.h"
#include "minmax.h"
#include "conversions.h"

#include "bladeRF.h"
#include "board/board.h"

#include "driver/spi_flash.h"

#include "flash.h"

#define OTP_BUFFER_SIZE 256

int spi_flash_write_fx3_fw(struct bladerf *dev, const uint8_t *image, size_t len)
{
    int status;
    uint8_t *readback_buf;
    uint8_t *padded_image;
    uint32_t padded_image_len;

    /* Pad firwmare data out to a page size */
    const uint32_t page_size = BLADERF_FLASH_PAGE_SIZE;
    const uint32_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    if (len >= (UINT32_MAX - padding_len)) {
        return BLADERF_ERR_INVAL;
    }

    padded_image_len = (uint32_t) len + padding_len;

    readback_buf = malloc(padded_image_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_image = malloc(padded_image_len);
    if (padded_image == NULL) {
        free(readback_buf);
        return BLADERF_ERR_MEM;
    }

    /* Copy image */
    memcpy(padded_image, image, len);

    /* Clear the padded region */
    memset(padded_image + len, 0xFF, padded_image_len - len);

    /* Erase the entire firmware region */
    status = spi_flash_erase(dev, BLADERF_FLASH_EB_FIRMWARE,
                             BLADERF_FLASH_EB_LEN_FIRMWARE);
    if (status != 0) {
        log_debug("Failed to erase firmware region: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Convert the image length to pages */
    padded_image_len /= BLADERF_FLASH_PAGE_SIZE;

    /* Write the firmware image to flash */
    status = spi_flash_write(dev, padded_image,
                             BLADERF_FLASH_PAGE_FIRMWARE, padded_image_len);

    if (status < 0) {
        log_debug("Failed to write firmware: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and double-check what we just wrote */
    status = spi_flash_verify(dev, readback_buf, padded_image,
                              BLADERF_FLASH_PAGE_FIRMWARE, padded_image_len);
    if (status != 0) {
        log_debug("Flash verification failed: %s\n", bladerf_strerror(status));
        goto error;
    }

error:
    free(padded_image);
    free(readback_buf);
    return status;
}

static inline void fill_fpga_metadata_page(uint8_t *metadata,
                                           size_t actual_bitstream_len)
{
    char len_str[12];
    int idx = 0;

    memset(len_str, 0, sizeof(len_str));
    memset(metadata, 0xff, BLADERF_FLASH_PAGE_SIZE);

    snprintf(len_str, sizeof(len_str), "%u",
             (unsigned int)actual_bitstream_len);

    binkv_encode_field((char *)metadata, BLADERF_FLASH_PAGE_SIZE,
                       &idx, "LEN", len_str);
}

int spi_flash_write_fpga_bitstream(struct bladerf *dev,
                                   const uint8_t *bitstream, size_t len)
{
    int status;
    uint8_t *readback_buf;
    uint8_t *padded_bitstream;
    uint8_t metadata[BLADERF_FLASH_PAGE_SIZE];
    uint32_t padded_bitstream_len;

    /* Pad data to be page-aligned */
    const uint32_t page_size = BLADERF_FLASH_PAGE_SIZE;
    const uint32_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    if (len >= (UINT32_MAX - padding_len)) {
        return BLADERF_ERR_INVAL;
    }

    padded_bitstream_len = (uint32_t) len + padding_len;

    /* Fill in metadata with the *actual* FPGA bitstream length */
    fill_fpga_metadata_page(metadata, len);

    readback_buf = malloc(padded_bitstream_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_bitstream = malloc(padded_bitstream_len);
    if (padded_bitstream == NULL) {
        free(readback_buf);
        return BLADERF_ERR_MEM;
    }

    /* Copy bitstream */
    memcpy(padded_bitstream, bitstream, len);

    /* Clear the padded region */
    memset(padded_bitstream + len, 0xFF, padded_bitstream_len - len);

    /* Erase FPGA metadata and bitstream region */
    status = spi_flash_erase(dev, BLADERF_FLASH_EB_FPGA, BLADERF_FLASH_EB_LEN_FPGA);
    if (status != 0) {
        log_debug("Failed to erase FPGA meta & bitstream regions: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Write the metadata page */
    status = spi_flash_write(dev, metadata, BLADERF_FLASH_PAGE_FPGA, 1);
    if (status != 0) {
        log_debug("Failed to write FPGA metadata page: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Convert the padded bitstream length to pages */
    padded_bitstream_len /= BLADERF_FLASH_PAGE_SIZE;

    /* Write the padded bitstream */
    status = spi_flash_write(dev, padded_bitstream, BLADERF_FLASH_PAGE_FPGA + 1,
                             padded_bitstream_len);
    if (status != 0) {
        log_debug("Failed to write bitstream: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify metadata */
    status = spi_flash_verify(dev, readback_buf, metadata,
                              BLADERF_FLASH_PAGE_FPGA, 1);
    if (status != 0) {
        log_debug("Failed to verify metadata: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify the bitstream data */
    status = spi_flash_verify(dev, readback_buf, padded_bitstream,
                              BLADERF_FLASH_PAGE_FPGA + 1, padded_bitstream_len);
    if (status != 0) {
        log_debug("Failed to verify bitstream data: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

error:
    free(padded_bitstream);
    free(readback_buf);
    return status;
}

int spi_flash_erase_fpga(struct bladerf *dev)
{
    /* Erase the entire FPGA region, including both autoload metadata and the
     * actual bitstream data */
    return spi_flash_erase(dev, BLADERF_FLASH_EB_FPGA, BLADERF_FLASH_EB_LEN_FPGA);
}

int spi_flash_read_otp(struct bladerf *dev, char *field,
                       char *data, size_t data_size)
{
    int status;
    char otp[OTP_BUFFER_SIZE];

    memset(otp, 0xff, OTP_BUFFER_SIZE);

    status = dev->backend->get_otp(dev, otp);
    if (status < 0)
        return status;
    else
        return binkv_decode_field(otp, OTP_BUFFER_SIZE, field, data, data_size);
}

int spi_flash_read_cal(struct bladerf *dev, char *field,
                       char *data, size_t data_size)
{
    int status;
    char cal[CAL_BUFFER_SIZE];

    status = dev->backend->get_cal(dev, cal);
    if (status < 0)
        return status;
    else
        return binkv_decode_field(cal, CAL_BUFFER_SIZE, field, data, data_size);
}

int spi_flash_read_serial(struct bladerf *dev, char *serial_buf)
{
    int status;

    status = spi_flash_read_otp(dev, "S", serial_buf, BLADERF_SERIAL_LENGTH - 1);

    if (status < 0) {
        log_info("Unable to fetch serial number. Defaulting to 0's.\n");
        memset(dev->ident.serial, '0', BLADERF_SERIAL_LENGTH - 1);

        /* Treat this as non-fatal */
        status = 0;
    }

    serial_buf[BLADERF_SERIAL_LENGTH - 1] = '\0';

    return status;
}

int spi_flash_read_vctcxo_trim(struct bladerf *dev, uint16_t *dac_trim)
{
    int status;
    bool ok;
    int16_t trim;
    char tmp[7] = { 0 };

    status = spi_flash_read_cal(dev, "DAC", tmp, sizeof(tmp) - 1);
    if (status < 0) {
        return status;
    }

    trim = str2uint(tmp, 0, 0xffff, &ok);
    if (ok == false) {
        return BLADERF_ERR_INVAL;
    }

    *dac_trim = trim;

    return 0;
}

int spi_flash_read_fpga_size(struct bladerf *dev, bladerf_fpga_size *fpga_size)
{
    int status;
    char tmp[7] = { 0 };

    status = spi_flash_read_cal(dev, "B", tmp, sizeof(tmp) - 1);
    if (status < 0) {
        return status;
    }

    if (!strcmp("40", tmp)) {
        *fpga_size = BLADERF_FPGA_40KLE;
    } else if(!strcmp("115", tmp)) {
        *fpga_size = BLADERF_FPGA_115KLE;
    } else if(!strcmp("A4", tmp)) {
        *fpga_size = BLADERF_FPGA_A4;
    } else if(!strcmp("A9", tmp)) {
        *fpga_size = BLADERF_FPGA_A9;
    } else {
        *fpga_size = BLADERF_FPGA_UNKNOWN;
    }

    return status;
}

int spi_flash_read_flash_id(struct bladerf *dev, uint8_t *mid, uint8_t *did)
{
    int status;

    status = dev->backend->get_flash_id(dev, mid, did);

    return status;
}

int spi_flash_decode_flash_architecture(struct bladerf *dev,
                                        bladerf_fpga_size  *fpga_size)
{
    int status;
    struct bladerf_flash_arch *flash_arch;

    status     = 0;
    flash_arch = dev->flash_arch;

    /* Fill in defaults */
    flash_arch->tsize_bytes  = 32 << 17; /* 32 Mbit */
    flash_arch->psize_bytes  = 256;
    flash_arch->ebsize_bytes = 64 << 10; /* 64 Kbyte */
    flash_arch->status       = STATUS_ASSUMED;

    /* First try to decode the MID/DID of the flash chip */
    switch( flash_arch->manufacturer_id ) {
        case 0xC2: /* MACRONIX */
            log_verbose( "Found SPI flash manufacturer: MACRONIX.\n" );
            switch( flash_arch->device_id ) {
                case 0x36:
                    log_verbose( "Found SPI flash device: MX25U3235E (32 Mbit).\n" );
                    flash_arch->tsize_bytes = 32 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                default:
                    log_debug( "Unknown Macronix flash device ID.\n" );
                    status = BLADERF_ERR_UNEXPECTED;
            }
            break;

        case 0xEF: /* WINBOND */
            log_verbose( "Found SPI flash manufacturer: WINBOND.\n" );
            switch( flash_arch->device_id ) {
                case 0x15:
                    log_verbose( "Found SPI flash device: W25Q32JV (32 Mbit).\n" );
                    flash_arch->tsize_bytes = 32 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                case 0x17:
                    log_verbose( "Found SPI flash device: W25Q128JV (128 Mbit).\n" );
                    flash_arch->tsize_bytes = 128 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                default:
                    log_debug( "Unknown Winbond flash device ID.\n" );
                    status = BLADERF_ERR_UNEXPECTED;
            }
            break;

        default:
            log_debug( "Unknown flash manufacturer ID.\n" );
            status = BLADERF_ERR_UNEXPECTED;
    }

    /* Could not decode flash MID/DID, so assume based on FPGA size */
    if( status < 0 || flash_arch->status != STATUS_SUCCESS ) {
        if( (fpga_size == NULL) || (*fpga_size == BLADERF_FPGA_UNKNOWN) ) {
            log_debug( "Could not decode flash manufacturer/device ID and have "
                       "an unknown FPGA size. Assume default flash "
                       "architecture.\n" );
        } else {
            switch( *fpga_size ) {
                case BLADERF_FPGA_A9:
                    flash_arch->tsize_bytes = 128 << 17;
                    break;
                default:
                    flash_arch->tsize_bytes = 32 << 17;
            }
            log_debug( "Could not decode flash manufacturer/device ID, but "
                       "found a %u kLE FPGA. Setting the most probable "
                       "flash architecture.\n", *fpga_size );
        }
    }

    flash_arch->num_pages = flash_arch->tsize_bytes / flash_arch->psize_bytes;
    flash_arch->num_ebs   = flash_arch->tsize_bytes / flash_arch->ebsize_bytes;

    log_verbose("SPI flash total size = %u Mbit\n", (flash_arch->tsize_bytes >> 17));
    log_verbose("SPI flash page size = %u bytes\n", flash_arch->psize_bytes);
    log_verbose("SPI flash erase block size = %u bytes\n", flash_arch->ebsize_bytes);
    log_verbose("SPI flash number of pages = %u\n", flash_arch->num_pages);
    log_verbose("SPI flash number of erase blocks = %u pages\n", flash_arch->num_ebs);

    return status;
}


/******
 * CRC16 implementation from http://softwaremonkey.org/Code/CRC16
 */
typedef  unsigned char                   byte;    /*     8 bit unsigned       */
typedef  unsigned short int              word;    /*    16 bit unsigned       */

static word crc16mp(word crcval, void *data_p, word count) {
    /* CRC-16 Routine for processing multiple part data blocks.
     * Pass 0 into 'crcval' for first call for any given block; for
     * subsequent calls pass the CRC returned by the previous call. */
    word            xx;
    byte            *ptr= (byte *)data_p;

    while (count-- > 0) {
        crcval=(word)(crcval^(word)(((word)*ptr++)<<8));
        for (xx=0;xx<8;xx++) {
            if(crcval&0x8000) { crcval=(word)((word)(crcval<<1)^0x1021); }
            else              { crcval=(word)(crcval<<1);                }
        }
    }
    return(crcval);
}

int binkv_decode_field(char *ptr, int len, char *field,
                       char *val, size_t  maxlen)
{
    int c;
    unsigned char *ub, *end;
    unsigned short a1, a2;
    size_t flen, wlen;

    flen = strlen(field);

    ub = (unsigned char *)ptr;
    end = ub + len;
    while (ub < end) {
        c = *ub;

        if (c == 0xff) // flash and OTP are 0xff if they've never been written to
            break;

        a1 = LE16_TO_HOST(*(unsigned short *)(&ub[c+1]));  // read checksum
        a2 = crc16mp(0, ub, c+1);  // calculated checksum

        if (a1 == a2) {
            if (!strncmp((char *)ub + 1, field, flen)) {
                wlen = min_sz(c - flen, maxlen);
                strncpy(val, (char *)ub + 1 + flen, wlen);
                val[wlen] = 0;
                return 0;
            }
        } else {
            log_debug( "%s: Field checksum mismatch\n", __FUNCTION__);
            return BLADERF_ERR_INVAL;
        }
        ub += c + 3; //skip past `c' bytes, 2 byte CRC field, and 1 byte len field
    }
    return BLADERF_ERR_INVAL;
}

int binkv_encode_field(char *ptr, int len, int *idx,
                       const char *field, const char *val)
{
    int vlen, flen, tlen;
    flen = (int)strlen(field);
    vlen = (int)strlen(val);
    tlen = flen + vlen + 1;

    if (tlen >= 256 || *idx + tlen >= len)
        return BLADERF_ERR_MEM;

    ptr[*idx] = flen + vlen;
    strcpy(&ptr[*idx + 1], field);
    strcpy(&ptr[*idx + 1 + flen], val);
    *(unsigned short *)(&ptr[*idx + tlen ]) = HOST_TO_LE16(crc16mp(0, &ptr[*idx ], tlen));
    *idx += tlen + 2;
    return 0;
}

int binkv_add_field(char *buf, int buf_len, const char *field_name, const char *val)
{
    int dummy_idx = 0;
    int i = 0;
    int rv;

    /* skip to the end, ignoring crc (don't want to further corrupt partially
     * corrupt data) */
    while(i < buf_len) {
        uint8_t field_len = buf[i];

        if(field_len == 0xff)
            break;

        /* skip past `field_len' bytes, 2 byte CRC field, and 1 byte len
         * field */
        i += field_len + 3;
    }

    rv = binkv_encode_field(buf + i, buf_len - i, &dummy_idx, field_name, val);
    if(rv < 0)
        return rv;

    return 0;
}

