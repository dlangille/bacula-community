/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2022 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 *
 *  Dumb program to extract files from a Bacula backup.
 *
 *   Kern E. Sibbald, MM
 *
 */

#include "bacula.h"
#include "stored.h"
#include "ch.h"
#include "findlib/find.h"

#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#endif

extern bool parse_sd_config(CONFIG *config, const char *configfile, int exit_code);

static void do_extract(char *fname);
static bool record_cb(DCR *dcr, DEV_RECORD *rec);

static DEVICE *dev = NULL;
static DCR *dcr;
static BFILE bfd;
static JCR *jcr;
static FF_PKT *ff;
static BSR *bsr = NULL;
static bool extract = false;
static int non_support_data = 0;
static long total = 0;
static ATTR *attr;
static POOLMEM *curr_fname;
static char *where;
static uint64_t num_errors = 0;
static uint64_t num_records = 0;
static uint32_t num_files = 0;
static uint32_t compress_buf_size = 70000;
static POOLMEM *compress_buf;
static int prog_name_msg = 0;
static int win32_data_msg = 0;
static char *VolumeName = NULL;

static char *wbuf;                    /* write buffer address */
static uint32_t wsize;                /* write size */
static uint64_t fileAddr = 0;         /* file write address */

static CONFIG *config;
#define CONFIG_FILE "bacula-sd.conf"

char *configfile = NULL;
bool skip_extract = false;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\n%sVersion: %s (%s)\n\n"
"Usage: bextract <options> <bacula-archive-device-name> <directory-to-store-files>\n"
"       -b <file>       specify a bootstrap file\n"
"       -c <file>       specify a Storage configuration file\n"
"       -d <nn>         set debug level to <nn>\n"
"       -dt             print timestamp in debug output\n"
"       -T              send debug traces to trace file (stored in /tmp)\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -p              proceed inspite of I/O errors\n"
"       -t              read data from volume, do not write anything\n"
"       -v              verbose\n"
"       -V <volumes>    specify Volume names (separated by |)\n"
"       -?              print this message\n\n"), 2000, BDEMO, VERSION, BDATE);
   exit(1);
}


int main (int argc, char *argv[])
{
   int ch;
   FILE *fd;
   char line[1000];
   bool got_inc = false;
   BtoolsAskDirHandler askdir_handler;

   init_askdir_handler(&askdir_handler);
   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   lmgr_init_thread();

   working_directory = "/tmp";
   my_name_is(argc, argv, "bextract");
   init_msg(NULL, NULL);              /* setup message handler */

   OSDependentInit();

   ff = init_find_files();
   binit(&bfd);

   while ((ch = getopt(argc, argv, "Ttb:c:d:e:i:pvV:?")) != -1) {
      switch (ch) {
      case 't':
         skip_extract = true;
         break;

      case 'b':                    /* bootstrap file */
         bsr = parse_bsr(NULL, optarg);
         break;

      case 'T':                 /* Send debug to trace file */
         set_trace(1);
         break;

      case 'c':                    /* specify config file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':                    /* debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            char *p;
            /* We probably find a tag list -d 10,sql,bvfs */
            if ((p = strchr(optarg, ',')) != NULL) {
               *p = 0;
            }
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
            if (p) {
               debug_parse_tags(p+1, &debug_level_tags);
            }
         }
         break;

      case 'e':                    /* exclude list */
         if ((fd = bfopen(optarg, "rb")) == NULL) {
            berrno be;
            Pmsg2(0, _("Could not open exclude file: %s, ERR=%s\n"),
               optarg, be.bstrerror());
            exit(1);
         }
         while (fgets(line, sizeof(line), fd) != NULL) {
            strip_trailing_junk(line);
            Dmsg1(900, "add_exclude %s\n", line);
            add_fname_to_exclude_list(ff, line);
         }
         fclose(fd);
         break;

      case 'i':                    /* include list */
         if ((fd = bfopen(optarg, "rb")) == NULL) {
            berrno be;
            Pmsg2(0, _("Could not open include file: %s, ERR=%s\n"),
               optarg, be.bstrerror());
            exit(1);
         }
         while (fgets(line, sizeof(line), fd) != NULL) {
            strip_trailing_junk(line);
            Dmsg1(900, "add_include %s\n", line);
            add_fname_to_include_list(ff, 0, line);
         }
         fclose(fd);
         got_inc = true;
         break;

      case 'p':
         forge_on = true;
         break;

      case 'v':
         verbose++;
         break;

      case 'V':                    /* Volume name */
         VolumeName = optarg;
         break;

      case '?':
      default:
         usage();

      } /* end switch */
   } /* end while */
   argc -= optind;
   argv += optind;

   if (argc != 2) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   config = New(CONFIG());
   parse_sd_config(config, configfile, M_ERROR_TERM);
   setup_me();
   load_sd_plugins(me->plugin_directory);

   if (!got_inc) {                            /* If no include file, */
      add_fname_to_include_list(ff, 0, "/");  /*   include everything */
   }

   where = argv[1];
   do_extract(argv[0]);

   if (bsr) {
      free_bsr(bsr);
   }
   if (prog_name_msg) {
      Pmsg1(000, _("%d Program Name and/or Program Data Stream records ignored.\n"),
         prog_name_msg);
   }
   if (win32_data_msg) {
      Pmsg1(000, _("%d Win32 data or Win32 gzip data stream records. Ignored.\n"),
         win32_data_msg);
   }
   term_include_exclude_files(ff);
   term_find_files(ff);
   return 0;
}

static void do_extract(char *devname)
{
   char ed1[50];
   struct stat statp;

   enable_backup_privileges(NULL, 1);

   jcr = setup_jcr("bextract", devname, bsr, VolumeName, SD_READ, true/*read dedup data*/, 10/*retry count*/);
   if (!jcr) {
      exit(1);
   }
   dev = jcr->read_dcr->dev;
   if (!dev) {
      exit(1);
   }
   dcr = jcr->read_dcr;

   /* Make sure where directory exists and that it is a directory */
   if (stat(where, &statp) < 0) {
      berrno be;
      Emsg2(M_ERROR_TERM, 0, _("Cannot stat %s. It must exist. ERR=%s\n"),
         where, be.bstrerror());
   }
   if (!S_ISDIR(statp.st_mode)) {
      Emsg1(M_ERROR_TERM, 0, _("%s must be a directory.\n"), where);
   }

   free(jcr->where);
   jcr->where = bstrdup(where);
   attr = new_attr(jcr);

   compress_buf = get_memory(compress_buf_size);
   curr_fname = get_pool_memory(PM_FNAME);
   *curr_fname = 0;

   if (jcr->dedup &&  skip_extract) {
      /* We want to force the checksum verification for blocks that come from DDE */
      jcr->dedup->set_checksum_after_rehydration(true);
   }

   read_records(dcr, record_cb, mount_next_read_volume);
   /* If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file.
    */
   if (is_bopen(&bfd)) {
      set_attributes(jcr, attr, &bfd);
   }
   dev->free_dedup_rehydration_interface(dcr); // disconnect from the DDE
   release_device(dcr);
   free_attr(attr);
   free_jcr(jcr);
   dev->term(NULL);
   free_pool_memory(curr_fname);

   printf(_("%u files restored.\n"), num_files);
   if (num_errors) {
      printf(_("Found %s error%s\n"), edit_uint64(num_errors, ed1), num_errors>1? "s":"");
   }
   return;
}

static bool store_data(int32_t stream, BFILE *bfd, char *data, const int32_t length)
{
   bool skip_win32 = false;
   /* This is a workaround of VSS Plugin backups that were incorrectly
    * generating WIN32 streams with the offset option
    */
   if (attr->data_stream == STREAM_WIN32_DATA && stream & STREAM_BIT_OFFSETS) {
      set_portable_backup(bfd);
      skip_win32 = true;        /* handle data as regular files */
      Dmsg0(50, "Found incorrect stream for WIN32 data\n");
   }

   if (is_win32_stream(attr->data_stream) && !have_win32_api() && !skip_win32) {
      set_portable_backup(bfd);
      if (!processWin32BackupAPIBlock(bfd, data, length)) {
         berrno be;
         Emsg2(M_ERROR_TERM, 0, _("Write error on %s: %s\n"),
               attr->ofname, be.bstrerror());
         return false;
      }
   } else if (bwrite(bfd, data, length) != (ssize_t)length) {
      berrno be;
      Emsg2(M_ERROR_TERM, 0, _("Write error on %s: %s\n"),
            attr->ofname, be.bstrerror());
      return false;
   }

   return true;
}

/*
 * Called here for each record from read_records()
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   int stat, ret=true;
   JCR *jcr = dcr->jcr;
   char ed1[50];

   bool     restoredatap = false;
   POOLMEM *orgdata = NULL;
   uint32_t orgdata_len = 0;

   if (rec->FileIndex < 0) {
      return true;                    /* we don't want labels */
   }

   /* Do rehydration */
   if (rec->Stream & STREAM_BIT_DEDUPLICATION_DATA) {
      if (!jcr->read_dcr->dev->setup_dedup_rehydration_interface(jcr->read_dcr)) {
         Jmsg0(jcr, M_FATAL, 0, _("Cannot do rehydration, device is not dedup aware\n"));
         return false;
      }
      int size;
      bool despite_of_error = forge_on;
      int err = jcr->dedup->record_rehydration(dcr, rec, jcr->dedup->get_msgbuf(), jcr->errmsg, despite_of_error, &size);
      if (err) {
         Jmsg(jcr, M_ERROR, 0, _("Got rehydration error at file=%d record=%s fname=%s volsessionid=%d volsessiontime=%d Msg=%s"),
               num_files, edit_uint64(num_records, ed1), curr_fname, rec->VolSessionId, rec->VolSessionTime, jcr->errmsg);
         if (err == -1) {
            /* cannot read data from DDE */
            Jmsg(jcr, M_FATAL, 0, "%s", jcr->errmsg);
            return false;
         } else {
            jcr->JobStatus = JS_Running; /* We want to continue */
            num_errors++;
         }
      }

      /* Keep old pointers and restore them when at the end,
       * after this point, we need to use goto bail_out
       */
      orgdata = rec->data;
      orgdata_len = rec->data_len;
      restoredatap = true;

      rec->data = jcr->dedup->get_msgbuf();
      rec->data_len = size;
   }

   /* In this mode, we do not create any file on disk, just read
    * everything from the volume.
    */
   if (skip_extract) {
      switch (rec->maskedStream) {
      case STREAM_UNIX_ATTRIBUTES:
      case STREAM_UNIX_ATTRIBUTES_EX:
         if (!unpack_attributes_record(jcr, rec->Stream, rec->data, rec->data_len, attr)) {
            Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
         }
         if (verbose) {
            attr->data_stream = decode_stat(attr->attr, &attr->statp, sizeof(attr->statp), &attr->LinkFI);
            build_attr_output_fnames(jcr, attr);
            print_ls_output(jcr, attr);
         }
         pm_strcpy(curr_fname, attr->fname);
         num_files++;
         break;
      }
      num_records++;

      /* We display some progress information if verbose not set or set to 2 */
      if (verbose != 1 && (num_records % 200000) == 0L) {
         fprintf(stderr, "\rfiles=%d records=%s\n", num_files, edit_uint64(num_records, ed1));
      }
      ret = true;
      goto bail_out;
   }

   /* File Attributes stream */

   switch (rec->maskedStream) {
   case STREAM_UNIX_ATTRIBUTES:
   case STREAM_UNIX_ATTRIBUTES_EX:
   case STREAM_UNIX_ATTRIBUTE_UPDATE:

      /* If extracting, it was from previous stream, so
       * close the output file.
       */
      if (extract) {
         if (!is_bopen(&bfd)) {
            Emsg0(M_ERROR, 0, _("Logic error output file should be open but is not.\n"));
         }
         set_attributes(jcr, attr, &bfd);
         extract = false;
      }

      if (!unpack_attributes_record(jcr, rec->Stream, rec->data, rec->data_len, attr)) {
         Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }

      /* Keep the name of the current file if we find a bad block */
      pm_strcpy(curr_fname, attr->fname);

      if (file_is_included(ff, attr->fname) && !file_is_excluded(ff, attr->fname)) {
         attr->data_stream = decode_stat(attr->attr, &attr->statp, sizeof(attr->statp), &attr->LinkFI);
         if (!is_restore_stream_supported(attr->data_stream)) {
            if (!non_support_data++) {
               Jmsg(jcr, M_ERROR, 0, _("%s stream not supported on this Client.\n"),
                  stream_to_ascii(attr->data_stream));
            }
            extract = false;
            goto bail_out;
         }

         build_attr_output_fnames(jcr, attr);

         if (attr->type == FT_DELETED) { /* TODO: choose the right fname/ofname */
            Jmsg(jcr, M_INFO, 0, _("%s was deleted.\n"), attr->fname);
            extract = false;
            goto bail_out;
         }

         extract = false;
         stat = create_file(jcr, attr, &bfd, REPLACE_ALWAYS);

         switch (stat) {
         case CF_ERROR:
         case CF_SKIP:
            break;
         case CF_EXTRACT:
            extract = true;
            print_ls_output(jcr, attr);
            num_files++;
            fileAddr = 0;
            break;
         case CF_CREATED:
            set_attributes(jcr, attr, &bfd);
            print_ls_output(jcr, attr);
            num_files++;
            fileAddr = 0;
            break;
         }
      }
      break;

   case STREAM_RESTORE_OBJECT:
   case STREAM_PLUGIN_OBJECT:
   case STREAM_PLUGIN_META_BLOB:
   case STREAM_PLUGIN_META_CATALOG:
      /* nothing to do */
      break;

   /* Data stream and extracting */
   case STREAM_FILE_DATA:
   case STREAM_SPARSE_DATA:
   case STREAM_WIN32_DATA:
      if (extract) {
         if (is_offset_stream(rec->Stream)) {
            ser_declare;
            uint64_t faddr;
            wbuf = rec->data + OFFSET_FADDR_SIZE;
            wsize = rec->data_len - OFFSET_FADDR_SIZE;
            ser_begin(rec->data, OFFSET_FADDR_SIZE);
            unser_uint64(faddr);
            if (fileAddr != faddr) {
               fileAddr = faddr;
               if (blseek(&bfd, (boffset_t)fileAddr, SEEK_SET) < 0) {
                  berrno be;
                  Emsg3(M_ERROR_TERM, 0, _("Seek error Addr=%llu on %s: %s\n"),
                     fileAddr, attr->ofname, be.bstrerror());
               }
            }
         } else {
            wbuf = rec->data;
            wsize = rec->data_len;
         }
         total += wsize;
         Dmsg2(8, "Write %u bytes, total=%u\n", wsize, total);
         store_data(rec->Stream, &bfd, wbuf, wsize);
         fileAddr += wsize;
      }
      break;

   /* GZIP data stream */
   case STREAM_GZIP_DATA:
   case STREAM_SPARSE_GZIP_DATA:
   case STREAM_WIN32_GZIP_DATA:
#ifdef HAVE_LIBZ
      if (extract) {
         uLong compress_len = compress_buf_size;
         int stat = Z_BUF_ERROR;

         if (is_offset_stream(rec->Stream)) {
            ser_declare;
            uint64_t faddr;
            char ec1[50];
            wbuf = rec->data + OFFSET_FADDR_SIZE;
            wsize = rec->data_len - OFFSET_FADDR_SIZE;
            ser_begin(rec->data, OFFSET_FADDR_SIZE);
            unser_uint64(faddr);
            if ((rec->Stream & STREAM_BIT_OFFSETS) == 0 && fileAddr != faddr) {
               fileAddr = faddr;
               if (blseek(&bfd, (boffset_t)fileAddr, SEEK_SET) < 0) {
                  berrno be;
                  Emsg3(M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
                     edit_uint64(fileAddr, ec1), attr->ofname, be.bstrerror());
                  extract = false;
                  goto bail_out;
               }
            }
         } else {
            wbuf = rec->data;
            wsize = rec->data_len;
         }

         while (compress_len < 10000000 && (stat=uncompress((Byte *)compress_buf, &compress_len,
                                 (const Byte *)wbuf, (uLong)wsize)) == Z_BUF_ERROR) {
            /* The buffer size is too small, try with a bigger one */
            compress_len = 2 * compress_len;
            compress_buf = check_pool_memory_size(compress_buf,
                                                  compress_len);
         }
         if (stat != Z_OK) {
            Emsg1(M_ERROR, 0, _("Uncompression error. ERR=%d\n"), stat);
            extract = false;
            goto bail_out;
         }

         Dmsg2(100, "Write uncompressed %d bytes, total before write=%d\n", compress_len, total);
         store_data(rec->Stream, &bfd, compress_buf, compress_len);
         total += compress_len;
         fileAddr += compress_len;
         Dmsg2(100, "Compress len=%d uncompressed=%d\n", rec->data_len,
            compress_len);
      }
#else
      if (extract) {
         Emsg0(M_ERROR, 0, _("GZIP data stream found, but GZIP not configured!\n"));
         extract = false;
         goto bail_out;
      }
#endif
      break;

   /* Compressed data stream */
   case STREAM_COMPRESSED_DATA:
   case STREAM_SPARSE_COMPRESSED_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
      if (extract) {
         uint32_t comp_magic, comp_len;
         uint16_t comp_level, comp_version;
#ifdef HAVE_LZO
         lzo_uint compress_len;
         const unsigned char *cbuf;
         int r, real_compress_len;
#endif

         if (is_offset_stream(rec->Stream)) {
            ser_declare;
            uint64_t faddr;
            char ec1[50];
            wbuf = rec->data + OFFSET_FADDR_SIZE;
            wsize = rec->data_len - OFFSET_FADDR_SIZE;
            ser_begin(rec->data, OFFSET_FADDR_SIZE);
            unser_uint64(faddr);
            if ((rec->Stream & STREAM_BIT_OFFSETS) == 0 && fileAddr != faddr) {
               fileAddr = faddr;
               if (blseek(&bfd, (boffset_t)fileAddr, SEEK_SET) < 0) {
                  berrno be;
                  Emsg3(M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
                     edit_uint64(fileAddr, ec1), attr->ofname, be.bstrerror());
                  extract = false;
                  goto bail_out;
               }
            }
         } else {
            wbuf = rec->data;
            wsize = rec->data_len;
         }

         /* read compress header */
         unser_declare;
         unser_begin(wbuf, sizeof(comp_stream_header));
         unser_uint32(comp_magic);
         unser_uint32(comp_len);
         unser_uint16(comp_level);
         unser_uint16(comp_version);
         Dmsg4(200, "Compressed data stream found: magic=0x%x, len=%d, level=%d, ver=0x%x\n", comp_magic, comp_len,
                                 comp_level, comp_version);

         /* version check */
         if (comp_version != COMP_HEAD_VERSION) {
            Emsg1(M_ERROR, 0, _("Compressed header version error. version=0x%x\n"), comp_version);
            ret = false;
            goto bail_out;
         }
         /* size check */
         if (comp_len + sizeof(comp_stream_header) != wsize) {
            Emsg2(M_ERROR, 0, _("Compressed header size error. comp_len=%d, msglen=%d\n"),
                 comp_len, wsize);
            ret = false;
            goto bail_out;
         }

          switch(comp_magic) {
#ifdef HAVE_LZO
            case COMPRESS_LZO1X:
               compress_len = compress_buf_size;
               cbuf = (const unsigned char*) wbuf + sizeof(comp_stream_header);
               real_compress_len = wsize - sizeof(comp_stream_header);
               Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, wsize);
               while ((r=lzo1x_decompress_safe(cbuf, real_compress_len,
                                               (unsigned char *)compress_buf, &compress_len, NULL)) == LZO_E_OUTPUT_OVERRUN)
               {

                  /* The buffer size is too small, try with a bigger one */
                  compress_len = 2 * compress_len;
                  compress_buf = check_pool_memory_size(compress_buf,
                                                  compress_len);
               }
               if (r != LZO_E_OK) {
                  Emsg1(M_ERROR, 0, _("LZO uncompression error. ERR=%d\n"), r);
                  extract = false;
                  goto bail_out;
               }
               Dmsg2(100, "Write uncompressed %d bytes, total before write=%d\n", compress_len, total);
               store_data(rec->Stream, &bfd, compress_buf, compress_len);
               total += compress_len;
               fileAddr += compress_len;
               Dmsg2(100, "Compress len=%d uncompressed=%d\n", rec->data_len, compress_len);
               break;
#endif
            default:
               Emsg1(M_ERROR, 0, _("Compression algorithm 0x%x found, but not supported!\n"), comp_magic);
               extract = false;
               goto bail_out;
         }

      }
      break;

   case STREAM_MD5_DIGEST:
   case STREAM_SHA1_DIGEST:
   case STREAM_SHA256_DIGEST:
   case STREAM_SHA512_DIGEST:
      break;

   case STREAM_SIGNED_DIGEST:
   case STREAM_ENCRYPTED_SESSION_DATA:
      // TODO landonf: Investigate crypto support in the storage daemon
      break;

   case STREAM_PROGRAM_NAMES:
   case STREAM_PROGRAM_DATA:
      if (!prog_name_msg) {
         Pmsg0(000, _("Got Program Name or Data Stream. Ignored.\n"));
         prog_name_msg++;
      }
      break;
   case STREAM_PLUGIN_NAME:
      /* Just ignore the plugin name */
      break;
   default:
      /* If extracting, weird stream (not 1 or 2), close output file anyway */
      if (extract) {
         if (!is_bopen(&bfd)) {
            Emsg0(M_ERROR, 0, _("Logic error output file should be open but is not.\n"));
         }
         set_attributes(jcr, attr, &bfd);
         extract = false;
      }
      Jmsg(jcr, M_ERROR, 0, _("Unknown stream=%d ignored. This shouldn't happen!\n"),
         rec->Stream);
      break;

   } /* end switch */
bail_out:
   if (restoredatap) {
      rec->data = orgdata;
      rec->data_len = orgdata_len;
   }
   return ret;
}
