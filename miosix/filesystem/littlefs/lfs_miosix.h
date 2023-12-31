/***************************************************************************
 *   Copyright (C) 2023 by Andrea Somaini                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef LFS_MIOSIX_H
#define LFS_MIOSIX_H

#include "bd/lfs_rambd.h"
#include "config/miosix_settings.h"
#include "filesystem/file.h"
#include "filesystem/stringpart.h"
#include "kernel/sync.h"
#include "lfs.h"
#include <memory>

namespace miosix {

#ifdef WITH_FILESYSTEM

struct lfs_driver_context {

public:
  lfs_driver_context(FileBase *disk) : disk(disk), mutex(Mutex::DEFAULT) {}

  FileBase *disk;
  Mutex mutex;
};

/**
 * LittleFS Filesystem.
 */
class LittleFS : public FilesystemBase {
public:
  /**
   * Constructor
   */
  LittleFS(intrusive_ref_ptr<FileBase> disk);

  /**
   * Open a file
   * \param file the file object will be stored here, if the call succeeds
   * \param name the name of the file to open, relative to the local
   * filesystem
   * \param flags file flags (open for reading, writing, ...)
   * \param mode file permissions
   * \return 0 on success, or a negative number on failure
   */
  virtual int open(intrusive_ref_ptr<FileBase> &file, StringPart &name,
                   int flags, int mode);

  /**
   * Obtain information on a file, identified by a path name. Does not follow
   * symlinks
   * \param name path name, relative to the local filesystem
   * \param pstat file information is stored here
   * \return 0 on success, or a negative number on failure
   */
  virtual int lstat(StringPart &name, struct stat *pstat);

  /**
   * Remove a file or directory
   * \param name path name of file or directory to remove
   * \return 0 on success, or a negative number on failure
   */
  virtual int unlink(StringPart &name);

  /**
   * Rename a file or directory
   * \param oldName old file name
   * \param newName new file name
   * \return 0 on success, or a negative number on failure
   */
  virtual int rename(StringPart &oldName, StringPart &newName);

  /**
   * Create a directory
   * \param name directory name
   * \param mode directory permissions
   * \return 0 on success, or a negative number on failure
   */
  virtual int mkdir(StringPart &name, int mode);

  /**
   * Remove a directory if empty
   * \param name directory name
   * \return 0 on success, or a negative number on failure
   */
  virtual int rmdir(StringPart &name);

  /**
   * \return true if the filesystem failed to mount
   */
  bool mountFailed() const { return mountError != 0; }

  /**
   * Destructor
   */
  ~LittleFS();

  lfs_t *getLfs() { return &lfs; }

private:
  /**
   * Specialization of LittleFS::open for directories only
   * \param directory the file object will be stored here, if the call succeeds
   * \param name the name of the directory to open, relative to the local
   * filesystem
   * \param flags file flags (open for reading, writing, ...)
   * \param mode file permissions
   * \return 0 on success, or a negative number on failure
   */
  int openDirectory(intrusive_ref_ptr<FileBase> &directory, StringPart &name,
                     int flags, int mode);
  /**
   * Specialization of LittleFS::open for files only
   * \param file the file object will be stored here, if the call succeeds
   * \param name the name of the file to open, relative to the local
   * filesystem
   * \param flags file flags (open for reading, writing, ...)
   * \param mode file permissions
   * \return 0 on success, or a negative number on failure
   */
  int openFile(intrusive_ref_ptr<FileBase> &file, StringPart &name, int flags,
                int mode);

  miosix::intrusive_ref_ptr<miosix::FileBase> drv; /* drive device */
  int mountError;                                  ///< Mount error code

  struct lfs_config config;
  lfs_rambd_t rambd;

  lfs_t lfs;
  lfs_file_t file;

  lfs_driver_context context;
};

class LittleFSFile : public FileBase {
public:
  /**
   * @brief Constructs a LittleFSFile object.
   *
   * @param parentFS A reference to the parent LittleFS object.
   * @param file A pointer to the underlying lfs_file_t object. This object
   * takes ownership of the pointer.
   * @param forceSync A boolean indicating whether to force synchronization
   * after writes
   */
  LittleFSFile(intrusive_ref_ptr<LittleFS> parentFS,
               std::unique_ptr<lfs_file_t> file, bool forceSync,
               StringPart &name)
      : FileBase(parentFS), file(std::move(file)), forceSync(forceSync),
        name(name) {}

  virtual int read(void *buf, size_t count) override;
  virtual int write(const void *buf, size_t count) override;
  virtual off_t lseek(off_t pos, int whence) override;
  virtual int fstat(struct stat *pstat) const override;

  ~LittleFSFile() {
    LittleFS *lfs_driver = static_cast<LittleFS *>(getParent().get());
    lfs_file_close(lfs_driver->getLfs(), file.get());
  }

private:
  std::unique_ptr<lfs_file_t> file;

  /// Force the file to be synced on every write
  bool forceSync;
  StringPart name;
};

class LittleFSDirectory : public DirectoryBase {
public:
  LittleFSDirectory(intrusive_ref_ptr<LittleFS> parentFS,
                    std::unique_ptr<lfs_dir_t> dir)
      : DirectoryBase(parentFS), dir(std::move(dir)) {}

  /**
   * Also directories can be opened as files. In this case, this system call
   * allows to retrieve directory entries.
   * \param dp pointer to a memory buffer where one or more struct dirent
   * will be placed. dp must be four words aligned.
   * \param len memory buffer size.
   * \return the number of bytes read on success, or a negative number on
   * failure.
   */
  virtual int getdents(void *dp, int len);

  ~LittleFSDirectory() {
    LittleFS *lfs_driver = static_cast<LittleFS *>(getParent().get());
    lfs_dir_close(lfs_driver->getLfs(), dir.get());
  };

private:
  std::unique_ptr<lfs_dir_t> dir;
  // Used for when `getdents` does not give enough memory to store the whole
  // directory childrens
  bool directoryTraversalUnfinished = false;

  // Where to place a children info
  lfs_info dirInfo;
};

/**
 * Convert the error flag given by LittleFS into a POSIX error code
 * \param lfs_err the error flag given by LittleFS
 * \return the respective POSIX error code
 */
int lfsErrorToPosix(int lfs_err);

/**
 * Convert a POSIX open flag into a LittleFS open flag
 * \param posix_flags POSIX open flag
 * \return LittleFS open flag
 */
int posixOpenToLfsFlags(int posix_flags);

// * Wrappers for LFS block device operations
int miosixBlockDeviceRead(const struct lfs_config *c, lfs_block_t block,
                             lfs_off_t off, void *buffer, lfs_size_t size);

int miosixBlockDeviceProg(const struct lfs_config *c, lfs_block_t block,
                             lfs_off_t off, const void *buffer,
                             lfs_size_t size);

int miosixBlockDeviceErase(const struct lfs_config *c, lfs_block_t block);

int miosixBlockDeviceSync(const struct lfs_config *c);

// Lock the underlying block device. Negative error codes
// are propagated to the user.
int miosixLfsLock(const struct lfs_config *c);

// Unlock the underlying block device. Negative error codes
// are propagated to the user.
int miosixLfsUnlock(const struct lfs_config *c);

#endif // WITH_FILESYSTEM

} // namespace miosix

#endif // LFS_MIOSIX_H
