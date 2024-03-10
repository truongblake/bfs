// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void* buf) {

  i32 inum = bfsFdToInum(fd);                       // converts bfsFDToInum
  i32 cursor_location = fsTell(fd);               // gets cursor locations 
  i32 fbnStart = cursor_location/BYTESPERBLOCK;               // gives us fbn (FBN 0 -> FBN 1)
  i32 fbnEnd = (cursor_location + numb)/BYTESPERBLOCK;        // gives us fbn Ending location 

  //printf("inum: %d, cursor_location: %d, fbnStart: %d, fbnEND: %d", inum,cursor_location,fbnStart,fbnEnd);
  
  //EOF check
  i32 number_of_bytes_read = numb;
  i32 fileSize = bfsGetSize(inum);

  if(cursor_location + numb > fileSize){ //if cursor + numb is more then file size then set number of byte to read to be exactly till end of fileSize
    number_of_bytes_read = fileSize - cursor_location;
    fbnEnd = (cursor_location + number_of_bytes_read)/BYTESPERBLOCK;
  }

  i8 tempbuf[BYTESPERBLOCK];
  i32 offset = 0;

  for(int fbn = fbnStart; fbn <= fbnEnd; fbn++){
    if(fbn == fbnEnd){
      bfsRead(inum,fbn,tempbuf);                //reads all dbn associated with file
      memcpy(buf + offset, tempbuf, numb % BYTESPERBLOCK); //copy the specified target data
    }else{
      bfsRead(inum,fbn,tempbuf);                //reads all dbn associated with file
      memcpy(buf + offset, tempbuf, BYTESPERBLOCK);        //read the full 512 byte block
    }
    offset += BYTESPERBLOCK;
  }
  fsSeek(fd, numb, SEEK_CUR);       // advance cursor after reading 
  return number_of_bytes_read;
}


// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {

  i32 inum = bfsFdToInum(fd);                           // converts bfsFDToInum
  i32 cursor_location = bfsTell(fd);                     // gets cursor locations 
  i32 cursor_buffer_location = 0;                       // buffer cursor location
  i32 fbnStart = cursor_location/BYTESPERBLOCK;         // gives us fbn (FBN 0 -> FBN 1)
  i32 fbnEnd = (cursor_location + numb)/BYTESPERBLOCK;  // gives us fbn Ending location 

  //extends if cursor + numb is > fileSize
  i32 fileSize = fsSize(fd);
  if(cursor_location + numb > fileSize){
    bfsExtend(inum, fbnEnd);
    bfsSetSize(inum, cursor_location + numb);
  }

  //temporary buf to store writing
  i8 tempbuf[BYTESPERBLOCK];
  i32 offset = cursor_location;

  for(int fbn = fbnStart; fbn <= fbnEnd; fbn++){ //cycles through FBN
    i32 dbn = bfsFbnToDbn(inum, fbn);
    bfsRead(inum, fbn, tempbuf);
    if(numb - cursor_buffer_location < BYTESPERBLOCK){
      memcpy(tempbuf + cursor_location % BYTESPERBLOCK, buf + cursor_buffer_location, numb - cursor_buffer_location);
      bioWrite(dbn, tempbuf);
      cursor_location += numb-cursor_buffer_location;
      cursor_buffer_location += numb-cursor_buffer_location;
    }else{
      memcpy(tempbuf + cursor_location % BYTESPERBLOCK, buf + cursor_buffer_location, BYTESPERBLOCK - cursor_location % BYTESPERBLOCK);
      bioWrite(dbn, tempbuf);
      cursor_location += BYTESPERBLOCK - cursor_location % BYTESPERBLOCK;
      cursor_buffer_location += BYTESPERBLOCK - cursor_location % BYTESPERBLOCK;
    }
    
  }
  //advance cursor
  fsSeek(fd, numb, SEEK_CUR);

  return 0;
}