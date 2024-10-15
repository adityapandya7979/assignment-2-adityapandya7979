#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

int is_mounted=0; //Global flag to check the status of jbod whether mounted or unmounted
jbod_error_t jbod_error;

int mdadm_mount(void) {
  // Complete your code here
  if(is_mounted){
    jbod_error = JBOD_ALREADY_MOUNTED;
    return -1;
  }

  uint32_t opr = (JBOD_MOUNT << 12);
  if(!(jbod_operation(opr, NULL))){
    is_mounted = 1; //Set Flag to 1 --> Mounted
    return 1; //Successfully mounted
  }


  //Return -1 for failure and set error if appropriate error is caught
  else {
    switch(jbod_error){
      case JBOD_NO_ERROR:
       //No error returned from jbod_operation, but something went wrong
       return -1;

       case JBOD_BAD_CMD:
       //Invalid command prompt sent to cmd
       return -1;

       case JBOD_UNMOUNTED:
       //jbod unmounted for some reason
       return -1;

      //Cases below are more suitable for mdadm_read but included to be on the safe side

       case JBOD_BAD_DISK_NUM:
       //Disk number specified not valid for jbod
       return -1;

       case JBOD_BAD_BLOCK_NUM:
       //Block number specified not valid for jbod
       return -1;

       default:
        //Unknown error encountered
        return -1;
    }
  }

  return 0;
}

int mdadm_unmount(void) {
  //Complete your code here
  if(!is_mounted){
    jbod_error = JBOD_ALREADY_UNMOUNTED;
    return -1;
  }

  uint32_t opr = (JBOD_UNMOUNT << 12);
  if(!(jbod_operation(opr, NULL))){
    is_mounted = 0; //Set Flag to 0 --> Unmounted
    return 1; //Successfully unmounted
  }


  //Return -1 for failure and set error if appropriate error is caught
  else {
    switch(jbod_error){
      case JBOD_NO_ERROR:
       //No error returned from jbod_operation, but something went wrong
       return -1;

       case JBOD_BAD_CMD:
       //Invalid command prompt sent to cmd
       return -1;

       //All error cases of mount not implemented as bad reads cannot occur while unmounting
       default:
        //Unknown error encountered
        return -1;
    }
  }
  return 0;
}


int CurrentDiskID = -1; //Set Default current disk to -1 to prevent accidental read of disk 0
int CurrentBlockID = 0; //Set Default current block to 0


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
  //Complete your code here
  uint32_t op;

  if(read_len > 1024){
    return -2; //read_len exceeded 1024 bytes
  }
  
  if(!is_mounted){
    return -3; //Failure as jbod is unmounted
  }

  if (start_addr < 0 || start_addr >= JBOD_NUM_DISKS * JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE) {
    return -1; // Out of valid address space
  }

  if(start_addr + read_len > JBOD_NUM_DISKS * JBOD_DISK_SIZE){
    return -1; //Out of Bounds
  }

  if(read_buf == NULL && read_len != 0){
    return -4; //Buffer error
  }


   //Create the counter of bytes read to start the read
   uint32_t bytes_read = 0;


   uint32_t current_addr = start_addr;

   //Begin the read process
   while(bytes_read < read_len){
    //First, we need to calculate which is our starting block and starting disk. We will also need an offset if we are at a partial read
    CurrentDiskID = current_addr/ JBOD_DISK_SIZE; //Get the current disk position
    CurrentBlockID = (current_addr % JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE; //Get the current block position int he disk
    uint32_t offset = (current_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE; //Have an offset incase we have a partial read

    //We need to amke sure the block and disks we found are valid
    int number_of_blocks = JBOD_NUM_BLOCKS_PER_DISK * JBOD_NUM_DISKS;
    if(CurrentBlockID >= number_of_blocks || CurrentDiskID >= JBOD_NUM_DISKS || CurrentDiskID < 0 || CurrentBlockID < 0)
      return -4; //-4 returned for additional restriction
    
    //Set current disk and block using seek
    op = (CurrentDiskID) | (JBOD_SEEK_TO_DISK << 12); //Use bitwise or to seek the correct disk using first 4 bits
    if(jbod_operation(op, NULL) == -1){
      jbod_error = JBOD_BAD_DISK_NUM;
      return -4;
    }

    op = (CurrentBlockID << 4) | (JBOD_SEEK_TO_BLOCK << 12);
    if(jbod_operation(op,NULL) == -1){
      jbod_error = JBOD_BAD_BLOCK_NUM;
      return -4;
    }

    uint8_t buffer_box[JBOD_BLOCK_SIZE]; //Create the buffer
    op = (CurrentDiskID) | (JBOD_READ_BLOCK << 12); //Read from the correct disk

    if(jbod_operation(op, buffer_box)){
      jbod_error = JBOD_BAD_READ;
      return -1; //Read operation failed
    }

    //Copy data to the buffer and include the offset declared above
    uint32_t copied_bytes = JBOD_BLOCK_SIZE - offset;
    if(copied_bytes > (read_len - bytes_read)){
      copied_bytes = read_len - bytes_read;
    }

    //After setting size of copied_bytes, we copy the data into read_buf
    for(int i=0; i<copied_bytes; i++){
      read_buf[bytes_read + i] = buffer_box[offset + i];
    }

    //We get the number of bytes_read from the current block
    bytes_read += copied_bytes;
    current_addr += copied_bytes;
   }

   return bytes_read; //return the total number of bytes read

  return 0;
}

